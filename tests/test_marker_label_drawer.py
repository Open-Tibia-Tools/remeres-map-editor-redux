"""
Unit & Differential Test Suite for MarkerLabelDrawer and Blocking Overlay Colors
Tests:
1. Marker label in-game suppression (ingame == True disables both waypoints and towns).
2. Option filtering (show_waypoints and show_towns independently control label visibility).
3. Floor visibility rules (current floor, surface multi-floor when show_all_floors is active).
4. LOD suppression at extreme zoom (zoom > 10.0).
5. Tile collision stacking (town label offsets higher when sharing tile with waypoint).
6. Blocking overlay shader color verification (bright cyan border + gray background wash).
"""

import pytest
import re
from pathlib import Path

GROUND_LAYER = 7

class MockPosition:
    def __init__(self, x: int, y: int, z: int):
        self.x = x
        self.y = y
        self.z = z

class MockWaypoint:
    def __init__(self, name: str, pos: MockPosition):
        self.name = name
        self.pos = pos

class MockTown:
    def __init__(self, town_id: int, name: str, temple_pos: MockPosition):
        self.id = town_id
        self.name = name
        self.temple_pos = temple_pos

    def get_id(self) -> int:
        return self.id

    def get_name(self) -> str:
        return self.name

    def get_temple_position(self) -> MockPosition:
        return self.temple_pos

class MockRenderView:
    def __init__(self, floor: int = 7, zoom: float = 1.0, screensize_x: int = 1920, screensize_y: int = 1080,
                 start_z: int = 0, end_z: int = 7):
        self.floor = floor
        self.zoom = zoom
        self.screensize_x = screensize_x
        self.screensize_y = screensize_y
        self.start_z = start_z
        self.end_z = end_z

    def is_tile_visible(self, x: int, y: int, z: int) -> bool:
        # Simplified viewport check
        return 0 <= x <= 2000 and 0 <= y <= 2000

class MockDrawingOptions:
    def __init__(self, show_waypoints: bool = True, show_towns: bool = True,
                 ingame: bool = False, show_all_floors: bool = False):
        self.show_waypoints = show_waypoints
        self.show_towns = show_towns
        self.ingame = ingame
        self.show_all_floors = show_all_floors


def is_floor_visible(z: int, view: MockRenderView, options: MockDrawingOptions) -> bool:
    if z == view.floor:
        return True
    if options.show_all_floors and view.floor <= GROUND_LAYER and view.start_z <= z <= view.end_z:
        return True
    return False


def collect_marker_labels(waypoints: list, towns: list, view: MockRenderView, options: MockDrawingOptions):
    if options.ingame:
        return []

    if not options.show_waypoints and not options.show_towns:
        return []

    if view.zoom > 10.0:
        return []

    labels = []
    tile_size_screen = 32.0 / view.zoom

    # 1. Waypoints
    if options.show_waypoints:
        for wp in waypoints:
            if not wp or not wp.name:
                continue
            if not is_floor_visible(wp.pos.z, view, options):
                continue
            if not view.is_tile_visible(wp.pos.x, wp.pos.y, wp.pos.z):
                continue

            screen_x = wp.pos.x * tile_size_screen
            screen_y = wp.pos.y * tile_size_screen
            label_x = screen_x + tile_size_screen * 0.5
            label_y = screen_y - 3.0
            labels.append({
                'type': 'waypoint',
                'text': wp.name,
                'x': label_x,
                'y': label_y,
                'height': 12.0
            })

    # 2. Towns
    if options.show_towns:
        for town in towns:
            if not town:
                continue
            pos = town.get_temple_position()
            if not is_floor_visible(pos.z, view, options):
                continue
            if not view.is_tile_visible(pos.x, pos.y, pos.z):
                continue

            text = town.get_name() if town.get_name() else f"Town {town.get_id()}"
            screen_x = pos.x * tile_size_screen
            screen_y = pos.y * tile_size_screen
            label_x = screen_x + tile_size_screen * 0.5
            label_y = screen_y - 3.0

            # Collision check with existing labels on same tile
            for existing in labels:
                if abs(existing['x'] - label_x) < 2.0 and abs(existing['y'] - label_y) < 10.0:
                    label_y -= (existing['height'] + 4.0 + 3.0)
                    break

            labels.append({
                'type': 'town',
                'text': text,
                'x': label_x,
                'y': label_y,
                'height': 12.0
            })

    return labels


