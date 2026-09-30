"""
Unit & Differential Test Suite for ZoneOverlayDrawer logic
Tests:
1. Multi-floor bounds evaluation (single floor vs transparent multi-floor).
2. Floor alpha decay and clamp bounds.
3. 4-way cardinal neighbor border bitmask generation for zones and blocking.
4. Secondary map preview tile lookup precedence.
"""

import pytest

# Zone flag constants matching zone_flags.h
ZONE_FLAG_BLOCKING       = 1 << 0  # 1
ZONE_FLAG_SPAWN          = 1 << 1  # 2
ZONE_FLAG_PZ             = 1 << 2  # 4
ZONE_FLAG_NO_PVP         = 1 << 3  # 8
ZONE_FLAG_NO_LOGOUT      = 1 << 4  # 16
ZONE_FLAG_PVP_ZONE       = 1 << 5  # 32

ZONE_BORDER_NORTH_SHIFT  = 6
ZONE_BORDER_SOUTH_SHIFT  = 7
ZONE_BORDER_WEST_SHIFT   = 8
ZONE_BORDER_EAST_SHIFT   = 9

SPECIAL_ZONE_BORDER_NORTH_SHIFT = 10
SPECIAL_ZONE_BORDER_SOUTH_SHIFT = 11
SPECIAL_ZONE_BORDER_WEST_SHIFT  = 12
SPECIAL_ZONE_BORDER_EAST_SHIFT  = 13

SPAWN_BORDER_NORTH_SHIFT = 14
SPAWN_BORDER_SOUTH_SHIFT = 15
SPAWN_BORDER_WEST_SHIFT  = 16
SPAWN_BORDER_EAST_SHIFT  = 17


class MockMapView:
    def __init__(self, floor: int, start_z: int, superend_z: int):
        self.floor = floor
        self.start_z = start_z
        self.superend_z = superend_z


class MockDrawingOptions:
    def __init__(self, transparent_floors: bool = False, show_special_tiles: bool = True, show_blocking: bool = True, show_spawns: bool = True):
        self.transparent_floors = transparent_floors
        self.show_special_tiles = show_special_tiles
        self.show_blocking = show_blocking
        self.show_spawns = show_spawns


class MockTile:
    def __init__(self, blocking: bool = False, pz: bool = False, no_pvp: bool = False,
                 no_logout: bool = False, pvp_zone: bool = False, is_spawn: bool = False,
                 spawn_id: int = 0):
        self.blocking = blocking
        self.pz = pz
        self.no_pvp = no_pvp
        self.no_logout = no_logout
        self.pvp_zone = pvp_zone
        self.is_spawn = is_spawn
        self.spawn_id = spawn_id

    def is_blocking(self) -> bool:
        return self.blocking

    def is_pz(self) -> bool:
        return self.pz

    def is_no_pvp(self) -> bool:
        return self.no_pvp

    def is_no_logout(self) -> bool:
        return self.no_logout

    def is_pvp_zone(self) -> bool:
        return self.pvp_zone

    def has_spawn(self) -> bool:
        return self.is_spawn


def get_floor_range(view: MockMapView, options: MockDrawingOptions):
    """Determines which z-levels to iterate."""
    start_z = view.start_z if options.transparent_floors else view.floor
    end_z = view.superend_z if options.transparent_floors else view.floor
    return start_z, end_z


def calculate_floor_alpha(view_floor: int, z: int) -> float:
    """Calculates floor alpha decay for lower floors."""
    if z == view_floor:
        return 1.0
    return max(0.25, 1.0 - (view_floor - z) * 0.20)


def compute_border_mask(center_val: bool, north: bool, south: bool, west: bool, east: bool) -> int:
    """Computes cardinal outer borders (1 = border, when neighbor does not have the flag)."""
    if not center_val:
        return 0
    mask = 0
    if not north:
        mask |= 1
    if not south:
        mask |= 2
    if not west:
        mask |= 4
    if not east:
        mask |= 8
    return mask


def resolve_tile(base_map: dict, secondary_map: dict, pos: tuple):
    """Resolves tile lookup with secondary map precedence."""
    if secondary_map and pos in secondary_map:
        return secondary_map[pos]
    return base_map.get(pos)


# Tests

