"""
Unit Test Suite for TechnicalItemRegistry and Tile Indicators
Tests:
1. Strict separation of Server IDs vs Client IDs in technical item classification.
2. Verification that Server ID matching an appearance Client ID does NOT falsely classify.
3. Verification that Client ID matching a database Server ID does NOT falsely classify.
4. Support for both sub-table [technical_items.<name>] and flat <name>_server_ids / <name>_client_ids configs.
5. Zero hardcoded fallbacks when config is empty.
6. Sorting, deduplication, and O(log N) lookup idempotency.
7. Marker ID calculation idempotency.
8. Badge style text and color validity.
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
class TechnicalIdFilter:
    server_ids: List[int] = field(default_factory=list)
    client_ids: List[int] = field(default_factory=list)

    def matches(self, sid: int, cid: int) -> bool:
        if sid != 0 and self.server_ids:
            idx = bisect.bisect_left(self.server_ids, sid)
            if idx < len(self.server_ids) and self.server_ids[idx] == sid:
                return True
        if cid != 0 and self.client_ids:
            idx = bisect.bisect_left(self.client_ids, cid)
            if idx < len(self.client_ids) and self.client_ids[idx] == cid:
                return True
        return False

    def sort_and_dedup(self):
        self.server_ids = sorted(set(self.server_ids))
        self.client_ids = sorted(set(self.client_ids))

@dataclass
class TechnicalItemLists:
    invisible_stairs: TechnicalIdFilter = field(default_factory=TechnicalIdFilter)
    invisible_walkable: TechnicalIdFilter = field(default_factory=TechnicalIdFilter)
    invisible_walls: TechnicalIdFilter = field(default_factory=TechnicalIdFilter)
    primal_lights: TechnicalIdFilter = field(default_factory=TechnicalIdFilter)

DEFAULT_CONFIG_TECHNICAL_ITEMS = {
    "invisible_stairs": {
        "server_ids": [459],
        "client_ids": [469],
    },
    "invisible_walkable": {
        "server_ids": [460],
        "client_ids": [470, 17970, 20028, 34168],
    },
    "invisible_walls": {
        "server_ids": [1548],
        "client_ids": [2187],
    },
    "primal_lights": {
        "server_ids": [],
        "client_ids": [39092, 39093, 39094, 39095, 39096, 39097, 39098, 39099, 39100, 39236, 39367, 39368],
    },
}

class TechnicalItemRegistry:
    _lists: TechnicalItemLists = TechnicalItemLists()

    @classmethod
    def initialize(cls, config: Dict[str, Any]):
        tech_section = config.get("technical_items", {})
        lists = TechnicalItemLists()

        def parse_filter(key: str) -> TechnicalIdFilter:
            filter_obj = TechnicalIdFilter()
            # 1. Try sub-table: [technical_items.<key>]
            sub = tech_section.get(key)
            if isinstance(sub, dict):
                s_arr = sub.get("server_ids", [])
                c_arr = sub.get("client_ids", [])
                filter_obj.server_ids.extend([int(x) for x in s_arr if 0 <= int(x) <= 0xFFFFFFFF])
                filter_obj.client_ids.extend([int(x) for x in c_arr if 0 <= int(x) <= 0xFFFFFFFF])

            # 2. Also check flat keys in [technical_items]: <key>_server_ids, <key>_client_ids
            flat_s = tech_section.get(f"{key}_server_ids", [])
            flat_c = tech_section.get(f"{key}_client_ids", [])
            filter_obj.server_ids.extend([int(x) for x in flat_s if 0 <= int(x) <= 0xFFFFFFFF])
            filter_obj.client_ids.extend([int(x) for x in flat_c if 0 <= int(x) <= 0xFFFFFFFF])

            return filter_obj

        lists.invisible_stairs = parse_filter("invisible_stairs")
        lists.invisible_walkable = parse_filter("invisible_walkable")
        lists.invisible_walls = parse_filter("invisible_walls")
        lists.primal_lights = parse_filter("primal_lights")

        cls.set_lists(lists)

    @classmethod
    def set_lists(cls, lists: TechnicalItemLists):
        lists.invisible_stairs.sort_and_dedup()
        lists.invisible_walkable.sort_and_dedup()
        lists.invisible_walls.sort_and_dedup()
        lists.primal_lights.sort_and_dedup()
        cls._lists = lists

    @classmethod
    def get_lists(cls) -> TechnicalItemLists:
        return cls._lists

    @classmethod
    def classify(cls, server_id: int, client_id: int) -> TileIndicatorType:
        if cls._lists.invisible_stairs.matches(server_id, client_id):
            return TileIndicatorType.TECH_INVISIBLE_STAIR

        if cls._lists.invisible_walkable.matches(server_id, client_id):
            return TileIndicatorType.TECH_INVISIBLE_WALKABLE

        if cls._lists.invisible_walls.matches(server_id, client_id):
            return TileIndicatorType.TECH_INVISIBLE_WALL

        if cls._lists.primal_lights.matches(server_id, client_id):
            return TileIndicatorType.TECH_PRIMAL_LIGHT

        return TileIndicatorType.NONE

    @classmethod
    def is_technical(cls, server_id: int, client_id: int) -> bool:
        return cls.classify(server_id, client_id) != TileIndicatorType.NONE

    @staticmethod
    def get_marker_id(ind_type: TileIndicatorType) -> float:
        mapping = {
            TileIndicatorType.HOUSE_ENTRY: 1000000.0,
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
            TileIndicatorType.HOUSE_ENTRY: {"text": "ENTRY", "short_text": "E", "border": (38, 128, 255)},
            TileIndicatorType.SPAWN: {"text": "SPAWN", "short_text": "S", "border": (255, 51, 255)},
            TileIndicatorType.TOWN_TEMPLE: {"text": "TOWN", "short_text": "T", "border": (255, 215, 0)},
            TileIndicatorType.WAYPOINT: {"text": "WAYPT", "short_text": "W", "border": (0, 255, 255)},
            TileIndicatorType.TECH_INVISIBLE_STAIR: {"text": "STAIR", "short_text": "S", "border": (255, 240, 30)},
            TileIndicatorType.TECH_INVISIBLE_WALKABLE: {"text": "WALK", "short_text": "W", "border": (0, 240, 240)},
            TileIndicatorType.TECH_INVISIBLE_WALL: {"text": "BLOCK", "short_text": "B", "border": (255, 40, 40)},
            TileIndicatorType.TECH_PRIMAL_LIGHT: {"text": "LIGHT", "short_text": "L", "border": (90, 220, 255)},
            TileIndicatorType.INVALID_GROUND: {"text": "", "short_text": "", "border": (255, 0, 0)},
            TileIndicatorType.INVALID_ITEM: {"text": "", "short_text": "", "border": (255, 165, 0)},
            TileIndicatorType.INVALID_ZONE: {"text": "", "short_text": "", "border": (255, 0, 255)},
        }
        return styles.get(ind_type, {"text": "?", "short_text": "?", "border": (200, 200, 200)})


@pytest.fixture(autouse=True)
def setup_default_registry():
    """Ensure registry is initialized with defaults for each test unless overridden."""
    TechnicalItemRegistry.initialize({"technical_items": DEFAULT_CONFIG_TECHNICAL_ITEMS})


def test_invisible_stairs_strict_separation():
    # Server 459 is invisible stairs
    assert TechnicalItemRegistry.classify(459, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    # Client 469 is invisible stairs
    assert TechnicalItemRegistry.classify(0, 469) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(459, 469) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.is_technical(459, 469) is True

    # Server 469 is a normal item in OTBM, NOT invisible stairs!
    assert TechnicalItemRegistry.classify(469, 0) == TileIndicatorType.NONE
    # Client 459 is a normal sprite in Tibia.dat, NOT invisible stairs!
    assert TechnicalItemRegistry.classify(0, 459) == TileIndicatorType.NONE

def test_invisible_walkable_strict_separation():
    # Server 460 is invisible walkable
    assert TechnicalItemRegistry.classify(460, 0) == TileIndicatorType.TECH_INVISIBLE_WALKABLE
    # Client IDs across Tibia versions
    for cid in [470, 17970, 20028, 34168]:
        assert TechnicalItemRegistry.classify(0, cid) == TileIndicatorType.TECH_INVISIBLE_WALKABLE

    # Server IDs matching those numbers in OTBM must NOT be classified as walkable!
    for sid in [470, 17970, 20028, 34168]:
        assert TechnicalItemRegistry.classify(sid, 0) == TileIndicatorType.NONE

    # Client 460 is NOT invisible walkable
    assert TechnicalItemRegistry.classify(0, 460) == TileIndicatorType.NONE

def test_invisible_wall_strict_separation():
    # Server 1548 is invisible wall
    assert TechnicalItemRegistry.classify(1548, 0) == TileIndicatorType.TECH_INVISIBLE_WALL
    # Client 2187 is invisible wall
    assert TechnicalItemRegistry.classify(0, 2187) == TileIndicatorType.TECH_INVISIBLE_WALL

    # Server 2187 is a common game item (e.g. gold/weapon), NOT an invisible wall!
    assert TechnicalItemRegistry.classify(2187, 0) == TileIndicatorType.NONE
    # Client 1548 is NOT an invisible wall
    assert TechnicalItemRegistry.classify(0, 1548) == TileIndicatorType.NONE

def test_primal_light_strict_separation():
    # Client IDs for primal light sources
    for cid in range(39092, 39101):
        assert TechnicalItemRegistry.classify(0, cid) == TileIndicatorType.TECH_PRIMAL_LIGHT
    for extra in [39236, 39367, 39368]:
        assert TechnicalItemRegistry.classify(0, extra) == TileIndicatorType.TECH_PRIMAL_LIGHT

    # Server IDs matching those numbers in OTBM must NOT be classified as primal light!
    for sid in range(39092, 39101):
        assert TechnicalItemRegistry.classify(sid, 0) == TileIndicatorType.NONE
    for extra in [39236, 39367, 39368]:
        assert TechnicalItemRegistry.classify(extra, 0) == TileIndicatorType.NONE

def test_flat_keys_configuration_support():
    """Verify that flat <name>_server_ids and <name>_client_ids syntax works."""
    flat_config = {
        "technical_items": {
            "invisible_stairs_server_ids": [1000],
            "invisible_stairs_client_ids": [2000],
        }
    }
    TechnicalItemRegistry.initialize(flat_config)
    assert TechnicalItemRegistry.classify(1000, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 2000) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(2000, 0) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(0, 1000) == TileIndicatorType.NONE

def test_zero_hardcoded_fallbacks_when_config_empty():
    """Verify that if technical_items is empty in config, NO items match (zero C++ hardcoded fallback)."""
    TechnicalItemRegistry.initialize({"technical_items": {}})
    assert TechnicalItemRegistry.classify(459, 469) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(460, 470) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(1548, 2187) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(0, 39092) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.is_technical(459, 469) is False

def test_custom_user_configured_ids():
    """Verify that user-defined IDs added to config.toml are recognized properly."""
    custom_config = {
        "technical_items": {
            "invisible_stairs": {
                "server_ids": [9999],
                "client_ids": [8888],
            },
            "invisible_walls": {
                "server_ids": [12345],
            },
        }
    }
    TechnicalItemRegistry.initialize(custom_config)
    assert TechnicalItemRegistry.classify(9999, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 8888) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(12345, 0) == TileIndicatorType.TECH_INVISIBLE_WALL
    # Non-configured IDs or cross-matched IDs do not classify
    assert TechnicalItemRegistry.classify(8888, 0) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(0, 9999) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(459, 0) == TileIndicatorType.NONE

def test_sorting_and_deduplication():
    lists = TechnicalItemLists()
    lists.invisible_stairs = TechnicalIdFilter(server_ids=[500, 100, 500, 200], client_ids=[300, 100, 300])
    TechnicalItemRegistry.set_lists(lists)
    stored = TechnicalItemRegistry.get_lists()
    assert stored.invisible_stairs.server_ids == [100, 200, 500]
    assert stored.invisible_stairs.client_ids == [100, 300]
    assert TechnicalItemRegistry.classify(200, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 300) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(300, 0) == TileIndicatorType.NONE

def test_unknown_items():
    assert TechnicalItemRegistry.classify(0, 0) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(100, 200) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(2160, 2160) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.is_technical(2160, 0) is False

def test_marker_ids():
    assert TechnicalItemRegistry.get_marker_id(TileIndicatorType.HOUSE_ENTRY) == 1000000.0
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
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_STAIR)["short_text"] == "S"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALKABLE)["text"] == "WALK"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALKABLE)["short_text"] == "W"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALL)["text"] == "BLOCK"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_INVISIBLE_WALL)["short_text"] == "B"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_PRIMAL_LIGHT)["text"] == "LIGHT"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.TECH_PRIMAL_LIGHT)["short_text"] == "L"
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["text"] == ""
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_GROUND)["border"] == (255, 0, 0)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ITEM)["border"] == (255, 165, 0)
    assert TechnicalItemRegistry.get_badge_style(TileIndicatorType.INVALID_ZONE)["border"] == (255, 0, 255)

def test_search_dialog_catalog_row_badge_classification():
    # In Search for Item dialog (AdvancedFinderResultsView), rows with technical items must yield their badge
    stair_type = TechnicalItemRegistry.classify(459, 469)
    assert stair_type == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.get_badge_style(stair_type)["text"] == "STAIR"

    walk_type = TechnicalItemRegistry.classify(460, 470)
    assert walk_type == TileIndicatorType.TECH_INVISIBLE_WALKABLE
    assert TechnicalItemRegistry.get_badge_style(walk_type)["text"] == "WALK"

    wall_type = TechnicalItemRegistry.classify(1548, 2187)
    assert wall_type == TileIndicatorType.TECH_INVISIBLE_WALL
    assert TechnicalItemRegistry.get_badge_style(wall_type)["text"] == "BLOCK"

    light_type = TechnicalItemRegistry.classify(0, 39092)
    assert light_type == TileIndicatorType.TECH_PRIMAL_LIGHT
    assert TechnicalItemRegistry.get_badge_style(light_type)["text"] == "LIGHT"

    # Regular items do not display technical badges
    normal_type = TechnicalItemRegistry.classify(2160, 3031)
    assert normal_type == TileIndicatorType.NONE

def test_full_width_32bit_ids():
    """Verify that full-width 32-bit item IDs (> 65535) are supported without narrowing or truncation."""
    config_32bit = {
        "technical_items": {
            "invisible_stairs": {
                "server_ids": [100000],
                "client_ids": [200000],
            },
            "invisible_walls": {
                "server_ids": [0xFFFFFF00],
            }
        }
    }
    TechnicalItemRegistry.initialize(config_32bit)
    assert TechnicalItemRegistry.classify(100000, 0) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0, 200000) == TileIndicatorType.TECH_INVISIBLE_STAIR
    assert TechnicalItemRegistry.classify(0xFFFFFF00, 0) == TileIndicatorType.TECH_INVISIBLE_WALL
    # Values narrowed to 16 bits must NOT falsely classify
    assert TechnicalItemRegistry.classify(100000 & 0xFFFF, 0) == TileIndicatorType.NONE
    assert TechnicalItemRegistry.classify(0, 200000 & 0xFFFF) == TileIndicatorType.NONE


