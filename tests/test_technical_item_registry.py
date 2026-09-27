"""
Unit Test Suite for TechnicalItemRegistry and Tile Indicators
Tests:
1. Classification of technical items across Tibia protocol versions (stairs, walkable, walls, light).
2. Unknown item fallback to None.
3. Marker ID calculation idempotency.
4. Badge style text and color validity.
"""

import pytest
from enum import IntEnum

class TileIndicatorType(IntEnum):
    NONE = 0
    HOUSE_ENTRY = 1
    SPAWN = 2
    TOWN_TEMPLE = 3
    WAYPOINT = 4
    TECH_INVISIBLE_STAIR = 5
    TECH_INVISIBLE_WALKABLE = 6
    TECH_INVISIBLE_WALL = 7
    TECH_PRIMAL_LIGHT = 8
    INVALID_GROUND = 9
    INVALID_ITEM = 10
    INVALID_ZONE = 11

class ProtocolItems:
    INVISIBLE_STAIRS = {459, 469}
    INVISIBLE_WALKABLE = {460, 470, 17970, 20028, 34168}
    INVISIBLE_WALLS = {1548, 2187}
    PRIMAL_LIGHT_MIN = 39092
    PRIMAL_LIGHT_MAX = 39100
    PRIMAL_LIGHT_EXTRA = {39236, 39367, 39368}

class TechnicalItemRegistry:
    @staticmethod
    def classify(server_id: int, client_id: int) -> TileIndicatorType:
        if server_id in ProtocolItems.INVISIBLE_STAIRS or client_id in ProtocolItems.INVISIBLE_STAIRS:
            return TileIndicatorType.TECH_INVISIBLE_STAIR

        if server_id in ProtocolItems.INVISIBLE_WALKABLE or client_id in ProtocolItems.INVISIBLE_WALKABLE:
            return TileIndicatorType.TECH_INVISIBLE_WALKABLE

        if server_id in ProtocolItems.INVISIBLE_WALLS or client_id in ProtocolItems.INVISIBLE_WALLS:
            return TileIndicatorType.TECH_INVISIBLE_WALL

        if ((ProtocolItems.PRIMAL_LIGHT_MIN <= client_id <= ProtocolItems.PRIMAL_LIGHT_MAX) or
            (ProtocolItems.PRIMAL_LIGHT_MIN <= server_id <= ProtocolItems.PRIMAL_LIGHT_MAX) or
            server_id in ProtocolItems.PRIMAL_LIGHT_EXTRA or client_id in ProtocolItems.PRIMAL_LIGHT_EXTRA):
            return TileIndicatorType.TECH_PRIMAL_LIGHT

        return TileIndicatorType.NONE

    @staticmethod
    def is_technical(server_id: int, client_id: int) -> bool:
        return TechnicalItemRegistry.classify(server_id, client_id) != TileIndicatorType.NONE

    @staticmethod
    def get_marker_id(ind_type: TileIndicatorType) -> float:
        mapping = {
            TileIndicatorType.SPAWN: 2000000.0,
            TileIndicatorType.TOWN_TEMPLE: 3000000.0,
            TileIndicatorType.WAYPOINT: 4000000.0,
            TileIndicatorType.TECH_INVISIBLE_STAIR: 5000000.0,
            TileIndicatorType.TECH_INVISIBLE_WALKABLE: 6000000.0,
            TileIndicatorType.TECH_INVISIBLE_WALL: 7000000.0,
            TileIndicatorType.TECH_PRIMAL_LIGHT: 8000000.0,
            TileIndicatorType.INVALID_GROUND: 9000000.0,
            TileIndicatorType.INVALID_ITEM: 10000000.0,
            TileIndicatorType.INVALID_ZONE: 11000000.0,
        }
        return mapping.get(ind_type, 0.0)

    @staticmethod
    def get_badge_style(ind_type: TileIndicatorType):
        styles = {
            TileIndicatorType.HOUSE_ENTRY: {"text": "ENTRY", "border": (38, 128, 255)},
            TileIndicatorType.SPAWN: {"text": "SPAWN", "border": (255, 51, 255)},
            TileIndicatorType.TOWN_TEMPLE: {"text": "TOWN", "border": (255, 215, 0)},
            TileIndicatorType.WAYPOINT: {"text": "WAYPT", "border": (0, 255, 255)},
            TileIndicatorType.TECH_INVISIBLE_STAIR: {"text": "STAIR", "border": (255, 240, 30)},
            TileIndicatorType.TECH_INVISIBLE_WALKABLE: {"text": "WALK", "border": (0, 240, 240)},
            TileIndicatorType.TECH_INVISIBLE_WALL: {"text": "BLOCK", "border": (255, 40, 40)},
            TileIndicatorType.TECH_PRIMAL_LIGHT: {"text": "LIGHT", "border": (90, 220, 255)},
            TileIndicatorType.INVALID_GROUND: {"text": "INVALID", "border": (255, 38, 38)},
            TileIndicatorType.INVALID_ITEM: {"text": "INVALID", "border": (255, 165, 0)},
            TileIndicatorType.INVALID_ZONE: {"text": "INVALID", "border": (255, 0, 255)},
        }
        return styles.get(ind_type, {"text": "?", "border": (200, 200, 200)})


