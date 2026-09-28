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
