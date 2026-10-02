"""
Unit & Differential Test Suite for ZoneOverlayDrawer logic
Tests:
1. Multi-floor bounds evaluation (single floor vs transparent multi-floor).
2. Floor alpha decay and clamp bounds.
3. 4-way cardinal neighbor border bitmask generation for zones and blocking.
4. Secondary map preview tile lookup precedence.
"""

import pytest
from pathlib import Path

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
    """Verify that in zone_shader.h, dynamic uniforms for borders and washes are declared and used without inside lines, with calibrated defaults in settings.cpp."""
    from pathlib import Path
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "zone_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    # Uniform declarations must be present
    assert "uniform int uShowZoneBorders;" in content
    assert "uniform vec4 uZoneBorderColor;" in content
    assert "uniform vec4 uPzWash;" in content
    assert "uniform vec4 uNpWash;" in content
    assert "uniform vec4 uNlWash;" in content
    assert "uniform vec4 uPvpWash;" in content
    assert "uniform vec4 uBlockingWash;" in content
    assert "uniform vec4 uSpawnWash;" in content

    assert "evaluateSpecialZones" in content
    idx = content.find("evaluateSpecialZones")
    end_idx = content.find("bool evaluateSpawnOverlay")
    fn_body = content[idx:end_idx]

    # Global outer outline guarded by uShowZoneBorders and uses uZoneBorderColor
    assert "uShowZoneBorders != 0" in fn_body
    assert "outLayer = uZoneBorderColor;" in fn_body

    # Washes must use dynamic uniforms
    assert "flags & 4u" in fn_body
    assert "activeWashes[count++] = uPzWash;" in fn_body

    assert "flags & 8u" in fn_body
    assert "activeWashes[count++] = uNpWash;" in fn_body

    assert "flags & 16u" in fn_body
    assert "activeWashes[count++] = uNlWash;" in fn_body

    assert "flags & 32u" in fn_body
    assert "activeWashes[count++] = uPvpWash;" in fn_body

    # Cluster badge evaluation helper must be present
    assert "evaluateClusterBadge" in content
    assert "4194304u" in content, "ZONE_FLAG_CLUSTER_BADGE dispatch must be present"

    # Inside 3D kitchen tile bevels/lines must NOT be present
    assert "!bNorthOuter && tile_ly == 0" not in fn_body
    assert "!bWestOuter && tile_lx == 0" not in fn_body
    assert "!bSouthOuter && tile_ly == 31" not in fn_body
    assert "!bEastOuter && tile_lx == 31" not in fn_body

    # Blocking overlay must use uShowZoneBorders and uBlockingWash
    assert "evaluateBlockingOverlay" in content
    b_idx = content.find("evaluateBlockingOverlay")
    b_end = content.find("bool evaluateZoneOverlay", b_idx)
    b_body = content[b_idx:b_end]
    assert "uShowZoneBorders != 0" in b_body
    assert "outLayer = uBlockingWash;" in b_body

    # Spawn overlay must use uShowZoneBorders and uSpawnWash
    assert "evaluateSpawnOverlay" in content
    s_idx = content.find("evaluateSpawnOverlay")
    s_end = content.find("bool evaluateBlockingOverlay", s_idx)
    s_body = content[s_idx:s_end]
    assert "uShowZoneBorders != 0" in s_body
    assert "outLayer = uSpawnWash;" in s_body

    # Verify default calibrated settings exist in settings.cpp
    settings_path = Path(__file__).parent.parent / "source" / "app" / "settings.cpp"
    settings_content = settings_path.read_text(encoding="utf-8")
    assert "ZONE_BORDERS_ENABLED, true" in settings_content
    assert "ZONE_MULTIPLICATIVE_BLENDING, false" in settings_content
    assert "ZONE_BORDER_COLOR_R, 13" in settings_content
    assert "ZONE_PZ_COLOR_R, 20" in settings_content
    assert "ZONE_NOPVP_COLOR_R, 0" in settings_content
    assert "ZONE_NOLOGOUT_COLOR_R, 255" in settings_content
    assert "ZONE_PVP_COLOR_R, 245" in settings_content
    assert "ZONE_BLOCKING_COLOR_R, 0" in settings_content
    assert "ZONE_BLOCKING_COLOR_G, 0" in settings_content
    assert "ZONE_BLOCKING_COLOR_B, 0" in settings_content
    assert "ZONE_BLOCKING_COLOR_A, 128" in settings_content
    assert "ZONE_SPAWN_COLOR_R, 242" in settings_content