def test_invisible_stairs_classification():
    assert TechnicalItemRegistry.classify(459, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 469) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.is_technical(459, 469) is True

def test_invisible_walkable_classification():
    for cid in [460, 470, 17970, 20028, 34168]:
        assert TechnicalItemRegistry.classify(0, cid) == TileIndicatorType.TECH_INVISIBLE_WALKABLE
        assert TechnicalItemRegistry.classify(cid, 0) == TileIndicatorType.TECH_INVISIBLE_WALKABLE

def test_invisible_wall_classification():
    assert TechnicalItemRegistry.classify(1548, 0) == TileIndicatorType.TECH_INVISIBLE_WALL
    assert TechnicalItemRegistry.classify(0, 2187) == TileIndicatorType.TECH_INVISIBLE_WALL

def test_primal_light_range_classification():
    for cid in range(39092, 39101):
        assert TechnicalItemRegistry.classify(0, cid) == TileIndicatorType.TECH_PRIMAL_LIGHT
    for extra in [39236, 39367, 39368]:
        assert TechnicalItemRegistry.classify(extra, 0) == TileIndicatorType.TECH_PRIMAL_LIGHT

def test_unknown_items():
    assert TechnicalItemRegistry.classify(0, 0) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(100, 200) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(2160, 2160) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.is_technical(2160, 0) is False

def test_marker_ids():
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.TECH_INVISIBLE_STAIR) == 5000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.TECH_INVISIBLE_WALKABLE) == 6000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.TECH_INVISIBLE_WALL) == 7000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.TECH_PRIMAL_LIGHT) == 8000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.INVALID_GROUND) == 9000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.INVALID_ITEM) == 10000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.INVALID_ZONE) == 11000000.0
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.NONE) == 0.0

def test_badge_styles():
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_STAIR)["text"] == "STAIR"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALKABLE)["text"] == "WALK"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALL)["text"] == "BLOCK"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_PRIMAL_LIGHT)["text"] == "LIGHT"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["text"] == "INVALID"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["text"] == "INVALID"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["text"] == "INVALID"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["border"] == (255, 38, 38)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["border"] == (255, 165, 0)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["border"] == (255, 0, 255)

def test_invalid_typography_bitmasks():
    in_mask = [0x00000000, 0x0745D528, 0x09455568, 0x0945D5A8, 0x09455528, 0x075D4928, 0x00000000]
    out_mask = [0x0FEFFFFC, 0x18AA2AD4, 0x16AAAA94, 0x16AA2A54, 0x16BAAAD4, 0x18A2B6D4, 0x0FFFFFFC]

    # Verify rows 0 and 6 are empty padding
    assert in_mask[0] == 0
    assert in_mask[6] == 0

    # Verify no pixel is simultaneously in text inside and text outline
    for r in range(7):
        assert (in_mask[r] & out_mask[r]) == 0, f"Row {r} has overlapping inside and outline masks"

    # Verify all text bits stay within 32 columns and have safe margins from border (lx=0 and lx=31)
    for r in range(7):
        assert (in_mask[r] & 1) == 0, f"Row {r} touches left border (bit 0)"
        assert (in_mask[r] & (1 << 31)) == 0, f"Row {r} touches right border (bit 31)"
        assert (out_mask[r] & 1) == 0, f"Row {r} outline touches left border (bit 0)"
        assert (out_mask[r] & (1 << 31)) == 0, f"Row {r} outline touches right border (bit 31)"