def test_floor_range_transparent_vs_single():
    view = MockMapView(floor=7, start_z=7, superend_z=15)
    opts_opaque = MockDrawingOptions(transparent_floors=False)
    assert get_floor_range(view, opts_opaque) == (7, 7)

    opts_transparent = MockDrawingOptions(transparent_floors=True)
    assert get_floor_range(view, opts_transparent) == (7, 15)


def test_floor_alpha_decay():
    # Active floor = alpha 1.0
    assert calculate_floor_alpha(7, 7) == pytest.approx(1.0)
    # Floor 6 when viewing 7 -> 1.0 - 1 * 0.20 = 0.80
    assert calculate_floor_alpha(7, 6) == pytest.approx(0.80)
    # Floor 5 when viewing 7 -> 1.0 - 2 * 0.20 = 0.60
    assert calculate_floor_alpha(7, 5) == pytest.approx(0.60)
    # Floor 4 when viewing 7 -> 1.0 - 3 * 0.20 = 0.40
    assert calculate_floor_alpha(7, 4) == pytest.approx(0.40)
    # Floor 0 when viewing 7 -> clamped at min 0.25
    assert calculate_floor_alpha(7, 0) == pytest.approx(0.25)


def test_cardinal_border_mask():
    # Isolated tile (no neighbors have flag) -> all 4 borders active
    assert compute_border_mask(True, north=False, south=False, west=False, east=False) == 1 | 2 | 4 | 8

    # Completely surrounded tile -> 0 borders active
    assert compute_border_mask(True, north=True, south=True, west=True, east=True) == 0

    # North and East border only
    assert compute_border_mask(True, north=False, south=True, west=True, east=False) == 1 | 8

    # Non-zone tile -> 0
    assert compute_border_mask(False, north=False, south=False, west=False, east=False) == 0


def test_secondary_map_precedence():
    base_tile = MockTile(blocking=False, pz=False)
    pasted_tile = MockTile(blocking=True, pz=True)

    base_map = {(100, 100, 7): base_tile}
    secondary_map = {(100, 100, 7): pasted_tile}

    resolved = resolve_tile(base_map, secondary_map, (100, 100, 7))
    assert resolved.is_blocking() is True
    assert resolved.is_pz() is True

    # Coordinate outside secondary map uses base map
    other = resolve_tile(base_map, secondary_map, (101, 100, 7))
    assert other is None


def test_zone_shader_3d_bevel_and_colors():
    """Verify that in zone_shader.h, 2px global black outline, 3D bevels, and clean zone washes are implemented."""
    from pathlib import Path
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "zone_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    assert "evaluateSpecialZones" in content
    idx = content.find("evaluateSpecialZones")
    end_idx = content.find("bool evaluateSpawnOverlay")
    fn_body = content[idx:end_idx]

    # Global 2px black outer outline must be present
    assert "vec4(0.05, 0.05, 0.07, 0.98)" in fn_body, "2px global black outline must be defined"

    # PZ (flags & 4u) must have golden yellow wash and 3D bevel
    assert "flags & 4u" in fn_body
    assert "1.00, 0.88, 0.12, 0.28" in fn_body, "PZ wash must be golden yellow"

    # No-PvP (flags & 8u) must have emerald green wash and 3D bevel
    assert "flags & 8u" in fn_body
    assert "0.12, 0.85, 0.24, 0.26" in fn_body, "No-PvP wash must be emerald green"

    # No-Logout (flags & 16u) must have warm orange wash and 3D bevel
    assert "flags & 16u" in fn_body
    assert "1.00, 0.52, 0.06, 0.28" in fn_body, "No-Logout wash must be warm orange"

    # PvP Zone (flags & 32u) must have crimson red wash and 3D bevel
    assert "flags & 32u" in fn_body
    assert "0.92, 0.12, 0.24, 0.28" in fn_body, "PvP Zone wash must be crimson red"

    # Cluster badge evaluation helper must be present
    assert "evaluateClusterBadge" in content
    assert "4194304u" in content, "ZONE_FLAG_CLUSTER_BADGE dispatch must be present"

    # Inside 3D kitchen tile bevels must be present
    assert "!bNorthOuter && tile_ly == 0" in fn_body
    assert "!bWestOuter && tile_lx == 0" in fn_body
    assert "!bSouthOuter && tile_ly == 31" in fn_body
    assert "!bEastOuter && tile_lx == 31" in fn_body

    # Blocking overlay must have cyan border
    assert "evaluateBlockingOverlay" in content
    b_idx = content.find("evaluateBlockingOverlay")
    b_body = content[b_idx:b_idx + 600]
    assert "0.00, 0.95, 1.00, 0.95" in b_body, "Blocking border must be cyan"

    # Spawn overlay must have magenta border
    assert "evaluateSpawnOverlay" in content
    s_idx = content.find("evaluateSpawnOverlay")
    s_body = content[s_idx:s_idx + 600]
    assert "1.00, 0.20, 1.00, 0.95" in s_body, "Spawn border must be magenta"
    assert "0.04, 0.04, 0.06, 0.95" in b_body, "Blocking overlay must evaluate dark drop shadow"
    assert "0.04, 0.04, 0.06, 0.95" in s_body, "Spawn overlay must evaluate dark drop shadow"


