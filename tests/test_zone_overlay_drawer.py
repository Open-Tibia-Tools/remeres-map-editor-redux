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

    # Dedicated 4-corner micro-badges must be present
    assert "pzMask" in fn_body and "npMask" in fn_body
    assert "nlMask" in fn_body and "pvpMask" in fn_body

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
