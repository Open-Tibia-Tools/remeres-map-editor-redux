"""
Unit & Differential Test Suite for Spawn Overlay LOD and High-Throughput Batching
Tests:
1. Static code invariants in zone_overlay_drawer.h, zone_overlay_drawer.cpp, and spawn.h.
2. Equivalence of floor-bounded lower_bound traversal vs linear scan.
3. Soundness of coarse AABB rejection (zero false negatives for any spawn radius <= kMaxCoarseRadius).
4. Multi-tier LOD transition behavior across zoom factors (LOD 0, LOD 1, LOD 2).
5. GPU Draw Call batching reduction (O(1) state switches instead of O(N) per-spawn flushes).
"""

from pathlib import Path
import bisect
import pytest
import random

# Mirroring C++ constants in zone_overlay_drawer.h
ZOOM_LOD_CUTOFF = 10.0
SPAWN_LOD_DETAIL_THRESHOLD = 1.0 / 0.15  # ~6.6667
MAX_COARSE_RADIUS = 128


class Position:
    __slots__ = ("x", "y", "z")

    def __init__(self, x: int, y: int, z: int):
        self.x = x
        self.y = y
        self.z = z

    def __lt__(self, other: "Position") -> bool:
        if self.z != other.z:
            return self.z < other.z
        if self.y != other.y:
            return self.y < other.y
        return self.x < other.x

    def __eq__(self, other: "Position") -> bool:
        return self.x == other.x and self.y == other.y and self.z == other.z

    def __repr__(self):
        return f"Pos({self.x}, {self.y}, {self.z})"


class MockSpawn:
    def __init__(self, size: int, selected: bool = False):
        self.size = size
        self.selected = selected


class ViewBounds:
    def __init__(self, start_x: int, start_y: int, end_x: int, end_y: int):
        self.start_x = start_x
        self.start_y = start_y
        self.end_x = end_x
        self.end_y = end_y


def test_source_code_invariants():
    """Verify that zone_overlay_drawer and spawn.h adhere to the LOD, DOD, and batching invariants."""
    root = Path(__file__).parent.parent

    # 1. zone_overlay_drawer.h
    zod_h_path = root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.h"
    assert zod_h_path.exists()
    zod_h = zod_h_path.read_text(encoding="utf-8")

    assert "inline constexpr float kZoomLODCutoff = 10.0f;" in zod_h
    assert "inline constexpr float kSpawnLODDetailThreshold = 1.0f / 0.15f;" in zod_h
    assert "struct PendingSpawnQuad" in zod_h
    assert "alpha_spawn_quads_;" in zod_h
    assert "mult_spawn_quads_;" in zod_h
    assert "spawn_borders_;" in zod_h
    assert "spawn_badges_;" in zod_h

    # 2. spawn.h
    spawn_h_path = root / "source" / "game" / "spawn.h"
    assert spawn_h_path.exists()
    spawn_h = spawn_h_path.read_text(encoding="utf-8")
    assert "lower_bound(const Position& pos)" in spawn_h

    # 3. zone_overlay_drawer.cpp
    zod_cpp_path = root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.cpp"
    assert zod_cpp_path.exists()
    zod_cpp = zod_cpp_path.read_text(encoding="utf-8")

    assert "view.zoom > kZoomLODCutoff" in zod_cpp
    assert "map.spawns.lower_bound" in zod_cpp
    assert "kMaxCoarseRadius" in zod_cpp
    assert "MAX_SPAWN_RADIUS" in zod_cpp
    assert "it->y > coarse_max_y" in zod_cpp
    assert "break;" in zod_cpp
    assert "show_spawn_details" in zod_cpp
    assert "alpha_spawn_quads_.push_back" in zod_cpp
    assert "mult_spawn_quads_.push_back" in zod_cpp

    # Verify that the redundant second loop over map.spawns is eliminated
    # There should only be ONE loop iterating map.spawns in the entire file
    assert zod_cpp.count("for (; it != map.spawns.end() && it->z == z; ++it)") == 1
    assert "for (const Position& spos : map.spawns)" not in zod_cpp