def test_indicator_shader_standardized_system():
    """Verify that indicator_shader.h implements 2px black border, 3D color bevel, kitchen tile wash, and bold centered badge."""
    from pathlib import Path
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "indicator_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    assert "lx <= 1 || lx >= 30 || ly <= 1 || ly >= 30" in content, "2px black border must be implemented"
    assert "vec4(0.05, 0.05, 0.07, 0.98)" in content, "Black border color must be defined"
    assert "lx >= 2 && lx <= 29 && ly >= 10 && ly <= 20" in content, "Centered bold pill badge must be evaluated"
    assert "zWash" in content, "Interior translucent wash must be evaluated"


def test_is_tile_path_blocking_excludes_invisible_wall():
    """Verify that invisible walls (1548) are excluded from pathing blocking overlay and ground is required."""
    class MockItem:
        def __init__(self, server_id: int, client_id: int, is_blocking: bool):
            self.server_id = server_id
            self.client_id = client_id
            self.blocking = is_blocking

    def is_invisible_wall(item: MockItem) -> bool:
        return item.server_id == 1548 or item.client_id == 2187

    def is_tile_path_blocking(ground: MockItem | None, items: list[MockItem]) -> bool:
        if not ground:
            return False
        if ground.blocking and not is_invisible_wall(ground):
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

    # Tile without ground (void / open air) must NOT be path-blocking (ground-only)
    assert not is_tile_path_blocking(None, [stone_wall]), "Tile without ground must NOT be path-blocking"


