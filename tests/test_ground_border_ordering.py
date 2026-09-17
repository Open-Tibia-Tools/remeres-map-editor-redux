"""
Unit & Differential Test Suite for Ground Border Layer Ordering in ChunkCacheManager
Tests:
1. Multi-Tile Ground Occlusion (Painter's Algorithm): Multi-tile grounds (e.g. 2x2 mountain ID 919) at (x+1, y+1)
   strictly occlude objects placed on tiles behind them (x, y).
2. Multi-Tile Item Overlap: Multi-tile sprites (e.g. 2x2 rock) at (x+1, y+1) or on the same tile
   strictly render on top of ground borders placed before them (x, y).
3. Elevation Stacking Invariant: Static items correctly accumulate elevation offsets.
4. Animated Terrain & Dynamic Isolation: Animated water grounds and borders are baked into the static chunk VBO,
   flag has_animated_terrain=True for periodic re-bake, and do NOT leak into dynamic overlay traversal.
"""

import sys

CHUNK_SIZE = 16

class MockItem:
    def __init__(self, item_id: int, is_border: bool = False, is_animated: bool = False, draw_height: int = 0, width: int = 1, height: int = 1):
        self.item_id = item_id
        self.is_border = is_border
        self.is_animated = is_animated
        self.draw_height = draw_height
        self.width = width
        self.height = height

    def has_elevation(self) -> bool:
        return self.draw_height > 0


class MockTile:
    def __init__(self, x: int, y: int, ground_id: int = 0, ground_width: int = 1, ground_height: int = 1, ground_animated: bool = False):
        self.x = x
        self.y = y
        self.ground_id = ground_id
        self.ground_width = ground_width
        self.ground_height = ground_height
        self.ground_animated = ground_animated
        self.items = []
        self.is_modified = True


class MockTileInstance:
    def __init__(self, x: float, y: float, w: float, h: float, sprite_id: int, layer: str, origin_tile: tuple):
        self.x = x
        self.y = y
        self.w = w
        self.h = h
        self.sprite_id = sprite_id
        self.layer = layer  # 'terrain', 'border', 'object'
        self.origin_tile = origin_tile


def bake_chunk_simulation(tiles: dict, base_x: int, base_y: int):
    """
    Simulates the single diagonal loop ChunkCacheManager::bakeChunk logic.
    Traverses tiles in strict diagonal Painter's Algorithm order (North-West to South-East).
    On each tile:
      1. Terrain Ground (static / animated)
      2. Ground Borders (static / animated)
      3. Static Objects & Structures with Elevation
    """
    bake_buffer = []
    dynamic_tiles = []
    has_animated_terrain = False

    # Helper to push instances
    def push_sprite_instances(item, x, y, layer, origin):
        num_cols = item.width
        num_rows = item.height
        x_offset = 0
        for cx in range(num_cols):
            y_offset = 0
            for cy in range(num_rows):
                inst = MockTileInstance(
                    x=float(x - x_offset),
                    y=float(y - y_offset),
                    w=32.0,
                    h=32.0,
                    sprite_id=item.item_id * 100 + cx * 10 + cy,
                    layer=layer,
                    origin_tile=origin
                )
                bake_buffer.append(inst)
                y_offset += 32
            x_offset += 32

    # Single Diagonal Loop
    for d in range(2 * CHUNK_SIZE - 1):
        for tx in range(min(d + 1, CHUNK_SIZE)):
            ty = d - tx
            if ty >= CHUNK_SIZE:
                continue

            tile = tiles.get((tx, ty))
            if not tile:
                continue

            x = base_x + tx
            y = base_y + ty
            is_dynamic = False

            # 1. Terrain Ground
            if tile.ground_id != 0:
                ground_item = MockItem(
                    item_id=tile.ground_id,
                    width=tile.ground_width,
                    height=tile.ground_height,
                    is_animated=tile.ground_animated
                )
                if ground_item.is_animated:
                    has_animated_terrain = True
                push_sprite_instances(ground_item, x * 32, y * 32, 'terrain', (tx, ty))

            # 2. Ground Borders
            for item in tile.items:
                if not item.is_border:
                    continue
                if item.is_animated:
                    has_animated_terrain = True
                push_sprite_instances(item, x * 32, y * 32, 'border', (tx, ty))

            # 3. Static Objects & Structures with Elevation
            elev = 0
            for item in tile.items:
                if item.is_border:
                    continue

                if item.is_animated:
                    is_dynamic = True
                    if item.has_elevation():
                        elev += item.draw_height
                    continue

                item_x = x * 32 - elev
                item_y = y * 32 - elev

                push_sprite_instances(item, item_x, item_y, 'object', (tx, ty))

                if item.has_elevation():
                    elev += item.draw_height

            if is_dynamic:
                dynamic_tiles.append((tx, ty))

    return bake_buffer, dynamic_tiles, has_animated_terrain


