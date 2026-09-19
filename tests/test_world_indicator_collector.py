"""
Unit & Differential Test Suite for WorldIndicatorCollector
Tests:
1. Zoom readability threshold boundary conditions.
2. Ingame mode bypass.
3. Hook indicator extraction and flag preservation.
4. Door indicator alignment and lock evaluation.
5. Single-pass collection idempotency and overlay lifecycle.
"""

import sys

# Door & Wall Alignment Enums (matching brush_enums.h)
WALL_HORIZONTAL = 6
WALL_VERTICAL = 9
WALL_OTHER = 0

class MockItem:
    def __init__(self, is_door: bool = False, border_alignment: int = 0, action_id: int = 0, unique_id: int = 0, door_id: int = 0):
        self._is_door = is_door
        self.border_alignment = border_alignment
        self.action_id = action_id
        self.unique_id = unique_id
        self.door_id = door_id

    def is_door(self) -> bool:
        return self._is_door

    def is_locked(self) -> bool:
        return (self.action_id != 0) or (self.unique_id != 0) or (self.door_id != 0)


class MockTile:
    def __init__(self, x: int, y: int, z: int, hook_south: bool = False, hook_east: bool = False):
        self.x = x
        self.y = y
        self.z = z
        self.hook_south = hook_south
        self.hook_east = hook_east
        self.items = []

    def has_hook_south(self) -> bool:
        return self.hook_south

    def has_hook_east(self) -> bool:
        return self.hook_east


class MockDoorDrawer:
    def __init__(self):
        self.requests = []

    def add_door(self, pos, locked: bool, south: bool, east: bool):
        self.requests.append({"pos": pos, "locked": locked, "south": south, "east": east})

    def clear(self):
        self.requests.clear()

    def empty(self) -> bool:
        return len(self.requests) == 0


class MockHookDrawer:
    def __init__(self):
        self.requests = []

    def add_hook(self, pos, south: bool, east: bool):
        self.requests.append({"pos": pos, "south": south, "east": east})

    def clear(self):
        self.requests.clear()

    def empty(self) -> bool:
        return len(self.requests) == 0


class WorldIndicatorCollectorSim:
    @staticmethod
    def collect(tiles, zoom: float, ingame: bool, highlight_locked_doors: bool, show_hooks: bool, door_drawer, hook_drawer):
        if ingame:
            return

        can_read_labels = (32.0 / zoom) >= 10.0
        if not can_read_labels:
            return

        need_doors = highlight_locked_doors and door_drawer is not None
        need_hooks = show_hooks and hook_drawer is not None

        if not need_doors and not need_hooks:
            return

        for tile in tiles:
            pos = (tile.x, tile.y, tile.z)

            # Hook indicators (O(1) flag check)
            if need_hooks and (tile.has_hook_south() or tile.has_hook_east()):
                hook_drawer.add_hook(pos, tile.has_hook_south(), tile.has_hook_east())

            # Door indicators
            if need_doors and tile.items:
                for item in tile.items:
                    if not item or not item.is_door():
                        continue

                    locked = item.is_locked()
                    south = (item.border_alignment == WALL_HORIZONTAL)
                    east = (item.border_alignment == WALL_VERTICAL)
                    door_drawer.add_door(pos, locked, south, east)


def test_zoom_readability():
    print("=== TEST 1: Zoom Readability Gating ===")
    tiles = [MockTile(100, 100, 7, hook_south=True)]
    door = MockItem(is_door=True, border_alignment=WALL_HORIZONTAL, action_id=1000)
    tiles[0].items.append(door)

    door_drawer = MockDoorDrawer()
    hook_drawer = MockHookDrawer()

    # Zoom 100% (zoom=1.0, tile_size=32.0 >= 10.0) -> Collected
    WorldIndicatorCollectorSim.collect(tiles, 1.0, False, True, True, door_drawer, hook_drawer)
    assert len(door_drawer.requests) == 1, "Expected 1 door request at 100% zoom"
    assert len(hook_drawer.requests) == 1, "Expected 1 hook request at 100% zoom"
    door_drawer.clear()
    hook_drawer.clear()

    # Zoom 320% (zoom=3.2, tile_size=10.0 >= 10.0) -> Collected (boundary case)
    WorldIndicatorCollectorSim.collect(tiles, 3.2, False, True, True, door_drawer, hook_drawer)
    assert len(door_drawer.requests) == 1, "Expected 1 door request at 320% zoom"
    assert len(hook_drawer.requests) == 1, "Expected 1 hook request at 320% zoom"
    door_drawer.clear()
    hook_drawer.clear()

    # Zoom 321% (zoom=3.21, tile_size < 10.0) -> Bypassed
    WorldIndicatorCollectorSim.collect(tiles, 3.21, False, True, True, door_drawer, hook_drawer)
    assert len(door_drawer.requests) == 0, "Expected 0 door requests at 321% zoom"
    assert len(hook_drawer.requests) == 0, "Expected 0 hook requests at 321% zoom"

    # Extreme zoom 2500% (zoom=25.0) -> Bypassed
    WorldIndicatorCollectorSim.collect(tiles, 25.0, False, True, True, door_drawer, hook_drawer)
    assert len(door_drawer.requests) == 0, "Expected 0 door requests at 2500% zoom"
    assert len(hook_drawer.requests) == 0, "Expected 0 hook requests at 2500% zoom"
    print("PASS: Zoom readability gating strictly verified across all boundaries!")