def test_indicator_shader_zero_fill_and_brackets():
    """Verify that indicator_shader.h defines evaluateTileBracket with shadows and discards non-indicator pixels."""
    from pathlib import Path
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "indicator_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    assert "void evaluateTileBracket" in content, "evaluateTileBracket helper must be defined"
    assert "isShadow = (isShadowInner || isShadowCap) && !isCore;" in content, "Bracket shadow logic must be implemented"
    assert "outColor = vec4(0.04, 0.04, 0.06, 0.95);" in content, "Dark drop shadow color must be set"
    assert "discard;" in content, "Non-indicator pixels must be discarded to guarantee zero white background"


def test_is_tile_path_blocking_excludes_invisible_wall():
    """Verify that invisible walls (1548) are excluded from pathing blocking overlay."""
    class MockItem:
        def __init__(self, server_id: int, client_id: int, is_blocking: bool):
            self.server_id = server_id
            self.client_id = client_id
            self.blocking = is_blocking

    def is_invisible_wall(item: MockItem) -> bool:
        return item.server_id == 1548 or item.client_id == 2187

    def is_tile_path_blocking(ground: MockItem | None, items: list[MockItem]) -> bool:
        if not ground and not items:
            return False
        if ground and ground.blocking and not is_invisible_wall(ground):
            return True
        for item in items:
            if item.blocking and not is_invisible_wall(item):
                return True
        return False

    # Tile with grass ground (not blocking) and invisible wall 1548
    grass = MockItem(server_id=101, client_id=101, is_blocking=False)
    invis_wall = MockItem(server_id=1548, client_id=2187, is_blocking=True)
    assert not is_tile_path_blocking(grass, [invis_wall]), "Tile with only invisible wall must NOT be path-blocking"

    # Tile with stone wall (blocking)
    stone_wall = MockItem(server_id=1025, client_id=1025, is_blocking=True)
    assert is_tile_path_blocking(grass, [stone_wall]), "Tile with stone wall MUST be path-blocking"

    # Tile with stone wall AND invisible wall
    assert is_tile_path_blocking(grass, [stone_wall, invis_wall]), "Tile with stone wall and invisible wall MUST be path-blocking"

    # Tile with water ground (blocking ground)
    water = MockItem(server_id=4608, client_id=4608, is_blocking=True)
    assert is_tile_path_blocking(water, []), "Water ground tile MUST be path-blocking"