def run_all_tests():
    print("=== TEST 1: Multi-Tile Ground Occlusion (Mountain 919 vs Boxes) ===")
    # Scenario matching Issue 2:
    # Tile (1, 1) has dirt ground and wooden boxes (static non-border item)
    # Tile (2, 2) has 2x2 mountain ground (ID 919, width=2, height=2)
    # Diagonal sum for (1, 1) = 2. Diagonal sum for (2, 2) = 4.
    # Because (2, 2) > (1, 1), the mountain ground must be baked AFTER the box on (1, 1),
    # properly occluding the box behind the mountain peak!
    tiles_m = {}
    dirt_tile = MockTile(1, 1, ground_id=103)
    dirt_tile.items.append(MockItem(item_id=200, is_border=False))  # Wooden box
    tiles_m[(1, 1)] = dirt_tile

    mountain_tile = MockTile(2, 2, ground_id=919, ground_width=2, ground_height=2)
    tiles_m[(2, 2)] = mountain_tile

    bake_buf, _, _ = bake_chunk_simulation(tiles_m, 0, 0)

    box_inst = next(inst for inst in bake_buf if inst.layer == 'object' and inst.origin_tile == (1, 1))
    mountain_instances = [inst for inst in bake_buf if inst.layer == 'terrain' and inst.origin_tile == (2, 2)]

    assert len(mountain_instances) == 4, f"Expected 4 mountain sub-sprites, got {len(mountain_instances)}"
    box_idx = bake_buf.index(box_inst)
    for m_inst in mountain_instances:
        m_idx = bake_buf.index(m_inst)
        assert m_idx > box_idx, f"Mountain sub-sprite at {m_idx} was baked BEFORE box at {box_idx}!"

    print(f"PASS: Box baked at #{box_idx}.")
    print(f"PASS: All 4 mountain ground sub-sprites baked at {[bake_buf.index(m) for m in mountain_instances]} strictly on top of box!")

    print("\n=== TEST 2: Multi-Tile Item Overlap over Ground Borders (2x2 Rock vs Shallow Water) ===")
    # Scenario:
    # Tile (1, 2) = Water with Shallow Water border 4647 (diagonal = 3)
    # Tile (2, 2) = Water with 2x2 Rock 1353 (diagonal = 4) extending to (1, 1), (2, 1), (1, 2), (2, 2)
    tiles_scene = {}
    water_border_tile = MockTile(1, 2, ground_id=4608)
    water_border_tile.items.append(MockItem(item_id=4647, is_border=True))  # Shallow water border
    tiles_scene[(1, 2)] = water_border_tile

    rock_tile = MockTile(2, 2, ground_id=4608)
    rock_tile.items.append(MockItem(item_id=1353, is_border=False, width=2, height=2))  # 2x2 Rock
    tiles_scene[(2, 2)] = rock_tile

    bake_buf, _, _ = bake_chunk_simulation(tiles_scene, 0, 0)

    border_idx = next(i for i, inst in enumerate(bake_buf) if inst.layer == 'border' and inst.origin_tile == (1, 2))
    rock_indices = [i for i, inst in enumerate(bake_buf) if inst.layer == 'object' and inst.origin_tile == (2, 2)]

    assert len(rock_indices) == 4, f"Expected 4 rock sub-sprites, got {len(rock_indices)}"
    for idx in rock_indices:
        assert idx > border_idx, f"Rock quad at index {idx} drawn before border at {border_idx}!"

    print(f"PASS: Shallow water border baked at #{border_idx}.")
    print(f"PASS: All 4 rock sub-sprites baked at #{rock_indices} strictly on top of border!")

    print("\n=== TEST 3: Elevation Stacking Invariant ===")
    elev_tiles = {}
    t = MockTile(2, 2, ground_id=4526)
    t.items.append(MockItem(item_id=10, is_border=True))  # Border: 0 elevation
    t.items.append(MockItem(item_id=50, is_border=False, draw_height=8))  # Table: 8px
    t.items.append(MockItem(item_id=60, is_border=False, draw_height=0))  # Vase on table: receives 8px offset
    elev_tiles[(2, 2)] = t

    bake_buf, _, _ = bake_chunk_simulation(elev_tiles, 0, 0)
    table_inst = next(inst for inst in bake_buf if inst.sprite_id == 5000)
    vase_inst = next(inst for inst in bake_buf if inst.sprite_id == 6000)

    base_y = 2 * 32.0
    assert table_inst.y == base_y, f"Table y should be {base_y}, got {table_inst.y}"
    assert vase_inst.y == base_y - 8.0, f"Vase y should be {base_y - 8.0} (elevated), got {vase_inst.y}"
    print(f"PASS: Elevation offsets correct (Table: {table_inst.y}, Vase: {vase_inst.y}).")

    print("\n=== TEST 4: Animated Terrain & Dynamic Isolation ===")
    dyn_tiles = {}
    # Scenario: Water tile (3, 3) has animated shallow water border 4647 (anim=True) and 2x2 rock 1353
    dt = MockTile(3, 3, ground_id=4608, ground_animated=True)
    dt.items.append(MockItem(item_id=4647, is_border=True, is_animated=True))  # Shallow water border 4647
    dt.items.append(MockItem(item_id=1353, is_border=False, width=2, height=2))  # 2x2 rock
    dyn_tiles[(3, 3)] = dt

    # Neighboring tile (4, 4) has an actual dynamic entity (torch)
    dt2 = MockTile(4, 4, ground_id=4526)
    dt2.items.append(MockItem(item_id=1487, is_border=False, is_animated=True))  # Fire torch
    dyn_tiles[(4, 4)] = dt2

    bake_buf, dyn_list, has_anim_terrain = bake_chunk_simulation(dyn_tiles, 0, 0)

    # 1. Animated terrain flag must be True
    assert has_anim_terrain is True, "Chunk with water ground and border 4647 must set has_animated_terrain=True"

    # 2. Animated shallow water border MUST be baked into static buffer
    border_4647_instances = [inst for inst in bake_buf if inst.sprite_id == 464700]
    assert len(border_4647_instances) == 1, "Animated border 4647 must be baked into static buffer"
    assert border_4647_instances[0].layer == 'border', "Border 4647 must be in border layer"

    # 3. Rock 2x2 must be strictly drawn AFTER border 4647
    rock_instances = [inst for inst in bake_buf if inst.layer == 'object' and inst.origin_tile == (3, 3)]
    assert len(rock_instances) == 4, "Rock must have 4 sub-sprites"
    b_idx = bake_buf.index(border_4647_instances[0])
    for rock_inst in rock_instances:
        r_idx = bake_buf.index(rock_inst)
        assert r_idx > b_idx, f"Rock instance at {r_idx} must be baked AFTER border 4647 at {b_idx}"

    # 4. Dynamic tile list must only contain tile (4, 4) with the actual animated non-border item (torch)
    assert (3, 3) not in dyn_list, "Tile (3, 3) with animated border must NOT be flagged dynamic"
    assert (4, 4) in dyn_list, "Tile (4, 4) with animated torch must be flagged dynamic"
    print("PASS: has_animated_terrain is True, allowing periodic re-bake without overlay overhead.")
    print("PASS: Animated shallow water border 4647 is in static VBO, overlaid by 2x2 rock, and not in dynamic list.")
    print("PASS: Animated torch correctly flagged dynamic for overlay pass.")

    print("\n========================================================")
    print("ALL GROUND BORDER LAYER ORDERING TESTS PASSED (4/4)!")
    print("========================================================")


if __name__ == "__main__":
    run_all_tests()

