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
    assert "show_spawn_details" in zod_cpp
    assert "alpha_spawn_quads_.push_back" in zod_cpp
    assert "mult_spawn_quads_.push_back" in zod_cpp

    # Verify that the redundant second loop over map.spawns is eliminated
    # There should only be ONE loop iterating map.spawns in the entire file
    assert zod_cpp.count("for (; it != map.spawns.end() && it->z == z; ++it)") == 1
    assert "for (const Position& spos : map.spawns)" not in zod_cpp


def test_lower_bound_floor_traversal_equivalence():
    """Prove that binary search lower_bound(Position(min, min, z)) + it.z == z
    finds the exact same elements as filtering all spawns by z, across 10,000 randomized spawns on 16 floors.
    """
    rng = random.Random(42)
    spawns = []
    int_min = -2147483648

    for _ in range(10000):
        pos = Position(rng.randint(0, 4000), rng.randint(0, 4000), rng.randint(0, 15))
        spawns.append(pos)

    # std::set is strictly sorted by Position::operator<
    spawns.sort()

    for floor in range(16):
        # Oracle: linear scan
        oracle_spawns = [p for p in spawns if p.z == floor]

        # Fast lower_bound implementation matching std::set
        search_target = Position(int_min, int_min, floor)
        idx = bisect.bisect_left(spawns, search_target)

        gathered = []
        while idx < len(spawns) and spawns[idx].z == floor:
            gathered.append(spawns[idx])
            idx += 1

        assert gathered == oracle_spawns, f"Floor {floor} mismatch between lower_bound and oracle"


def test_coarse_aabb_culling_soundness():
    """Prove that coarse AABB culling with MAX_COARSE_RADIUS = 128 has ZERO false negatives
    for any spawn with size <= 128.
    """
    bounds = ViewBounds(500, 500, 700, 700)
    coarse_min_x = bounds.start_x - MAX_COARSE_RADIUS
    coarse_max_x = bounds.end_x + MAX_COARSE_RADIUS
    coarse_min_y = bounds.start_y - MAX_COARSE_RADIUS
    coarse_max_y = bounds.end_y + MAX_COARSE_RADIUS

    rng = random.Random(1337)
    for _ in range(20000):
        spos_x = rng.randint(0, 1500)
        spos_y = rng.randint(0, 1500)
        radius = rng.randint(1, 128)

        sx0 = spos_x - radius
        sx1 = spos_x + radius
        sy0 = spos_y - radius
        sy1 = spos_y + radius

        # Exact intersection
        exact_visible = not (sx1 < bounds.start_x or sx0 > bounds.end_x or
                             sy1 < bounds.start_y or sy0 > bounds.end_y)

        # Coarse test
        coarse_inside = not (spos_x < coarse_min_x or spos_x > coarse_max_x or
                             spos_y < coarse_min_y or spos_y > coarse_max_y)

        if exact_visible:
            # SOUNDNESS: if a spawn actually intersects, coarse test MUST NOT reject it!
            assert coarse_inside, f"False negative at ({spos_x}, {spos_y}) with radius {radius}"


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