def test_marker_label_suppression_ingame():
    wp = MockWaypoint("depot", MockPosition(100, 100, 7))
    town = MockTown(1, "Thais", MockPosition(100, 100, 7))
    view = MockRenderView(floor=7, zoom=1.0)
    opts_ingame = MockDrawingOptions(show_waypoints=True, show_towns=True, ingame=True)

    labels = collect_marker_labels([wp], [town], view, opts_ingame)
    assert len(labels) == 0, "All marker labels must be suppressed in In-Game view mode!"


def test_marker_label_option_filters():
    wp = MockWaypoint("depot", MockPosition(100, 100, 7))
    town = MockTown(1, "Thais", MockPosition(200, 200, 7))
    view = MockRenderView(floor=7, zoom=1.0)

    # Waypoints only
    opts_wp = MockDrawingOptions(show_waypoints=True, show_towns=False, ingame=False)
    labels = collect_marker_labels([wp], [town], view, opts_wp)
    assert len(labels) == 1
    assert labels[0]['text'] == "depot"
    assert labels[0]['type'] == 'waypoint'

    # Towns only
    opts_town = MockDrawingOptions(show_waypoints=False, show_towns=True, ingame=False)
    labels = collect_marker_labels([wp], [town], view, opts_town)
    assert len(labels) == 1
    assert labels[0]['text'] == "Thais"
    assert labels[0]['type'] == 'town'


def test_marker_label_floor_visibility():
    wp_ground = MockWaypoint("ground_wp", MockPosition(100, 100, 7))
    wp_roof = MockWaypoint("roof_wp", MockPosition(100, 100, 5))
    wp_underground = MockWaypoint("cave_wp", MockPosition(100, 100, 9))
    view = MockRenderView(floor=7, zoom=1.0)

    # Single floor mode (show_all_floors = False)
    opts_single = MockDrawingOptions(show_waypoints=True, show_all_floors=False)
    labels = collect_marker_labels([wp_ground, wp_roof, wp_underground], [], view, opts_single)
    assert len(labels) == 1
    assert labels[0]['text'] == "ground_wp"

    # Multi floor surface mode (show_all_floors = True)
    opts_multi = MockDrawingOptions(show_waypoints=True, show_all_floors=True)
    labels = collect_marker_labels([wp_ground, wp_roof, wp_underground], [], view, opts_multi)
    assert len(labels) == 2, "Surface floors (5 and 7) should be visible when show_all_floors is active"
    texts = {l['text'] for l in labels}
    assert "ground_wp" in texts and "roof_wp" in texts
    assert "cave_wp" not in texts, "Underground floor 9 should not be visible when viewing surface"


def test_marker_label_zoom_lod():
    wp = MockWaypoint("depot", MockPosition(100, 100, 7))
    view_extreme_zoom = MockRenderView(floor=7, zoom=12.0)
    opts = MockDrawingOptions(show_waypoints=True, show_towns=True)

    labels = collect_marker_labels([wp], [], view_extreme_zoom, opts)
    assert len(labels) == 0, "Labels must be culled at extreme zoom (zoom > 10.0)"


def test_marker_label_tile_collision_stacking():
    pos = MockPosition(100, 100, 7)
    wp = MockWaypoint("depot", pos)
    town = MockTown(1, "Thais", pos)
    view = MockRenderView(floor=7, zoom=1.0)
    opts = MockDrawingOptions(show_waypoints=True, show_towns=True)

    labels = collect_marker_labels([wp], [town], view, opts)
    assert len(labels) == 2
    wp_lbl = next(l for l in labels if l['type'] == 'waypoint')
    town_lbl = next(l for l in labels if l['type'] == 'town')

    assert wp_lbl['x'] == town_lbl['x'], "Both labels should share horizontal center"
    assert town_lbl['y'] < wp_lbl['y'], "Town label must be stacked higher than waypoint label to avoid collision"


def test_blocking_overlay_shader_colors():
    shader_path = Path(__file__).parent.parent / "source" / "rendering" / "shaders" / "zone_shader.h"
    content = shader_path.read_text(encoding="utf-8")

    assert "evaluateBlockingOverlay" in content, "evaluateBlockingOverlay function must exist in zone_shader.h"
    idx = content.find("evaluateBlockingOverlay")
    fn_body = content[idx:idx + 500]

    # Border: vec4(0.00, 0.95, 1.00, 0.95) -> Bright cyan
    assert "0.00, 0.95, 1.00, 0.95" in fn_body or "0.0, 0.95, 1.0, 0.95" in fn_body, "Blocking border must be bright cyan"
    # Background: vec4(0.40, 0.40, 0.40, 0.35) -> Gray
    assert "0.40, 0.40, 0.40, 0.35" in fn_body or "0.4, 0.4, 0.4, 0.35" in fn_body, "Blocking background wash must be gray"
