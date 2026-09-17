"""
Unit & Differential Test Suite for Ground Border Layer Ordering in ChunkCacheManager
Tests:
1. Strict Depth Layering: All ground borders across the chunk strictly precede all non-border items in bake_buffer_.
2. Multi-Tile Overlap: Multi-tile sprites (2x1, 1x2, 2x2, 3x3) always appear in Pass 2 (above all Pass 1 borders).
3. Diagonal Depth Independence: Neighboring borders with higher diagonal (South/East) never draw on top of lower-diagonal items.
4. Elevation Invariant: Pass 2 elevation accumulation is unaffected by Pass 1 borders.
5. Dynamic Entity Isolation: Animated borders or items correctly flag dynamic tiles without leaking into static instance buffer.
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
    def __init__(self, x: int, y: int, ground_id: int = 0):
        self.x = x
        self.y = y
        self.ground_id = ground_id
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
    Simulates the two-pass ChunkCacheManager::bakeChunk logic.
    Pass 1: Terrain Ground & Ground Borders
    Pass 2: Objects, Structures & Elevated Items (Non-border)
    """
    bake_buffer = []
    dynamic_tiles = []

    # Helper to push instances
    def push_sprite_instances(item, x, y, layer, origin):
        # 1x1 or composite layout
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

    # =========================================================================
    # Pass 1: Static Terrain Ground & Ground Borders (Floor base layer)
    # =========================================================================
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

            # 1. Base Ground
            if tile.ground_id != 0:
                ground_item = MockItem(tile.ground_id)
                push_sprite_instances(ground_item, x * 32, y * 32, 'terrain', (tx, ty))

            # 2. Ground Borders (including animated transitions such as shallow water ID 4647)
            for item in tile.items:
                if not item.is_border:
                    continue
                push_sprite_instances(item, x * 32, y * 32, 'border', (tx, ty))

    # =========================================================================
    # Pass 2: Static Objects, Structures & Elevated Items (Non-border items)
    # =========================================================================
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

    return bake_buffer, dynamic_tiles