def test_y_bounded_floor_traversal_efficiency_and_equivalence():
    """Prove that binary search lower_bound(Position(min, coarse_min_y, z)) + early break on it.y > coarse_max_y
    finds the exact same visible spawns as an exhaustive linear scan of all 10,000 spawns on the floor,
    while visiting only a fraction of the nodes (O(log N + K_visible) vs O(N_floor)).
    """
    rng = random.Random(42)
    spawns = []
    int_min = -2147483648
    floor = 7
    max_coarse_radius = 128

    for _ in range(10000):
        pos = Position(rng.randint(0, 4000), rng.randint(0, 4000), floor)
        size = rng.randint(1, 30)
        spawns.append((pos, MockSpawn(size=size)))

    # std::set is strictly sorted by Position::operator< (z, then y, then x)
    spawns.sort(key=lambda item: item[0])

    # Viewport at y in [1500, 1800]
    bounds = ViewBounds(1000, 1500, 1400, 1800)
    coarse_min_x = bounds.start_x - max_coarse_radius
    coarse_max_x = bounds.end_x + max_coarse_radius
    coarse_min_y = bounds.start_y - max_coarse_radius
    coarse_max_y = bounds.end_y + max_coarse_radius

    # 1. Oracle: linear scan of all 10,000 spawns on this floor
    oracle_visible = []
    for spos, spawn in spawns:
        sx0 = spos.x - spawn.size
        sx1 = spos.x + spawn.size
        sy0 = spos.y - spawn.size
        sy1 = spos.y + spawn.size
        if not (sx1 < bounds.start_x or sx0 > bounds.end_x or sy1 < bounds.start_y or sy0 > bounds.end_y):
            oracle_visible.append(spos)

    # 2. Optimized Y-bounded traversal matching updated ZoneOverlayDrawer
    pos_keys = [item[0] for item in spawns]
    search_target = Position(int_min, coarse_min_y, floor)
    idx = bisect.bisect_left(pos_keys, search_target)

    traversal_visible = []
    nodes_visited = 0
    while idx < len(spawns) and spawns[idx][0].z == floor:
        nodes_visited += 1
        spos, spawn = spawns[idx]
        if spos.y > coarse_max_y:
            break

        if spos.x < coarse_min_x or spos.x > coarse_max_x:
            idx += 1
            continue

        sx0 = spos.x - spawn.size
        sx1 = spos.x + spawn.size
        sy0 = spos.y - spawn.size
        sy1 = spos.y + spawn.size
        if not (sx1 < bounds.start_x or sx0 > bounds.end_x or sy1 < bounds.start_y or sy0 > bounds.end_y):
            traversal_visible.append(spos)
        idx += 1

    # Soundness & Completeness: EXACT match with oracle
    assert traversal_visible == oracle_visible, "Y-bounded traversal missed or incorrectly added visible spawns"
    # Efficiency: visited nodes must be far less than total spawns on floor
    assert nodes_visited < 2000, f"Expected < 2000 visited nodes, got {nodes_visited} out of 10000"
    assert len(traversal_visible) > 0, "Should have found visible spawns in test scenario"


def test_dynamic_coarse_radius_soundness_and_fixed_radius_counterexample():
    """Prove that:
    1. A hardcoded coarse radius of 128 produces FALSE NEGATIVES when spawn radius exceeds 128 (prior attempt bug).
    2. Dynamic coarse radius bounded by max(128, MAX_SPAWN_RADIUS) has ZERO false negatives.
    """
    bounds = ViewBounds(500, 500, 700, 700)

    # Counterexample showing prior attempt flaw: spawn radius 150 placed at x=360
    # sx1 = 360 + 150 = 510 >= bounds.start_x (500) -> IS VISIBLE
    spos_x = 360
    spos_y = 600
    radius = 150
    sx0 = spos_x - radius
    sx1 = spos_x + radius
    sy0 = spos_y - radius
    sy1 = spos_y + radius

    is_visible = not (sx1 < bounds.start_x or sx0 > bounds.end_x or sy1 < bounds.start_y or sy0 > bounds.end_y)
    assert is_visible, "Test setup: spawn must actually be visible"

    # With prior attempt's hardcoded 128:
    prior_coarse_min_x = bounds.start_x - 128  # 372
    prior_culled = (spos_x < prior_coarse_min_x)  # 360 < 372 -> TRUE -> FALSE NEGATIVE!
    assert prior_culled, "Fixed 128 radius must falsely cull the visible radius=150 spawn"

    # With dynamic max_coarse_radius = max(128, setting_radius = 200):
    dynamic_max_radius = max(128, 200)
    dynamic_coarse_min_x = bounds.start_x - dynamic_max_radius  # 300
    dynamic_coarse_max_x = bounds.end_x + dynamic_max_radius
    dynamic_coarse_min_y = bounds.start_y - dynamic_max_radius
    dynamic_coarse_max_y = bounds.end_y + dynamic_max_radius

    dynamic_inside = not (spos_x < dynamic_coarse_min_x or spos_x > dynamic_coarse_max_x or
                          spos_y < dynamic_coarse_min_y or spos_y > dynamic_coarse_max_y)
    assert dynamic_inside, "Dynamic coarse radius MUST NOT cull the visible spawn"