def test_ground_level_render_order_and_subpass_partitioning():
    """Verify that zone and blocking overlays are rendered at ground level between terrain and items."""
    from pathlib import Path
    root = Path(__file__).parent.parent

    # 1. Verify chunk_cache_manager.h declares terrain and item instance counts and methods
    ccm_h = (root / "source" / "rendering" / "core" / "chunk_cache_manager.h").read_text(encoding="utf-8")
    assert "uint32_t terrain_instance_count" in ccm_h
    assert "uint32_t item_instance_count" in ccm_h
    assert "void renderFloorTerrain" in ccm_h
    assert "void renderFloorItems" in ccm_h

    # 2. Verify zone_overlay_drawer.h declares drawFloor, drawFloorBlocking, and drawFloorBadges
    zod_h = (root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.h").read_text(encoding="utf-8")
    assert "void drawFloor(" in zod_h
    assert "void drawFloorBlocking(" in zod_h
    assert "void drawFloorBadges(" in zod_h

    # 3. Verify map_layer_drawer.cpp invokes drawFloor and drawFloorBlocking at ground level between renderFloorTerrain and renderFloorItems
    mld_cpp = (root / "source" / "rendering" / "drawers" / "map_layer_drawer.cpp").read_text(encoding="utf-8")
    terrain_pos = mld_cpp.find("renderFloorTerrain")
    zone_pos = mld_cpp.find("zone_overlay_drawer->drawFloor")
    blocking_pos = mld_cpp.find("zone_overlay_drawer->drawFloorBlocking")
    items_pos = mld_cpp.find("renderFloorItems")
    badges_pos = mld_cpp.find("zone_overlay_drawer->drawFloorBadges")

    assert terrain_pos != -1, "renderFloorTerrain must be called"
    assert zone_pos != -1, "zone_overlay_drawer->drawFloor must be called"
    assert blocking_pos != -1, "zone_overlay_drawer->drawFloorBlocking must be called"
    assert items_pos != -1, "renderFloorItems must be called"
    assert badges_pos != -1, "zone_overlay_drawer->drawFloorBadges must be called"

    assert terrain_pos < zone_pos <= blocking_pos < items_pos < badges_pos, (
        "Strict Order: renderFloorTerrain -> drawFloor -> drawFloorBlocking -> renderFloorItems -> drawFloorBadges"
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
    assert "if (ct.is_pz)    tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);" in zod_cpp
    assert "if (ct.is_nopvp) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);" in zod_cpp
    assert "if (ct.is_nolog) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);" in zod_cpp
    assert "if (ct.is_pvp)   tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);" in zod_cpp
    assert "else if (ct.is_nopvp)" not in zod_cpp, "zone_overlay_drawer must NOT use else if for special zones"

    # 5. Neighbor check in zone_overlay_drawer.cpp must check all 4 flags
    assert "n.is_pz != ct.is_pz || n.is_nopvp != ct.is_nopvp ||" in zod_cpp

    # 6. preview_drawer.cpp must accumulate all zone bits independently (no else-if)
    prev_cpp = (root / "source" / "rendering" / "drawers" / "overlays" / "preview_drawer.cpp").read_text(encoding="utf-8")
    assert "else if ((tile->getMapFlags() & TILESTATE_NOPVP)" not in prev_cpp, "preview_drawer must not use else if"


def test_zone_shader_multi_zone_quadrants_and_badges():
    """Verify that zone_shader.h evaluates 4 distinct corner badges and multiplicative tint compounding."""
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
    assert "vec4(0.18, 0.55, 1.00, 0.98)" in badge_body, "PZ badge text must be cool azure"
    assert "vec4(0.12, 0.85, 0.48, 0.98)" in badge_body, "NP badge text must be cool mint green"
    assert "vec4(1.00, 0.52, 0.06, 0.98)" in badge_body, "NL badge text must be warm orange"
    assert "vec4(0.92, 0.12, 0.24, 0.98)" in badge_body, "PvP badge text must be warm red"

    # Verify high-resolution font and character bitmasks
    assert "charH = 10" in badge_body
    assert "pMask" in badge_body and "zMask" in badge_body
    assert "nMask" in badge_body and "lMask" in badge_body and "vMask" in badge_body

    # Verify diagonal halves and triangle partitioning for overlapping zones
    assert "tile_lx + tile_ly < 31" in fn_body
    assert "activeWashes[0]" in fn_body
    assert "activeWashes[1]" in fn_body

    # Simulate diagonal triangle partitioning in Python and verify correctness
    def resolve_triangle_zone(flags, lx, ly):
        pz = (0.08, 0.46, 1.00, 0.48)
        np = (0.00, 0.86, 0.36, 0.46)
        nl = (1.00, 0.48, 0.00, 0.48)
        pvp = (0.96, 0.10, 0.20, 0.48)
        active = []
        if flags & 4:  active.append(pz)
        if flags & 8:  active.append(np)
        if flags & 16: active.append(nl)
        if flags & 32: active.append(pvp)
        if not active:
            return (0.0, 0.0, 0.0, 0.0)
        n = len(active)
        if n == 1:
            return active[0]
        if n == 2:
            return active[0] if (lx + ly < 31) else active[1]
        if n == 3:
            if lx + ly < 31:
                return active[0]
            return active[1] if (lx >= ly) else active[2]
        # n == 4
        diag1 = (ly <= lx)
        diag2 = (lx + ly <= 31)
        if diag1 and diag2:
            return active[0]
        if diag1 and not diag2:
            return active[1]
        if not diag1 and not diag2:
            return active[2]
        return active[3]

    # 2 zones (PZ + No Logout): Top-Left triangle is PZ, Bottom-Right triangle is NL
    assert resolve_triangle_zone(4 | 16, 2, 2) == (0.08, 0.46, 1.00, 0.48)
    assert resolve_triangle_zone(4 | 16, 29, 29) == (1.00, 0.48, 0.00, 0.48)

    # 2 zones (PZ + Non-PvP): Top-Left triangle is PZ, Bottom-Right triangle is NP
    assert resolve_triangle_zone(4 | 8, 2, 2) == (0.08, 0.46, 1.00, 0.48)
    assert resolve_triangle_zone(4 | 8, 29, 29) == (0.00, 0.86, 0.36, 0.46)

    # 2 zones (PZ + PvP): Top-Left triangle is PZ, Bottom-Right triangle is PvP
    assert resolve_triangle_zone(4 | 32, 2, 2) == (0.08, 0.46, 1.00, 0.48)
    assert resolve_triangle_zone(4 | 32, 29, 29) == (0.96, 0.10, 0.20, 0.48)

    # Single zone tile (only PZ) everywhere evaluates to PZ
    assert resolve_triangle_zone(4, 29, 29) == (0.08, 0.46, 1.00, 0.48)


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

    def get_badge_size(tile_count: int, c_w: int = 10, c_h: int = 10) -> tuple[float, float]:
        if tile_count == 1:
            bw, bh = 18.0, 12.0
        elif tile_count <= 4:
            bw, bh = 26.0, 15.0
        elif tile_count <= 12:
            bw, bh = 38.0, 20.0
        elif tile_count <= 25:
            bw, bh = 52.0, 26.0
        elif tile_count <= 50:
            bw, bh = 66.0, 32.0
        elif tile_count <= 100:
            bw, bh = 82.0, 38.0
        else:
            bw, bh = 98.0, 44.0
        max_bw = max(18.0, float(c_w * 32 - 4))
        max_bh = max(12.0, float(c_h * 32 - 4))
        return min(bw, max_bw), min(bh, max_bh)

    # 1. Isolated 1x1 tile
    p1 = compute_pole_of_inaccessibility({(10, 10)})
    assert (p1[0], p1[1]) == (10, 10)
    assert get_badge_size(p1[2]) == (18.0, 12.0)

    # 2. 2x2 cluster (4 tiles)
    tiles_2x2 = {(10, 10), (11, 10), (10, 11), (11, 11)}
    p2 = compute_pole_of_inaccessibility(tiles_2x2)
    assert (p2[0], p2[1]) in tiles_2x2
    assert get_badge_size(p2[2]) == (26.0, 15.0)

    # 3. 7x7 solid square (49 tiles) -> center should be at (13, 13)
    tiles_7x7 = {(x, y) for x in range(10, 17) for y in range(10, 17)}
    p3 = compute_pole_of_inaccessibility(tiles_7x7)
    assert (p3[0], p3[1]) == (13, 13)
    assert get_badge_size(p3[2]) == (66.0, 32.0)

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
    assert get_badge_size(p_donut[2]) == (66.0, 32.0)

    # 5. Overlapping badges side-by-side layout verification
    badges = [
        {"flag": 4, "w": 38.0, "h": 20.0},
        {"flag": 8, "w": 38.0, "h": 20.0}
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

    # 6. Non-touching zones of the same type (e.g. Room A with 30 tiles and Room B with 20 tiles)
    # Both must be detected as distinct clusters and each receive its own badge!
    room_a = {(x, y) for x in range(10, 16) for y in range(10, 15)}  # 6x5 = 30 tiles
    room_b = {(x, y) for x in range(30, 35) for y in range(10, 14)}  # 5x4 = 20 tiles
    all_tiles = room_a | room_b

    clusters = []
    visited = set()
    for t in all_tiles:
        if t in visited:
            continue
        comp = set()
        q = [t]
        visited.add(t)
        while q:
            cur = q.pop(0)
            comp.add(cur)
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                n = (cur[0] + dx, cur[1] + dy)
                if n in all_tiles and n not in visited:
                    visited.add(n)
                    q.append(n)
        clusters.append(comp)

    assert len(clusters) == 2, "Must discover exactly 2 separate clusters for non-touching zones"
    assert {len(c) for c in clusters} == {30, 20}

    pa = compute_pole_of_inaccessibility(room_a)
    pb = compute_pole_of_inaccessibility(room_b)
    assert pa[0] in range(10, 16) and pa[1] in range(10, 15)
    assert pb[0] in range(30, 35) and pb[1] in range(10, 14)
    # Room A (30 tiles) has size 66x32, Room B (20 tiles) has size 52x26
    assert get_badge_size(30) == (66.0, 32.0)
    assert get_badge_size(20) == (52.0, 26.0)


def test_large_tibia_world_coordinates_zone_badges():
    """
    Verifies that map coordinates >= 32768 (standard Tibia coordinates for Ankrahmun,
    Darashia, Port Hope, Edron, Venore, etc.) do NOT overflow 16-bit integers and
    properly discover and emit zone cluster badges.
    """
    from pathlib import Path
    root = Path(__file__).resolve().parent.parent

    # 1. Verify VisibleZoneTile in C++ header uses 32-bit int, not int16_t
    finder_h = (root / "source" / "rendering" / "indicators" / "zone_cluster_finder.h").read_text(encoding="utf-8")
    assert "int x = 0;\n\tint y = 0;" in finder_h or "int x = 0;\r\n\tint y = 0;" in finder_h, \
           "VisibleZoneTile must use 32-bit int to prevent overflow above 32767"

    # 2. Verify drawer does not cast to int16_t
    drawer_cpp = (root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.cpp").read_text(encoding="utf-8")
    assert "static_cast<int16_t>(x)" not in drawer_cpp, "Must not cast x to int16_t"
    assert "static_cast<int16_t>(y)" not in drawer_cpp, "Must not cast y to int16_t"

    # 3. Simulate a 10x10 Protection Zone cluster at Ankrahmun coordinates (33100, 32800)
    cluster_x0, cluster_y0 = 33100, 32800
    cluster_tiles = set()
    for dx in range(10):
        for dy in range(10):
            cluster_tiles.add((cluster_x0 + dx, cluster_y0 + dy))

    bounds_start_x, bounds_end_x = 33080, 33130
    bounds_start_y, bounds_end_y = 32780, 32830
    grid_w = bounds_end_x - bounds_start_x + 1
    grid_h = bounds_end_y - bounds_start_y + 1

    tile_grid = [0] * (grid_w * grid_h)
    for tx, ty in cluster_tiles:
        lx = tx - bounds_start_x
        ly = ty - bounds_start_y
        assert 0 <= lx < grid_w and 0 <= ly < grid_h, f"Coord ({tx}, {ty}) must map within viewport grid"
        tile_grid[ly * grid_w + lx] = 1

    visited = [0] * (grid_w * grid_h)
    found_clusters = []
    for tx, ty in cluster_tiles:
        start_lx = tx - bounds_start_x
        start_ly = ty - bounds_start_y
        idx = start_ly * grid_w + start_lx
        if visited[idx] != 0:
            continue
        comp = []
        q = [(tx, ty)]
        visited[idx] = 1
        while q:
            cx, cy = q.pop(0)
            comp.append((cx, cy))
            for ddx, ddy in [(0, -1), (0, 1), (-1, 0), (1, 0)]:
                nx, ny = cx + ddx, cy + ddy
                nlx = nx - bounds_start_x
                nly = ny - bounds_start_y
                if 0 <= nlx < grid_w and 0 <= nly < grid_h:
                    nidx = nly * grid_w + nlx
                    if tile_grid[nidx] != 0 and visited[nidx] == 0:
                        visited[nidx] = 1
                        q.append((nx, ny))
        found_clusters.append(comp)

    assert len(found_clusters) == 1, "Must discover exactly 1 cluster for the 10x10 PZ zone"
    assert len(found_clusters[0]) == 100, "Cluster must have all 100 tiles"


def test_customizable_shader_overlays_architecture():
    """Verify end-to-end architecture: settings keys, drawing options, shader uniforms, and preferences page."""
    from pathlib import Path
    root = Path(__file__).parent.parent

    # 1. House shader uniforms
    house_shader = (root / "source" / "rendering" / "shaders" / "house_shader.h").read_text(encoding="utf-8")
    assert "uniform vec4 uHouseActiveWash;" in house_shader
    assert "uniform vec4 uHouseInactiveWash;" in house_shader
    assert "vec4 zWash = isActive ? uHouseActiveWash : uHouseInactiveWash;" in house_shader

    # 2. SpriteBatch shader plumbing
    sb_shader = (root / "source" / "rendering" / "shaders" / "sprite_batch_shader.h").read_text(encoding="utf-8")
    assert "shader.SetInt(\"uShowZoneBorders\", show_zone_borders ? 1 : 0);" in sb_shader
    assert "shader.SetVec4(\"uZoneBorderColor\", zone_border_color);" in sb_shader
    assert "shader.SetVec4(\"uPzWash\", zone_pz_color);" in sb_shader
    assert "shader.SetVec4(\"uHouseActiveWash\", house_active_color);" in sb_shader
    assert "shader.SetVec4(\"uHouseInactiveWash\", house_inactive_color);" in sb_shader

    # 3. Chunk Cache Manager plumbing for houses
    ccm = (root / "source" / "rendering" / "core" / "chunk_cache_manager.cpp").read_text(encoding="utf-8")
    assert "shader_.SetVec4(\"uHouseActiveWash\", ctx.options.house_active_color);" in ccm
    assert "shader_.SetVec4(\"uHouseInactiveWash\", ctx.options.house_inactive_color);" in ccm

    # 4. DrawingOptions struct members
    drawing_opts = (root / "source" / "rendering" / "core" / "drawing_options.h").read_text(encoding="utf-8")
    assert "bool show_zone_borders" in drawing_opts
    assert "bool zone_multiplicative_blending" in drawing_opts
    assert "glm::vec4 zone_border_color;" in drawing_opts
    assert "glm::vec4 zone_pz_color;" in drawing_opts
    assert "glm::vec4 zone_blocking_color;" in drawing_opts
    assert "glm::vec4 zone_spawn_color;" in drawing_opts
    assert "glm::vec4 house_active_color;" in drawing_opts
    assert "glm::vec4 house_inactive_color;" in drawing_opts

    # 5. Settings keys
    settings_h = (root / "source" / "app" / "settings.h").read_text(encoding="utf-8")
    assert "ZONE_BORDERS_ENABLED," in settings_h
    assert "ZONE_MULTIPLICATIVE_BLENDING," in settings_h
    assert "ZONE_BORDER_COLOR_R," in settings_h
    assert "ZONE_PZ_COLOR_R," in settings_h
    assert "ZONE_BLOCKING_COLOR_R," in settings_h
    assert "HOUSE_ACTIVE_COLOR_R," in settings_h

    # 6. Preferences GraphicsPage controls
    graphics_page_h = (root / "source" / "app" / "preferences" / "graphics_page.h").read_text(encoding="utf-8")
    assert "wxCheckBox* zone_borders_enabled_chkbox" in graphics_page_h
    assert "wxCheckBox* zone_multiplicative_chkbox" in graphics_page_h
    assert "wxColourPickerCtrl* zone_border_color_pick" in graphics_page_h
    assert "wxColourPickerCtrl* zone_pz_color_pick" in graphics_page_h
    assert "wxSpinCtrl* zone_pz_opacity_spin" in graphics_page_h
    assert "wxButton* reset_zone_defaults_btn" in graphics_page_h


def test_zone_multiplicative_blending_system():
    """Verify that multiplicative color blending is implemented end-to-end:
    - Shader uniform uZoneBlendMode and fragment modulation
    - SetSpriteBatchOverlayUniforms forwarding
    - ZoneOverlayDrawer blend mode switching and restoring
    - GraphicsPage wiring and default resetting
    """
    root = Path(__file__).parent.parent

    # 1. Shader uniform and fragment logic
    shader_h = (root / "source" / "rendering" / "shaders" / "sprite_batch_shader.h").read_text(encoding="utf-8")
    assert "uniform int uZoneBlendMode;" in shader_h
    assert "shader.SetInt(\"uZoneBlendMode\", zone_multiplicative_blending ? 1 : 0);" in shader_h
    assert "if (uZoneBlendMode == 1 && !isBadge)" in shader_h
    assert "FragColor.rgb = mix(vec3(1.0), FragColor.rgb, FragColor.a * Tint.a * uGlobalTint.a);" in shader_h

    # 2. Zone overlay drawer blend mode switches
    zod_cpp = (root / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.cpp").read_text(encoding="utf-8")
    assert "sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);" in zod_cpp
    assert "sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);" in zod_cpp

    # 3. GraphicsPage wiring
    gp_cpp = (root / "source" / "app" / "preferences" / "graphics_page.cpp").read_text(encoding="utf-8")
    assert "zone_multiplicative_chkbox = PreferencesLayout::AddCheckBoxRow" in gp_cpp
    assert "g_settings.setInteger(Config::ZONE_MULTIPLICATIVE_BLENDING, zone_multiplicative_chkbox->GetValue());" in gp_cpp
    assert "zone_multiplicative_chkbox->SetValue(false);" in gp_cpp