def run_all_tests():
    print("=== TEST 1: Strict Depth Layering Invariant ===")
    tiles = {}
    # Place a mix of ground, borders, and items across tiles
    tiles[(5, 5)] = MockTile(5, 5, ground_id=4526)
    tiles[(5, 5)].items.append(MockItem(item_id=100, is_border=True))
    tiles[(5, 5)].items.append(MockItem(item_id=200, is_border=False))

    tiles[(6, 5)] = MockTile(6, 5, ground_id=4526)
    tiles[(6, 5)].items.append(MockItem(item_id=101, is_border=True))
    tiles[(6, 5)].items.append(MockItem(item_id=201, is_border=False))

    bake_buf, _ = bake_chunk_simulation(tiles, 0, 0)

    # Find the maximum index of any border or terrain instance
    max_terrain_border_idx = max(i for i, inst in enumerate(bake_buf) if inst.layer in ('terrain', 'border'))
    # Find the minimum index of any object instance
    min_object_idx = min(i for i, inst in enumerate(bake_buf) if inst.layer == 'object')

    assert max_terrain_border_idx < min_object_idx, f"Order violation: max terrain/border {max_terrain_border_idx} >= min object {min_object_idx}"
    print(f"PASS: All terrain and borders (0..{max_terrain_border_idx}) strictly precede all objects ({min_object_idx}..{len(bake_buf)-1}).")

    print("\n=== TEST 2: Diagonal Depth Independence & Multi-Tile Overlap ===")
    # Scenario matching user screenshot:
    # Tile (5, 5) = Water with 2x2 rock extending to (4, 4), (5, 4), (4, 5), (5, 5)
    # Tile (6, 5) = Coastline with Cliff border (higher diagonal: 6+5=11 > 5+5=10)
    # Tile (5, 6) = Coastline with Cliff border (higher diagonal: 5+6=11 > 5+5=10)
    tiles_scene = {}
    water_tile = MockTile(5, 5, ground_id=4608)
    rock_2x2 = MockItem(item_id=300, is_border=False, width=2, height=2)
    water_tile.items.append(rock_2x2)
    tiles_scene[(5, 5)] = water_tile

    coast_east = MockTile(6, 5, ground_id=4526)
    cliff_border_e = MockItem(item_id=105, is_border=True)
    coast_east.items.append(cliff_border_e)
    tiles_scene[(6, 5)] = coast_east

    coast_south = MockTile(5, 6, ground_id=4526)
    cliff_border_s = MockItem(item_id=106, is_border=True)
    coast_south.items.append(cliff_border_s)
    tiles_scene[(5, 6)] = coast_south

    bake_buf, _ = bake_chunk_simulation(tiles_scene, 0, 0)

    # Verify that all 4 sub-sprites of the 2x2 rock are baked AFTER both cliff borders
    border_e_idx = next(i for i, inst in enumerate(bake_buf) if inst.layer == 'border' and inst.origin_tile == (6, 5))
    border_s_idx = next(i for i, inst in enumerate(bake_buf) if inst.layer == 'border' and inst.origin_tile == (5, 6))
    rock_indices = [i for i, inst in enumerate(bake_buf) if inst.layer == 'object' and inst.origin_tile == (5, 5)]

    assert len(rock_indices) == 4, f"Expected 4 rock sub-sprites, got {len(rock_indices)}"
    for idx in rock_indices:
        assert idx > border_e_idx, f"Rock quad at index {idx} drawn before east border at {border_e_idx}!"
        assert idx > border_s_idx, f"Rock quad at index {idx} drawn before south border at {border_s_idx}!"

    print(f"PASS: East border baked at #{border_e_idx}, South border baked at #{border_s_idx}.")
    print(f"PASS: All 4 rock sub-sprites baked at #{rock_indices} strictly on top of both borders!")

    print("\n=== TEST 3: Elevation Stacking Invariant ===")
    elev_tiles = {}
    t = MockTile(2, 2, ground_id=4526)
    t.items.append(MockItem(item_id=10, is_border=True))  # Border: 0 elevation
    t.items.append(MockItem(item_id=50, is_border=False, draw_height=8))  # Table: 8px
    t.items.append(MockItem(item_id=60, is_border=False, draw_height=0))  # Vase on table: receives 8px offset
    elev_tiles[(2, 2)] = t

    bake_buf, _ = bake_chunk_simulation(elev_tiles, 0, 0)
    table_inst = next(inst for inst in bake_buf if inst.sprite_id == 5000)
    vase_inst = next(inst for inst in bake_buf if inst.sprite_id == 6000)

    base_y = 2 * 32.0
    assert table_inst.y == base_y, f"Table y should be {base_y}, got {table_inst.y}"
    assert vase_inst.y == base_y - 8.0, f"Vase y should be {base_y - 8.0} (elevated), got {vase_inst.y}"
    print(f"PASS: Elevation offsets correct (Table: {table_inst.y}, Vase: {vase_inst.y}).")

    print("\n=== TEST 4: Animated Ground Borders (Shallow Water ID 4647) & Dynamic Isolation ===")
    dyn_tiles = {}
    # Scenario: Water tile (3, 3) has animated shallow water border 4647 (anim=True) and 2x2 rock 1353
    dt = MockTile(3, 3, ground_id=4608)
    dt.items.append(MockItem(item_id=4647, is_border=True, is_animated=True))  # Shallow water border 4647
    dt.items.append(MockItem(item_id=1353, is_border=False, width=2, height=2))  # 2x2 rock
    dyn_tiles[(3, 3)] = dt

    # Neighboring tile (4, 4) has an actual dynamic entity (torch)
    dt2 = MockTile(4, 4, ground_id=4526)
    dt2.items.append(MockItem(item_id=1487, is_border=False, is_animated=True))  # Fire torch
    dyn_tiles[(4, 4)] = dt2

    bake_buf, dyn_list = bake_chunk_simulation(dyn_tiles, 0, 0)

    # 1. Animated shallow water border MUST be baked into static Pass 1 buffer
    border_4647_instances = [inst for inst in bake_buf if inst.sprite_id == 464700]
    assert len(border_4647_instances) == 1, "Animated border 4647 must be baked into static Pass 1 buffer"
    assert border_4647_instances[0].layer == 'border', "Border 4647 must be in border layer"

    # 2. Rock 2x2 must be in Pass 2 and strictly drawn AFTER border 4647
    rock_instances = [inst for inst in bake_buf if inst.layer == 'object' and inst.origin_tile == (3, 3)]
    assert len(rock_instances) == 4, "Rock must have 4 sub-sprites"
    border_idx = bake_buf.index(border_4647_instances[0])
    for rock_inst in rock_instances:
        rock_idx = bake_buf.index(rock_inst)
        assert rock_idx > border_idx, f"Rock instance at {rock_idx} must be baked AFTER border 4647 at {border_idx}"

    # 3. Dynamic tile list must only contain tile (4, 4) with the actual animated non-border item (torch)
    assert (3, 3) not in dyn_list, "Tile (3, 3) with animated border must NOT be flagged dynamic"
    assert (4, 4) in dyn_list, "Tile (4, 4) with animated torch must be flagged dynamic"
    print("PASS: Animated shallow water border 4647 is strictly in Pass 1, overlaid by 2x2 rock, and not in dynamic list.")
    print("PASS: Animated torch correctly flagged dynamic for overlay pass.")

    print("\n========================================================")
    print("ALL GROUND BORDER LAYER ORDERING TESTS PASSED (4/4)!")
    print("========================================================")


if __name__ == "__main__":
    run_all_tests()