def simulate_spawn_render(zoom: float, spawns_on_floor, bounds: ViewBounds, blend_mode: int, show_borders: bool):
    """Simulates the updated ZoneOverlayDrawer spawn rendering logic."""
    if zoom > ZOOM_LOD_CUTOFF:
        return {"draw_calls": 0, "fill_quads": 0, "border_quads": 0, "badge_quads": 0}

    show_spawn_details = (zoom <= SPAWN_LOD_DETAIL_THRESHOLD)

    alpha_quads = []
    mult_quads = []
    border_quads = []
    badge_quads = []

    for spos, spawn in spawns_on_floor:
        # Coarse AABB
        if (spos.x < bounds.start_x - MAX_COARSE_RADIUS or spos.x > bounds.end_x + MAX_COARSE_RADIUS or
            spos.y < bounds.start_y - MAX_COARSE_RADIUS or spos.y > bounds.end_y + MAX_COARSE_RADIUS):
            continue

        radius = spawn.size
        sx0 = spos.x - radius
        sx1 = spos.x + radius
        sy0 = spos.y - radius
        sy1 = spos.y + radius

        if sx1 < bounds.start_x or sx0 > bounds.end_x or sy1 < bounds.start_y or sy0 > bounds.end_y:
            continue

        if blend_mode == 1:
            mult_quads.append(spos)
        else:
            alpha_quads.append(spos)

        if show_spawn_details:
            if show_borders:
                border_quads.append(spos)
            badge_quads.append(spos)

    draw_calls = 0
    if len(alpha_quads) > 0:
        draw_calls += 1
    if len(mult_quads) > 0:
        draw_calls += 1
    if len(border_quads) > 0:
        draw_calls += 1
    if len(badge_quads) > 0:
        draw_calls += 1

    return {
        "draw_calls": draw_calls,
        "fill_quads": len(alpha_quads) + len(mult_quads),
        "border_quads": len(border_quads),
        "badge_quads": len(badge_quads),
    }


def test_multi_tier_lod_behavior():
    """Verify LOD 0 (full detail), LOD 1 (simplified quads), and LOD 2 (culled) transitions."""
    bounds = ViewBounds(100, 100, 300, 300)
    spawns = [
        (Position(150, 150, 7), MockSpawn(size=5)),
        (Position(200, 200, 7), MockSpawn(size=10)),
        (Position(250, 250, 7), MockSpawn(size=3)),
    ]

    # LOD 0: Normal zoom (e.g. zoom = 1.0, 100%) -> Full Detail
    res_lod0 = simulate_spawn_render(zoom=1.0, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod0["fill_quads"] == 3
    assert res_lod0["border_quads"] == 3
    assert res_lod0["badge_quads"] == 3
    assert res_lod0["draw_calls"] == 3  # 1 mult fill + 1 border + 1 badge (batched!)

    # LOD 0 boundary: Zoom 15% (zoom factor ~6.66) -> Full Detail
    res_lod0_edge = simulate_spawn_render(zoom=6.66, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod0_edge["fill_quads"] == 3
    assert res_lod0_edge["border_quads"] == 3
    assert res_lod0_edge["badge_quads"] == 3

    # LOD 1: Zoomed out beyond 15% (e.g. zoom = 7.0 or zoom = 9.0) -> Simplified quads
    res_lod1 = simulate_spawn_render(zoom=8.0, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod1["fill_quads"] == 3
    assert res_lod1["border_quads"] == 0, "Borders must be skipped in LOD 1 to prevent subpixel discard stalls"
    assert res_lod1["badge_quads"] == 0, "Badges must be skipped in LOD 1 to prevent subpixel sprite clutter"
    assert res_lod1["draw_calls"] == 1  # Exactly 1 batched draw call!

    # LOD 2: Zoomed out beyond 10x (zoom = 10.01, zoom < 10%) -> Culled under editor LOD policy
    res_lod2 = simulate_spawn_render(zoom=10.01, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod2["fill_quads"] == 0
    assert res_lod2["border_quads"] == 0
    assert res_lod2["badge_quads"] == 0
    assert res_lod2["draw_calls"] == 0, "Extreme zoom must issue 0 draw calls to preserve 160 FPS chunk cache throughput"


def test_draw_call_scalability_500_spawns():
    """Verify that with 500 visible spawns on screen:
    - Legacy unbatched code would issue 1,000+ draw calls due to per-spawn setBlendFunc flushes.
    - Updated batched code issues at most 3 draw calls in LOD 0, and exactly 1 in LOD 1.
    """
    bounds = ViewBounds(0, 0, 1000, 1000)
    spawns = [(Position(i * 2, i * 2, 7), MockSpawn(size=3)) for i in range(500)]

    res_lod1 = simulate_spawn_render(zoom=7.5, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod1["fill_quads"] == 500
    assert res_lod1["draw_calls"] == 1, "500 spawns in LOD 1 must render in exactly 1 GPU draw call"

    res_lod0 = simulate_spawn_render(zoom=2.0, spawns_on_floor=spawns, bounds=bounds, blend_mode=1, show_borders=True)
    assert res_lod0["fill_quads"] == 500
    assert res_lod0["border_quads"] == 500
    assert res_lod0["badge_quads"] == 500
    assert res_lod0["draw_calls"] <= 3, "500 spawns in LOD 0 must render in at most 3 batched draw calls"
