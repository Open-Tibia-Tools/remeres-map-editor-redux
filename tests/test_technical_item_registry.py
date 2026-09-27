"""
Unit Test Suite for TechnicalItemRegistry and Tile Indicators
Tests:
1. Classification of technical items loaded dynamically from config.toml (stairs, walkable, walls, light).
2. Zero hardcoded fallbacks: when lists are empty, classify() returns NONE.
3. Custom user-configured IDs from TOML config.
4. Sorting, deduplication, and O(log N) lookup idempotency.
5. Marker ID calculation idempotency.
6. Badge style text and color validity.
"""

import bisect
import pytest
from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Dict, Any

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

@dataclass
class TechnicalItemLists:
    invisible_stairs: List[int] = field(default_factory=list)
    invisible_walkable: List[int] = field(default_factory=list)
    invisible_walls: List[int] = field(default_factory=list)
    primal_lights: List[int] = field(default_factory=list)

DEFAULT_CONFIG_TECHNICAL_ITEMS = {
    "invisible_stairs": [459, 469],
    "invisible_walkable": [460, 470, 17970, 20028, 34168],
    "invisible_walls": [1548, 2187],
    "primal_lights": [
        39092, 39093, 39094, 39095, 39096, 39097, 39098, 39099, 39100,
        39236, 39367, 39368
    ],
}

class TechnicalItemRegistry:
    _lists: TechnicalItemLists = TechnicalItemLists()

    @classmethod
    def initialize(cls, config: Dict[str, Any]):
        tech_section = config.get("technical_items", {})
        stairs = [int(x) for x in tech_section.get("invisible_stairs", []) if 0 <= int(x) <= 65535]
        walkable = [int(x) for x in tech_section.get("invisible_walkable", []) if 0 <= int(x) <= 65535]
        walls = [int(x) for x in tech_section.get("invisible_walls", []) if 0 <= int(x) <= 65535]
        lights = [int(x) for x in tech_section.get("primal_lights", []) if 0 <= int(x) <= 65535]

        cls.set_lists(TechnicalItemLists(
            invisible_stairs=stairs,
            invisible_walkable=walkable,
            invisible_walls=walls,
            primal_lights=lights,
        ))

    @classmethod
    def set_lists(cls, lists: TechnicalItemLists):
        def sort_and_dedup(lst: List[int]) -> List[int]:
            return sorted(set(lst))

        cls._lists = TechnicalItemLists(
            invisible_stairs=sort_and_dedup(lists.invisible_stairs),
            invisible_walkable=sort_and_dedup(lists.invisible_walkable),
            invisible_walls=sort_and_dedup(lists.invisible_walls),
            primal_lights=sort_and_dedup(lists.primal_lights),
        )

    @classmethod
    def get_lists(cls) -> TechnicalItemLists:
        return cls._lists

    @classmethod
    def classify(cls, server_id: int, client_id: int) -> TileIndicatorType:
        def binary_contains(lst: List[int], val: int) -> bool:
            if not lst or val == 0:
                return False
            idx = bisect.bisect_left(lst, val)
            return idx < len(lst) and lst[idx] == val

        def contains(lst: List[int]) -> bool:
            return binary_contains(lst, server_id) or binary_contains(lst, client_id)

        if contains(cls._lists.invisible_stairs):
            return TileIndicatorType.TECH_INVISIBLE_STAIR

        if contains(cls._lists.invisible_walkable):
            return TileIndicatorType.TECH_INVISIBLE_WALKABLE

        if contains(cls._lists.invisible_walls):
            return TileIndicatorType.TECH_INVISIBLE_WALL

        if contains(cls._lists.primal_lights):
            return TileIndicatorType.TECH_PRIMAL_LIGHT

        return TileIndicatorType.NONE

    @classmethod
    def is_technical(cls, server_id: int, client_id: int) -> bool:
        return cls.classify(server_id, client_id) != TileIndicatorType.NONE

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
            TileIndicatorType.INVALID_GROUND: {"text": "", "border": (255, 0, 0)},
            TileIndicatorType.INVALID_ITEM: {"text": "", "border": (255, 165, 0)},
            TileIndicatorType.INVALID_ZONE: {"text": "", "border": (255, 0, 255)},
        }
        return styles.get(ind_type, {"text": "?", "border": (200, 200, 200)})


@pytest.fixture(autouse=True)
def setup_default_registry():
    """Ensure registry is initialized with defaults for each test unless overridden."""
    TechnicalItemRegistry.initialize({"technical_items": DEFAULT_CONFIG_TECHNICAL_ITEMS})


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

def test_primal_light_classification():
    for cid in range(39092, 39101):
        assert TechnicalItemRegistry.classify(0, cid) == TileIndicatorType.TECH_PRIMAL_LIGHT
    for extra in [39236, 39367, 39368]:
        assert TechnicalItemRegistry.classify(extra, 0) == TileIndicatorType.TECH_PRIMAL_LIGHT

def test_zero_hardcoded_fallbacks_when_config_empty():
    """Verify that if technical_items is empty in config, NO items match (zero C++ hardcoded fallback)."""
    TechnicalItemRegistry.initialize({"technical_items": {}})
    # Standard IDs should now classify as NONE
    assert TechnicalItemRegistry.classify(459, 469) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(460, 470) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(1548, 2187) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(39092, 39092) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.is_technical(459, 469) is False

def test_custom_user_configured_ids():
    """Verify that user-defined IDs added to config.toml are recognized properly."""
    custom_config = {
        "technical_items": {
            "invisible_stairs": [9999],
            "invisible_walls": [8888, 12345],
        }
    }
    TechnicalItemRegistry.initialize(custom_config)
    assert TechnicalItemRegistry.classify(9999, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 8888) == TileIndicatorType.TECH_INVISIBLE_WALL
    assert TechnicalItemRegistry.classify(12345, 0) == TileIndicatorType.TECH_INVISIBLE_WALL
    # Non-configured IDs are not classified
    assert TechnicalItemRegistry.classify(459, 0) == TileIndicatorType.NONE

def test_sorting_and_deduplication():
    lists = TechnicalItemLists(
        invisible_stairs=[500, 100, 500, 200],
        invisible_walkable=[300, 300, 100],
        invisible_walls=[],
        primal_lights=[],
    )
    TechnicalItemRegistry.set_lists(lists)
    stored = TechnicalItemRegistry.get_lists()
    assert stored.invisible_stairs == [100, 200, 500]
    assert stored.invisible_walkable == [100, 300]
    assert TechnicalItemRegistry.classify(200, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR

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
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["border"] == (255, 0, 0)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["border"] == (255, 165, 0)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["border"] == (255, 0, 255)