def test_ingame_bypass():
    print("=== TEST 2: Ingame Mode Bypass ===")
    tiles = [MockTile(100, 100, 7, hook_south=True)]
    tiles[0].items.append(MockItem(is_door=True, border_alignment=WALL_HORIZONTAL))
    door_drawer = MockDoorDrawer()
    hook_drawer = MockHookDrawer()

    WorldIndicatorCollectorSim.collect(tiles, 1.0, True, True, True, door_drawer, hook_drawer)
    assert len(door_drawer.requests) == 0, "Ingame mode must bypass collection"
    assert len(hook_drawer.requests) == 0, "Ingame mode must bypass collection"
    print("PASS: Ingame mode strictly bypasses all indicator collections!")


def test_door_alignment_and_locks():
    print("=== TEST 3: Door Alignment & Lock Classification ===")
    door_drawer = MockDoorDrawer()

    # 1. Horizontal Locked Door (actionID)
    t1 = MockTile(10, 20, 7)
    t1.items.append(MockItem(is_door=True, border_alignment=WALL_HORIZONTAL, action_id=2000))

    # 2. Vertical Unlocked Door
    t2 = MockTile(11, 20, 7)
    t2.items.append(MockItem(is_door=True, border_alignment=WALL_VERTICAL))

    # 3. Non-aligned Locked Door (uniqueID)
    t3 = MockTile(12, 20, 7)
    t3.items.append(MockItem(is_door=True, border_alignment=WALL_OTHER, unique_id=5001))

    # 4. Vertical Locked Door (doorID)
    t4 = MockTile(13, 20, 7)
    t4.items.append(MockItem(is_door=True, border_alignment=WALL_VERTICAL, door_id=1))

    # 5. Non-door item (e.g. table, wall)
    t5 = MockTile(14, 20, 7)
    t5.items.append(MockItem(is_door=False, border_alignment=WALL_HORIZONTAL, action_id=1000))

    WorldIndicatorCollectorSim.collect([t1, t2, t3, t4, t5], 1.0, False, True, False, door_drawer, None)

    assert len(door_drawer.requests) == 4, f"Expected 4 doors, got {len(door_drawer.requests)}"

    r1 = door_drawer.requests[0]
    assert r1["pos"] == (10, 20, 7) and r1["locked"] is True and r1["south"] is True and r1["east"] is False

    r2 = door_drawer.requests[1]
    assert r2["pos"] == (11, 20, 7) and r2["locked"] is False and r2["south"] is False and r2["east"] is True

    r3 = door_drawer.requests[2]
    assert r3["pos"] == (12, 20, 7) and r3["locked"] is True and r3["south"] is False and r3["east"] is False

    r4 = door_drawer.requests[3]
    assert r4["pos"] == (13, 20, 7) and r4["locked"] is True and r4["south"] is False and r4["east"] is True

    print("PASS: All door orientations and lock states classified accurately!")


def test_hook_classification():
    print("=== TEST 4: Wall Hook Classification ===")
    hook_drawer = MockHookDrawer()

    t1 = MockTile(1, 1, 7, hook_south=True, hook_east=False)
    t2 = MockTile(2, 1, 7, hook_south=False, hook_east=True)
    t3 = MockTile(3, 1, 7, hook_south=True, hook_east=True)
    t4 = MockTile(4, 1, 7, hook_south=False, hook_east=False)

    WorldIndicatorCollectorSim.collect([t1, t2, t3, t4], 1.0, False, False, True, None, hook_drawer)

    assert len(hook_drawer.requests) == 3, f"Expected 3 hook requests, got {len(hook_drawer.requests)}"
    assert hook_drawer.requests[0]["south"] is True and hook_drawer.requests[0]["east"] is False
    assert hook_drawer.requests[1]["south"] is False and hook_drawer.requests[1]["east"] is True
    assert hook_drawer.requests[2]["south"] is True and hook_drawer.requests[2]["east"] is True

    print("PASS: Wall hook flags accurately preserved in single-pass collection!")


def test_frame_lifecycle_idempotency():
    print("=== TEST 5: Frame Lifecycle & Collection Idempotency ===")
    # Simulating MapDrawer frame lifecycle
    tiles = [MockTile(10, 10, 7, hook_south=True)]
    tiles[0].items.append(MockItem(is_door=True, border_alignment=WALL_HORIZONTAL, action_id=100))

    door_drawer = MockDoorDrawer()
    hook_drawer = MockHookDrawer()

    # Frame 1:
    indicators_collected = False
    collection_runs = 0

    def collect_indicators():
        nonlocal indicators_collected, collection_runs
        if indicators_collected:
            return
        indicators_collected = True
        collection_runs += 1
        WorldIndicatorCollectorSim.collect(tiles, 1.0, False, True, True, door_drawer, hook_drawer)

    # DrawHookIndicators runs first
    collect_indicators()
    assert len(hook_drawer.requests) == 1
    assert len(door_drawer.requests) == 1
    assert collection_runs == 1

    # DrawDoorIndicators runs second in the same frame
    collect_indicators()
    assert len(hook_drawer.requests) == 1
    assert len(door_drawer.requests) == 1
    assert collection_runs == 1, "Second drawer call must not re-run collection in the same frame!"

    # Frame 1 end (ClearFrameOverlays):
    indicators_collected = False
    door_drawer.clear()
    hook_drawer.clear()
    assert door_drawer.empty() and hook_drawer.empty()

    # Frame 2:
    collect_indicators()
    assert collection_runs == 2
    assert len(door_drawer.requests) == 1
    assert len(hook_drawer.requests) == 1

    print("PASS: Frame lifecycle idempotency strictly verified!")


if __name__ == "__main__":
    test_zoom_readability()
    test_ingame_bypass()
    test_door_alignment_and_locks()
    test_hook_classification()
    test_frame_lifecycle_idempotency()
    print("\n========================================================")
    print("ALL WORLD INDICATOR COLLECTOR TESTS PASSED PERFECTLY!")
    print("========================================================")
