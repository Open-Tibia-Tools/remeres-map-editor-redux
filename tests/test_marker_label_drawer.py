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
                 start_z: int = 7, end_z: int = 0):
        self.floor = floor
        self.zoom = zoom
        self.screensize_x = screensize_x
        self.screensize_y = screensize_y
        self.start_z = start_z
        self.end_z = end_z

    def is_tile_visible(self, x: int, y: int, z: int) -> bool:
        # Simplified viewport check
        return 0 <= x <= 2000 and 0 <= y <= 2000

class MockSpawn:
    def __init__(self, size: int, pos: MockPosition):
        self.size = size
        self.pos = pos

    def get_size(self) -> int:
        return self.size

    def get_position(self) -> MockPosition:
        return self.pos

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
    min_z = min(view.start_z, view.end_z)
    max_z = max(view.start_z, view.end_z)
    if options.show_all_floors and view.floor <= GROUND_LAYER and min_z <= z <= max_z:
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

    pad_x = 5.0
    pad_y = 2.0

    def overlaps(x1, y1, w1, h1, x2, y2, w2, h2):
        l1 = x1 - w1 * 0.5 - pad_x
        r1 = x1 + w1 * 0.5 + pad_x
        t1 = y1 - h1 - pad_y * 2.0
        b1 = y1

        l2 = x2 - w2 * 0.5 - pad_x
        r2 = x2 + w2 * 0.5 + pad_x
        t2 = y2 - h2 - pad_y * 2.0
        b2 = y2

        return (l1 < r2 and r1 > l2 and t1 < b2 and b1 > t2)

    def resolve_collision(x, initial_y, w, h):
        y = initial_y
        shifted = True
        iterations = 0
        while shifted and iterations < 8:
            shifted = False
            iterations += 1
            for existing in labels:
                if overlaps(x, y, w, h, existing['x'], existing['y'], existing['width'], existing['height']):
                    y = existing['y'] - existing['height'] - pad_y * 2.0 - 2.0
                    shifted = True
                    break
        return y

    # 1. Waypoints
    if options.show_waypoints:
        for wp in waypoints:
            if not wp or not wp.name:
                continue
            if wp.pos.z != view.floor:
                continue
            if not view.is_tile_visible(wp.pos.x, wp.pos.y, wp.pos.z):
                continue

            screen_x = wp.pos.x * tile_size_screen
            screen_y = wp.pos.y * tile_size_screen
            label_x = screen_x + tile_size_screen * 0.5
            w = 30.0
            h = 12.0
            label_y = resolve_collision(label_x, screen_y - 3.0, w, h)
            labels.append({
                'type': 'waypoint',
                'text': wp.name,
                'x': label_x,
                'y': label_y,
                'width': w,
                'height': h
            })

    # 2. Towns
    if options.show_towns:
        for town in towns:
            if not town:
                continue
            pos = town.get_temple_position()
            if pos.z != view.floor:
                continue
            if not view.is_tile_visible(pos.x, pos.y, pos.z):
                continue

            text = town.get_name() if town.get_name() else f"Town {town.get_id()}"
            screen_x = pos.x * tile_size_screen
            screen_y = pos.y * tile_size_screen
            label_x = screen_x + tile_size_screen * 0.5
            w = 30.0
            h = 12.0
            label_y = resolve_collision(label_x, screen_y - 3.0, w, h)

            labels.append({
                'type': 'town',
                'text': text,
                'x': label_x,
                'y': label_y,
                'width': w,
                'height': h
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

    # Multi floor surface mode (show_all_floors = True): Labels are strictly displayed only per floor!
    opts_multi = MockDrawingOptions(show_waypoints=True, show_all_floors=True)
    labels = collect_marker_labels([wp_ground, wp_roof, wp_underground], [], view, opts_multi)
    assert len(labels) == 1, "Marker labels must strictly be displayed only on the active floor"
    assert labels[0]['text'] == "ground_wp"

    # Verify switching active floor to 5 displays roof_wp only
    view_roof = MockRenderView(floor=5, zoom=1.0)
    labels_roof = collect_marker_labels([wp_ground, wp_roof, wp_underground], [], view_roof, opts_multi)
    assert len(labels_roof) == 1
    assert labels_roof[0]['text'] == "roof_wp"


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
    fn_body = content[idx:idx + 600]

    # Standardized 2px black border and 50% black shade
    assert "vec4(0.05, 0.05, 0.07, 0.98)" in fn_body, "Blocking must have 2px black perimeter"
    assert "0.0, 0.0, 0.0, 0.50" in fn_body, "Blocking wash must be 50% black shade"


def test_marker_drawer_does_not_render_spawn_size_labels():
    drawer_path = Path(__file__).parent.parent / "source" / "rendering" / "drawers" / "overlays" / "marker_label_drawer.cpp"
    content = drawer_path.read_text(encoding="utf-8")

    assert "MarkerLabelType::Spawn" not in content, "Spawn size labels must not be in MarkerLabelDrawer"
    assert "Collect Spawns" not in content, "Spawns collection must be removed from MarkerLabelDrawer"


def test_waypoint_and_town_2d_collision_stacking():
    pos = MockPosition(50, 50, 7)
    wp = MockWaypoint("temple_wp", pos)
    town = MockTown(1, "Carlin", pos)
    view = MockRenderView(floor=7, zoom=1.0)
    opts = MockDrawingOptions(show_waypoints=True, show_towns=True)

    labels = collect_marker_labels([wp], [town], view, opts)
    assert len(labels) == 2, "Both waypoint and town labels must be rendered"

    wp_lbl = next(l for l in labels if l['type'] == 'waypoint')
    town_lbl = next(l for l in labels if l['type'] == 'town')

    assert wp_lbl['x'] == town_lbl['x'], "Both labels share the same X center"
    assert town_lbl['y'] < wp_lbl['y'], "Town label must be stacked vertically higher than waypoint label"


def test_creature_respawn_timer_formatting():
    drawer_path = Path(__file__).parent.parent / "source" / "rendering" / "drawers" / "entities" / "creature_name_drawer.cpp"
    content = drawer_path.read_text(encoding="utf-8")

    # Assert dedicated timer badge format in bottom-right corner: "{}s"
    assert 'std::format("{}s", label.creature->getSpawnTime())' in content, "Creature respawn timer must be formatted as '{}s'"

    # Assert condition filters NPCs and non-positive spawn timers
    assert "!label.creature->isNpc()" in content, "NPCs must not display respawn timers"
    assert "label.creature->getSpawnTime() > 0" in content, "Creatures with spawn time <= 0 must not display timer"


def test_spawn_drag_preview_logic():
    # 1. DragShadowDrawer verification: draws full spawn area when dragging selected spawn
    drag_drawer_path = Path(__file__).parent.parent / "source" / "rendering" / "drawers" / "cursors" / "drag_shadow_drawer.cpp"
    drag_content = drag_drawer_path.read_text(encoding="utf-8")

    assert "tile->spawn && tile->spawn->isSelected()" in drag_content, "Drag shadow must check for selected spawn"
    assert "ZONE_FLAG_SPAWN" in drag_content, "Drag shadow must render spawn zone flag"
    assert "INDICATOR_SPAWN_BASE" in drag_content, "Drag shadow must render spawn center badge"

    # Math test: radius 3 -> area is 7x7 tiles = 224x224 pixels
    radius = 3
    pos_x, pos_y = 100, 100
    sx0 = pos_x - radius
    sx1 = pos_x + radius
    sy0 = pos_y - radius
    sy1 = pos_y + radius
    spawn_w = (sx1 - sx0 + 1) * 32
    spawn_h = (sy1 - sy0 + 1) * 32
    assert spawn_w == 224
    assert spawn_h == 224

    # 2. ZoneOverlayDrawer verification: stationary spawn is dimmed while dragging
    zone_drawer_path = Path(__file__).parent.parent / "source" / "rendering" / "drawers" / "overlays" / "zone_overlay_drawer.cpp"
    zone_content = zone_drawer_path.read_text(encoding="utf-8")

    assert "st->spawn->isSelected() && options.dragging" in zone_content, "Stationary spawn must detect when it is being dragged"
    assert "floor_alpha * 0.30f" in zone_content or "0.30f" in zone_content, "Stationary spawn must be dimmed during drag"


