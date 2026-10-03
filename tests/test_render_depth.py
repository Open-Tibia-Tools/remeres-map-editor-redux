import math

def calculate_tile_depth(map_x: int, map_y: int, sublayer: int, elevation_step: int = 0) -> float:
    k_max_index = 2097152.0  # 131072 * 16
    d = max(0, map_x + map_y)
    sub = sublayer
    if sublayer == 4:  # CommonItem
        sub += max(0, min(3, elevation_step))
    index = max(0, min(int(k_max_index), d * 16 + sub))
    return 1.0 - (float(index) + 1.0) / (k_max_index + 2.0)

class RenderSublayer:
    Ground = 0
    Border = 1
    GroundOverlay = 2
    BottomItem = 3
    CommonItem = 4
    DynamicEntity = 8
    TopItem = 10
    Overlay = 12

def run_tests():
    print("=== TEST 1: Same Tile Sublayer Monotonic Stacking ===")
    x, y = 1000, 1000
    sublayers = [
        ("Ground", RenderSublayer.Ground, 0),
        ("Border", RenderSublayer.Border, 0),
        ("GroundOverlay", RenderSublayer.GroundOverlay, 0),
        ("BottomItem", RenderSublayer.BottomItem, 0),
        ("CommonItem-0", RenderSublayer.CommonItem, 0),
        ("CommonItem-1", RenderSublayer.CommonItem, 1),
        ("DynamicEntity", RenderSublayer.DynamicEntity, 0),
        ("TopItem", RenderSublayer.TopItem, 0),
        ("Overlay", RenderSublayer.Overlay, 0)
    ]
    
    depths = []
    for name, sub, elev in sublayers:
        depth = calculate_tile_depth(x, y, sub, elev)
        depths.append((name, depth))
        print(f"  {name:15s}: depth = {depth:.8f}")

    for i in range(len(depths) - 1):
        name_curr, depth_curr = depths[i]
        name_next, depth_next = depths[i + 1]
        assert depth_next < depth_curr, f"Violation: {name_next} ({depth_next}) not strictly smaller than {name_curr} ({depth_curr})"
    print("PASS: Sublayers within tile strictly decrease in depth (GL_LEQUAL passes upwards)!")

    print("\n=== TEST 2: Mountain 919 at (x, y) vs North/West Railings/Items ===")
    # Mountain 919 is a ground tile at (x, y)
    mountain_x, mountain_y = 500, 500
    mountain_ground_depth = calculate_tile_depth(mountain_x, mountain_y, RenderSublayer.Ground)
    
    # Items from (x, y-1) [North tile] or (x-1, y) [West tile] hanging into (x, y)
    north_item_depth = calculate_tile_depth(mountain_x, mountain_y - 1, RenderSublayer.TopItem)
    west_item_depth = calculate_tile_depth(mountain_x - 1, mountain_y, RenderSublayer.TopItem)
    
    print(f"  North item depth (500, 499, TopItem): {north_item_depth:.8f}")
    print(f"  West item depth  (499, 500, TopItem): {west_item_depth:.8f}")
    print(f"  Mountain depth   (500, 500, Ground):  {mountain_ground_depth:.8f}")
    
    assert mountain_ground_depth < north_item_depth, "Mountain must have smaller depth than North item!"
    assert mountain_ground_depth < west_item_depth, "Mountain must have smaller depth than West item!"
    print("PASS: Mountain ground at (x, y) has strictly smaller depth than any North/West items!")

    print("\n=== TEST 3: Ground Overlays vs Mountain & Foreground Items ===")
    zone_overlay_depth = calculate_tile_depth(mountain_x, mountain_y - 1, RenderSublayer.GroundOverlay)
    assert zone_overlay_depth > mountain_ground_depth, "Zone overlay on tile (x, y-1) must be occluded by Mountain at (x, y)"
    print("PASS: Zone overlay behind mountain is correctly occluded by mountain!")

    print("\n=== TEST 4: Precision and Range Boundaries ===")
    d_min = calculate_tile_depth(0, 0, RenderSublayer.Ground)
    d_max = calculate_tile_depth(65535, 65535, RenderSublayer.Overlay)
    print(f"  Depth at (0, 0, Ground): {d_min:.8f}")
    print(f"  Depth at (65535, 65535, Overlay): {d_max:.8f}")
    assert 0.0 < d_max < d_min < 1.0, "All depth values must be strictly inside (0.0, 1.0)"
    print("PASS: All depth values are strictly normalized within (0.0, 1.0) with zero clipping!")

    print("\n========================================================")
    print("ALL 2.5D ISOMETRIC RENDER DEPTH TESTS PASSED (4/4)!")
    print("========================================================")

if __name__ == "__main__":
    run_tests()