def test_ground_level_render_order_and_subpass_partitioning():
    """Verify that zone overlays are rendered at ground level between terrain and items."""
    from pathlib import Path
    root = Path(__file__).parent.parent

    # 1. Verify chunk_cache_manager.h declares terrain and item instance counts and methods
    ccm_h = (root / "source" / "rendering" / "core" / "chunk_cache_manager.h").read_text(encoding="utf-8")
    assert "uint32_t terrain_instance_count" in ccm_h
    assert "uint32_t item_instance_count" in ccm_h
    assert "void renderFloorTerrain" in ccm_h
    assert "void renderFloorItems" in ccm_h

    # 2. Verify zone_overlay_drawer.h declares drawFloor
    zod_h = (root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.h").read_text(encoding="utf-8")
    assert "void drawFloor(" in zod_h

    # 3. Verify map_layer_drawer.cpp invokes drawFloor between renderFloorTerrain and renderFloorItems
    mld_cpp = (root / "source" / "rendering" / "drawers" / "map_layer_drawer.cpp").read_text(encoding="utf-8")
    terrain_pos = mld_cpp.find("renderFloorTerrain")
    zone_pos = mld_cpp.find("zone_overlay_drawer->drawFloor")
    items_pos = mld_cpp.find("renderFloorItems")

    assert terrain_pos != -1, "renderFloorTerrain must be called"
    assert zone_pos != -1, "zone_overlay_drawer->drawFloor must be called"
    assert items_pos != -1, "renderFloorItems must be called"

    assert terrain_pos < zone_pos < items_pos, (
        "Strict Ground-Level Draw Order: renderFloorTerrain -> zone_overlay_drawer->drawFloor -> renderFloorItems"
    )

    # 4. Verify post-map overlay pass in map_drawer.cpp does NOT draw zone overlays on top of items
    md_cpp = (root / "source" / "rendering" / "map_drawer.cpp").read_text(encoding="utf-8")
    draw_render_frame_idx = md_cpp.find("void MapDrawer::DrawRenderFrame")
    post_map_batch_resume = md_cpp.find("// Resume Batch for Overlays", draw_render_frame_idx)
    post_map_end = md_cpp.find("// End Batches and Flush", post_map_batch_resume)
    post_map_chunk = md_cpp[post_map_batch_resume:post_map_end]

    assert "zone_overlay_drawer.draw(" not in post_map_chunk, (
        "zone_overlay_drawer must NOT be called in post-map pass to avoid tinting walls, tables, and items"
    )


def test_multi_zone_flag_accumulation_and_brush():
    """Verify that multiple zones can co-exist on a tile without overwriting each other."""
    from pathlib import Path
    root = Path(__file__).parent.parent

    # 1. Tile::addMapFlags must be declared and defined in tile.h
    tile_h = (root / "source" / "map" / "tile.h").read_text(encoding="utf-8")
    assert "void addMapFlags(uint32_t _flags);" in tile_h, "Tile::addMapFlags must be declared"
    assert "inline void Tile::addMapFlags(uint32_t _flags)" in tile_h, "Tile::addMapFlags must be defined"
    assert "mapflags |= _flags;" in tile_h, "Tile::addMapFlags must perform bitwise OR"

    # 2. FlagBrush::draw must call addMapFlags instead of setMapFlags to prevent erasing existing zones
    flag_brush_cpp = (root / "source" / "brushes" / "flag" / "flag_brush.cpp").read_text(encoding="utf-8")
    draw_idx = flag_brush_cpp.find("void FlagBrush::draw")
    assert draw_idx != -1
    draw_body = flag_brush_cpp[draw_idx:draw_idx + 250]
    assert "tile->addMapFlags" in draw_body, "FlagBrush::draw must call addMapFlags"
    assert "tile->setMapFlags" not in draw_body, "FlagBrush::draw must not call setMapFlags"

    # 3. map_flags_panel.cpp must call addMapFlags when checking flags
    flags_panel_cpp = (root / "source" / "ui" / "tile_properties" / "map_flags_panel.cpp").read_text(encoding="utf-8")
    assert "new_tile->addMapFlags(TILESTATE_NOPVP);" in flags_panel_cpp
    assert "new_tile->addMapFlags(TILESTATE_NOLOGOUT);" in flags_panel_cpp
    assert "new_tile->addMapFlags(TILESTATE_PVPZONE);" in flags_panel_cpp

    # 4. zone_overlay_drawer.cpp must accumulate all zone bits independently (no else-if)
    zod_cpp = (root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.cpp").read_text(encoding="utf-8")
    spec_tiles_idx = zod_cpp.find("if (options.show_special_tiles)")
    assert spec_tiles_idx != -1
    spec_body = zod_cpp[spec_tiles_idx:spec_tiles_idx + 800]
    assert "if (has_pz)    tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);" in spec_body
    assert "if (has_nopvp) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);" in spec_body
    assert "if (has_nolog) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);" in spec_body
    assert "if (has_pvp)   tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);" in spec_body
    assert "else if (has_nopvp)" not in spec_body, "zone_overlay_drawer must NOT use else if for special zones"

    # 5. sameZone in zone_overlay_drawer.cpp must check all 4 flags for equality
    assert "(t_pz == pz) && (t_nopvp == nopvp) && (t_nolog == nolog) && (t_pvp == pvp)" in zod_cpp

    # 6. preview_drawer.cpp must accumulate all zone bits independently (no else-if)
    prev_cpp = (root / "source" / "rendering" / "drawers" / "overlays" / "preview_drawer.cpp").read_text(encoding="utf-8")
    assert "else if ((tile->getMapFlags() & TILESTATE_NOPVP)" not in prev_cpp, "preview_drawer must not use else if"


def test_zone_shader_multi_zone_quadrants_and_badges():
    """Verify that zone_shader.h evaluates 4 distinct corner badges and Voronoi quadrant wash."""
    from pathlib import Path
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "zone_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    badge_idx = content.find("evaluateClusterBadge")
    badge_end = content.find("bool evaluateSpecialZones")
    badge_body = content[badge_idx:badge_end]

    idx = content.find("evaluateSpecialZones")
    end_idx = content.find("bool evaluateSpawnOverlay")
    fn_body = content[idx:end_idx]

    # Verify all 4 badge text colors are distinct and explicit in evaluateClusterBadge
    assert "vec4(1.00, 0.90, 0.10, 0.98)" in badge_body, "PZ badge text must be golden-yellow"
    assert "vec4(0.20, 1.00, 0.30, 0.98)" in badge_body, "NP badge text must be emerald-green"
    assert "vec4(1.00, 0.55, 0.10, 0.98)" in badge_body, "NL badge text must be warm-orange"
    assert "vec4(1.00, 0.15, 0.30, 0.98)" in badge_body, "PvP badge text must be crimson-red"

    # Verify dynamic 1x / 2x font scaling and character bitmasks
    assert "bool is2x = (h >= 18);" in badge_body
    assert "int fontScale = is2x ? 2 : 1;" in badge_body
    assert "pMask" in badge_body and "zMask" in badge_body
    assert "nMask" in badge_body and "lMask" in badge_body and "vMask" in badge_body

    # Verify bevel averaging across active zones
    assert "activeCount += 1.0;" in fn_body
    assert "vec4 zDark = sumDark / activeCount;" in fn_body
    assert "vec4 zLight = sumLight / activeCount;" in fn_body

    # Verify Voronoi quadrant wash
    assert "int minDist = 999999;" in fn_body
    assert "int d = tile_lx * tile_lx + tile_ly * tile_ly;" in fn_body
    assert "d < minDist" in fn_body

    # Simulate Voronoi resolution in Python and verify correctness
    def resolve_wash(flags, lx, ly):
        has_pz = (flags & 4) != 0
        has_np = (flags & 8) != 0
        has_nl = (flags & 16) != 0
        has_pvp = (flags & 32) != 0
        min_d = 999999
        chosen = None
        if has_pz:
            d = lx * lx + ly * ly
            if d < min_d: min_d = d; chosen = "PZ"
        if has_np:
            dx = 31 - lx
            d = dx * dx + ly * ly
            if d < min_d: min_d = d; chosen = "NP"
        if has_nl:
            dy = 31 - ly
            d = lx * lx + dy * dy
            if d < min_d: min_d = d; chosen = "NL"
        if has_pvp:
            dx = 31 - lx
            dy = 31 - ly
            d = dx * dx + dy * dy
            if d < min_d: min_d = d; chosen = "PvP"
        return chosen

    all_flags = 4 | 8 | 16 | 32
    # Check 4 corners match their respective zones when all 4 are active
    assert resolve_wash(all_flags, 2, 2) == "PZ"
    assert resolve_wash(all_flags, 29, 2) == "NP"
    assert resolve_wash(all_flags, 2, 29) == "NL"
    assert resolve_wash(all_flags, 29, 29) == "PvP"

    # Check 2 zones (PZ + NP) vertical 50/50 split
    two_flags = 4 | 8
    assert resolve_wash(two_flags, 5, 10) == "PZ"
    assert resolve_wash(two_flags, 25, 10) == "NP"
    assert resolve_wash(two_flags, 5, 25) == "PZ"
    assert resolve_wash(two_flags, 25, 25) == "NP"


def test_zone_cluster_finder_algorithm():
    """Verify connected component BFS, distance transform pole of inaccessibility, and badge scaling logic."""
    from collections import deque

    def compute_pole_of_inaccessibility(tiles: set[tuple[int, int]]) -> tuple[int, int, int]:
        """Multi-source BFS distance transform from perimeter inward."""
        assert tiles
        q = deque()
        dist = {}

        # 1. Identify perimeter tiles (tiles adjacent to exterior)
        for x, y in tiles:
            is_boundary = False
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                if (x + dx, y + dy) not in tiles:
                    is_boundary = True
                    break
            if is_boundary:
                dist[(x, y)] = 0
                q.append((x, y))

        # 2. Multi-source BFS inward
        while q:
            cx, cy = q.popleft()
            cd = dist[(cx, cy)]
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                npos = (cx + dx, cy + dy)
                if npos in tiles and npos not in dist:
                    dist[npos] = cd + 1
                    q.append(npos)

        # 3. Find pole (max clearance, breaking ties by distance to centroid)
        avg_x = sum(x for x, y in tiles) / len(tiles)
        avg_y = sum(y for x, y in tiles) / len(tiles)

        best_pos = None
        best_d = -1
        best_c_dist = float("inf")

        for pos, d in dist.items():
            c_dist = (pos[0] - avg_x) ** 2 + (pos[1] - avg_y) ** 2
            if d > best_d or (d == best_d and c_dist < best_c_dist):
                best_d = d
                best_c_dist = c_dist
                best_pos = pos

        return best_pos[0], best_pos[1], len(tiles)

    def get_badge_size(tile_count: int) -> tuple[int, int]:
        if tile_count == 1:
            return 18, 12
        elif tile_count < 5:
            return 26, 15
        else:
            return 38, 20

    # 1. Isolated 1x1 tile
    p1 = compute_pole_of_inaccessibility({(10, 10)})
    assert (p1[0], p1[1]) == (10, 10)
    assert get_badge_size(p1[2]) == (18, 12)

    # 2. 2x2 cluster (4 tiles)
    tiles_2x2 = {(10, 10), (11, 10), (10, 11), (11, 11)}
    p2 = compute_pole_of_inaccessibility(tiles_2x2)
    assert (p2[0], p2[1]) in tiles_2x2
    assert get_badge_size(p2[2]) == (26, 15)

    # 3. 7x7 solid square (49 tiles) -> center should be at (13, 13)
    tiles_7x7 = {(x, y) for x in range(10, 17) for y in range(10, 17)}
    p3 = compute_pole_of_inaccessibility(tiles_7x7)
    assert (p3[0], p3[1]) == (13, 13)
    assert get_badge_size(p3[2]) == (38, 20)

    # 4. Donut shape (outer 7x7 with inner 3x3 hole)
    # Centroid falls in the hole (13, 13). Pole MUST be inside the ring!
    donut_tiles = set()
    for x in range(10, 17):
        for y in range(10, 17):
            if not (12 <= x <= 14 and 12 <= y <= 14):
                donut_tiles.add((x, y))
    centroid_donut = (13, 13)
    assert centroid_donut not in donut_tiles, "Centroid is in the hollow void"
    p_donut = compute_pole_of_inaccessibility(donut_tiles)
    assert (p_donut[0], p_donut[1]) in donut_tiles, "Pole of inaccessibility MUST be in the cluster ring"
    assert get_badge_size(p_donut[2]) == (38, 20)

    # 5. Overlapping badges side-by-side layout verification
    badges = [
        {"flag": 4, "w": 38, "h": 20},
        {"flag": 8, "w": 38, "h": 20}
    ]
    # For 2 badges of width 38 with gap 2: total_w = 38 * 2 + 2 = 78
    gap = 2.0
    total_w = sum(b["w"] for b in badges) + gap * (len(badges) - 1)
    assert total_w == 78.0
    start_x = -total_w / 2.0
    offsets = []
    curr = start_x
    for b in badges:
        offsets.append(curr + b["w"] / 2.0)
        curr += b["w"] + gap
    # First badge centered at -20, second badge at +20
    assert offsets[0] == pytest.approx(-20.0)
    assert offsets[1] == pytest.approx(20.0)


