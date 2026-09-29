#!/usr/bin/env python3
"""
RME Redux - 4 Zones with CipSoft Lined Square Prototype
Generates comprehensive visual comparisons of the 4 Special Zones:
1. Protection Zone (PZ - Yellow)
2. No-PvP Zone (Green)
3. No-Logout Zone (Orange)
4. PvP Zone (Crimson Red)

Comparing:
- Current RME (28% solid wash + solid border)
- CipSoft Lined Square (Corners + mid dashes per tile, zero wash)
- CipSoft Lined Square + Subtle 8% tint
- Multi-tile contiguous areas (3x3 room)
- Full in-game test scene
"""

from __future__ import annotations
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np


# The 4 Zone Colors:
ZONE_COLORS = {
    "PZ": {
        "name": "Protection Zone (PZ)",
        "core": (250, 240, 110),       # Pale Lemon / Yellow (matches CipSoft screenshot)
        "wash": (242, 217, 26),        # RME wash
        "glsl": "vec4(0.95, 0.85, 0.10, 0.28)",
        "label": "PZ"
    },
    "NOPVP": {
        "name": "No-PvP Zone",
        "core": (90, 245, 115),        # Emerald Green
        "wash": (38, 230, 51),
        "glsl": "vec4(0.15, 0.90, 0.20, 0.28)",
        "label": "NO-PVP"
    },
    "NOLOG": {
        "name": "No-Logout Zone",
        "core": (255, 155, 45),        # Vibrant Orange
        "wash": (255, 128, 13),
        "glsl": "vec4(1.00, 0.50, 0.05, 0.28)",
        "label": "NO-LOGOUT"
    },
    "PVP": {
        "name": "PvP Zone (Hardcore)",
        "core": (255, 55, 80),         # Crimson Red
        "wash": (217, 13, 64),
        "glsl": "vec4(0.85, 0.05, 0.25, 0.28)",
        "label": "PVP"
    },
}


def draw_cipsoft_tile_bracket(tile: Image.Image,
                              color: tuple,
                              has_shadow: bool = True,
                              line_w: int = 2,
                              margin: int = 2) -> Image.Image:
    """Renders the authentic CipSoft corner bracket + mid dash on a single tile."""
    out = tile.copy().convert('RGBA')
    overlay = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    
    w, h = out.size
    x0, y0 = margin, margin
    x1, y1 = w - 1 - margin, h - 1 - margin
    
    c_len = max(3, int(w * 0.22))
    m_len = max(3, int(w * 0.24))
    
    mx0 = (w - m_len) // 2
    mx1 = mx0 + m_len
    my0 = (h - m_len) // 2
    my1 = my0 + m_len
    
    segments = [
        # Top-left corner
        ((x0, y0), (x0 + c_len, y0)),
        ((x0, y0), (x0, y0 + c_len)),
        # Top-right corner
        ((x1 - c_len, y0), (x1, y0)),
        ((x1, y0), (x1, y0 + c_len)),
        # Bottom-left corner
        ((x0, y1), (x0 + c_len, y1)),
        ((x0, y1 - c_len), (x0, y1)),
        # Bottom-right corner
        ((x1 - c_len, y1), (x1, y1)),
        ((x1, y1 - c_len), (x1, y1)),
        # Midpoint dashes
        ((mx0, y0), (mx1, y0)),
        ((mx0, y1), (mx1, y1)),
        ((x0, my0), (x0, my1)),
        ((x1, my0), (x1, my1)),
    ]
    
    # 1. Dark drop-shadow / outline
    if has_shadow:
        for (sx0, sy0), (sx1, sy1) in segments:
            draw.line([(sx0, sy0), (sx1, sy1)], fill=(15, 15, 18, 240), width=line_w + 2)
            
    # 2. Bright colored core
    for (sx0, sy0), (sx1, sy1) in segments:
        draw.line([(sx0, sy0), (sx1, sy1)], fill=color + (255,), width=line_w)
        
    return Image.alpha_composite(out, overlay)


def draw_rme_zone_tile(tile: Image.Image, wash_color: tuple, wash_alpha: float = 0.28, border_color: tuple = None) -> Image.Image:
    """Simulates RME's solid wash and 1px border."""
    out = tile.copy().convert('RGBA')
    overlay = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    w, h = out.size
    
    a_int = int(wash_alpha * 255)
    draw.rectangle([1, 1, w - 2, h - 2], fill=wash_color + (a_int,))
    bcol = border_color if border_color else wash_color
    draw.rectangle([0, 0, w - 1, h - 1], outline=bcol + (240,), width=1)
    return Image.alpha_composite(out, overlay)


def create_stone_tile_with_item(item_type: str = "chest", size: int = 64) -> Image.Image:
    """Creates a stone tile with a book, chest, or stair sprite."""
    np.random.seed(42 if item_type == "chest" else (77 if item_type == "book" else 99))
    base = np.full((size, size, 3), 132, dtype=np.float32)
    noise = np.random.normal(0, 6, (size, size, 3))
    base = np.clip(base + noise, 105, 160)
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size - 1, size - 1], outline=(100, 100, 105), width=1)
    
    cx, cy = size // 2, size // 2
    if item_type == "book":
        # Draw open tome (like CipSoft screenshot)
        bw, bh = int(size * 0.55), int(size * 0.52)
        bx0, by0 = cx - bw // 2, cy - bh // 2
        # Pages & spine
        draw.polygon([(bx0 + 4, by0 + bh), (cx, by0 + bh - 4), (bx0 + bw - 4, by0 + bh), (cx, by0 + 4)], fill=(210, 205, 190), outline=(50, 45, 40))
        # Brown leather covers
        draw.polygon([(bx0, by0 + 8), (cx - 2, by0 + 4), (cx - 2, by0 + bh - 2), (bx0, by0 + bh - 6)], fill=(120, 75, 45), outline=(40, 25, 15))
        draw.polygon([(cx + 2, by0 + 4), (bx0 + bw, by0 + 8), (bx0 + bw, by0 + bh - 6), (cx + 2, by0 + bh - 2)], fill=(135, 85, 50), outline=(40, 25, 15))
        # Red ribbon at top
        draw.line([(cx - 1, by0 - 4), (cx + 3, by0 + 6)], fill=(210, 40, 50), width=2)
        # Gold emblem on cover
        draw.ellipse([cx + 8, cy - 2, cx + 18, cy + 8], fill=(225, 180, 40), outline=(60, 45, 10), width=1)
    elif item_type == "chest":
        bw, bh = int(size * 0.62), int(size * 0.48)
        bx0, by0 = cx - bw // 2, cy - bh // 2 + 2
        draw.rectangle([bx0, by0 + 6, bx0 + bw, by0 + bh], fill=(135, 80, 40), outline=(45, 25, 10), width=1)
        draw.polygon([(bx0, by0 + 6), (bx0 + 4, by0), (bx0 + bw - 4, by0), (bx0 + bw, by0 + 6)], fill=(160, 100, 50), outline=(45, 25, 10))
        draw.rectangle([cx - 4, by0 + 7, cx + 4, by0 + 17], fill=(240, 200, 50), outline=(60, 45, 10), width=1)
    elif item_type == "stairs":
        tiers = 3
        th = size // (tiers + 1)
        for i in range(tiers):
            y = (i + 1) * th
            draw.rectangle([4, y, size - 4, y + 4], fill=(100 - i * 15, 100 - i * 15, 100 - i * 15), outline=(40, 40, 40), width=1)
            draw.rectangle([4, y + 4, size - 4, min(size - 2, y + th)], fill=(155 - i * 15, 155 - i * 15, 155 - i * 15), outline=(50, 50, 50), width=1)
    return img


def build_zone_study_board() -> Image.Image:
    tile_sz = 84
    canvas_w = 1120
    canvas_h = 1080
    board = Image.new('RGB', (canvas_w, canvas_h), (22, 22, 26))
    draw = ImageDraw.Draw(board)
    
    try:
        title_font = ImageFont.truetype("arialbd.ttf", 22)
        h2_font    = ImageFont.truetype("arialbd.ttf", 15)
        body_font  = ImageFont.truetype("arial.ttf", 12)
        bold_font  = ImageFont.truetype("arialbd.ttf", 12)
        mono_font  = ImageFont.truetype("consola.ttf", 11)
    except IOError:
        title_font = ImageFont.load_default()
        h2_font = title_font
        body_font = title_font
        bold_font = title_font
        mono_font = title_font

    # Banner
    draw.rectangle([0, 0, canvas_w, 75], fill=(30, 30, 36))
    draw.line([(0, 75), (canvas_w, 75)], fill=(48, 48, 56), width=1)
    draw.text((30, 16), "RME Redux — The 4 Special Zones: CipSoft Lined Squares vs. RME Wash", font=title_font, fill=(255, 255, 255))
    draw.text((30, 46), "Comparing how Protection Zone, No-PvP, No-Logout, and PvP Zone look with CipSoft's lined square markers", font=body_font, fill=(160, 165, 175))

    # Helper to place tile
    def place(tile: Image.Image, x: int, y: int, top_text: str = "", bottom_text: str = ""):
        board.paste(tile, (x, y))
        draw.rectangle([x - 1, y - 1, x + tile.width, y + tile.height], outline=(60, 60, 70), width=1)
        if top_text:
            draw.text((x + tile.width // 2, y - 16), top_text, font=bold_font, fill=(210, 210, 220), anchor="mt")
        if bottom_text:
            draw.text((x + tile.width // 2, y + tile.height + 5), bottom_text, font=mono_font, fill=(140, 145, 155), anchor="mt")

    # =========================================================================
    # PART 1: The 4 Zones on Single Tiles (Side-by-Side: Current RME vs CipSoft)
    # =========================================================================
    sec1_y = 105
    draw.text((30, sec1_y - 20), "1. The 4 Special Zones: Current RME Wash vs. CipSoft Lined Square", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec1_y + 4), (canvas_w - 30, sec1_y + 4)], fill=(45, 45, 52), width=1)

    zones_list = ["PZ", "NOPVP", "NOLOG", "PVP"]
    items_list = ["book", "chest", "stairs", "chest"]
    
    col_w = 250
    start_x = 55
    y_rme = sec1_y + 40
    y_cip = sec1_y + 165

    draw.text((30, y_rme + 30), "Current RME Style\n(28% Opaque Wash)", font=bold_font, fill=(240, 140, 140))
    draw.text((30, y_cip + 30), "CipSoft Style\n(Lined Square)", font=bold_font, fill=(130, 240, 150))

    for idx, z_key in enumerate(zones_list):
        z = ZONE_COLORS[z_key]
        item_t = items_list[idx]
        base_t = create_stone_tile_with_item(item_t, tile_sz)
        
        # Current RME: 28% wash + border
        rme_t = draw_rme_zone_tile(base_t, z["wash"], 0.28)
        
        # CipSoft: unshaded tile + colored lined square
        cip_t = draw_cipsoft_tile_bracket(base_t, z["core"], has_shadow=True, line_w=2)
        
        tx = 190 + idx * col_w
        place(rme_t, tx, y_rme, z["name"], "Wash masks sprite colors")
        place(cip_t, tx, y_cip, "", "100% crisp book/chest")

    # Callout
    callout1_y = sec1_y + 275
    draw.rectangle([30, callout1_y, canvas_w - 30, callout1_y + 45], fill=(30, 32, 40), outline=(50, 55, 70), width=1)
    draw.text((45, callout1_y + 8), "Visual Comparison:", font=bold_font, fill=(90, 180, 255))
    draw.text((180, callout1_y + 8), "Under RME's wash, the brown book cover turns greenish/yellowish, and gold locks turn muddy.", font=body_font, fill=(220, 225, 230))
    draw.text((45, callout1_y + 25), "Under CipSoft's lined square, the book and chest retain 100% of their natural pixel art colors.", font=body_font, fill=(160, 165, 175))

    # =========================================================================
    # PART 2: Contiguous Multi-Tile Rooms (3x3 Protection Zone Temple/Depot)
    # =========================================================================
    sec2_y = 445
    draw.text((30, sec2_y), "2. Contiguous Zone Area: 3x3 Protection Zone (Depot / Temple Room)", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec2_y + 24), (canvas_w - 30, sec2_y + 24)], fill=(45, 45, 52), width=1)

    # Build a 3x3 base room (stone floor, 1 chest, 1 book, 1 stairs)
    room_sz = 3 * 64
    base_room = Image.new('RGB', (room_sz, room_sz))
    for ry in range(3):
        for rx in range(3):
            # Pick item
            if rx == 1 and ry == 1:
                t = create_stone_tile_with_item("book", 64)
            elif rx == 0 and ry == 0:
                t = create_stone_tile_with_item("chest", 64)
            elif rx == 2 and ry == 2:
                t = create_stone_tile_with_item("stairs", 64)
            else:
                t = create_stone_tile_with_item("none", 64)
            base_room.paste(t, (rx * 64, ry * 64))

    # Mode 1: Current RME (Full 3x3 yellow wash + 1px yellow outer border)
    room_rme = base_room.copy()
    rme_over = Image.new('RGBA', (room_sz, room_sz), (0, 0, 0, 0))
    odraw = ImageDraw.Draw(rme_over)
    odraw.rectangle([1, 1, room_sz - 2, room_sz - 2], fill=(242, 217, 26, 70))
    odraw.rectangle([0, 0, room_sz - 1, room_sz - 1], outline=(255, 230, 26, 240), width=1)
    room_rme = Image.alpha_composite(room_rme.convert('RGBA'), rme_over)

    # Mode 2: CipSoft Per-Tile Lined Squares (Every tile in the 3x3 has its own yellow brackets)
    room_cip_tiles = base_room.copy().convert('RGBA')
    for ry in range(3):
        for rx in range(3):
            sub_t = base_room.crop((rx * 64, ry * 64, (rx + 1) * 64, (ry + 1) * 64))
            sub_bracket = draw_cipsoft_tile_bracket(sub_t, ZONE_COLORS["PZ"]["core"], line_w=2)
            room_cip_tiles.paste(sub_bracket, (rx * 64, ry * 64))

    # Mode 3: CipSoft Hybrid (Lined Square Outer Perimeter + Clean Interior)
    room_cip_perimeter = base_room.copy().convert('RGBA')
    # Draw dashed yellow outer border with shadow
    perim_over = Image.new('RGBA', (room_sz, room_sz), (0, 0, 0, 0))
    pdraw = ImageDraw.Draw(perim_over)
    # Dashed outer box
    dash_len = 14
    gap_len = 10
    # North & South
    for x in range(2, room_sz - 2, dash_len + gap_len):
        x_end = min(room_sz - 2, x + dash_len)
        pdraw.line([(x, 2), (x_end, 2)], fill=(15, 15, 18, 240), width=4)
        pdraw.line([(x, 2), (x_end, 2)], fill=(250, 240, 110, 255), width=2)
        pdraw.line([(x, room_sz - 3), (x_end, room_sz - 3)], fill=(15, 15, 18, 240), width=4)
        pdraw.line([(x, room_sz - 3), (x_end, room_sz - 3)], fill=(250, 240, 110, 255), width=2)
    # West & East
    for y in range(2, room_sz - 2, dash_len + gap_len):
        y_end = min(room_sz - 2, y + dash_len)
        pdraw.line([(2, y), (2, y_end)], fill=(15, 15, 18, 240), width=4)
        pdraw.line([(2, y), (2, y_end)], fill=(250, 240, 110, 255), width=2)
        pdraw.line([(room_sz - 3, y), (room_sz - 3, y_end)], fill=(15, 15, 18, 240), width=4)
        pdraw.line([(room_sz - 3, y), (room_sz - 3, y_end)], fill=(250, 240, 110, 255), width=2)
    room_cip_perimeter = Image.alpha_composite(room_cip_perimeter, perim_over)

    # Place the 3 rooms
    rx_col1 = 60
    rx_col2 = 410
    rx_col3 = 760
    ry_pos = sec2_y + 40

    place(room_rme, rx_col1, ry_pos, "Current RME Style", "Solid yellow wash tints entire room")
    place(room_cip_tiles, rx_col2, ry_pos, "CipSoft Style: Per-Tile Brackets", "Each PZ tile has yellow brackets")
    place(room_cip_perimeter, rx_col3, ry_pos, "CipSoft Hybrid: Perimeter Dashes", "Dashed boundary, clean room interior")

    # =========================================================================
    # PART 3: The 4 Zones on Real In-Game Map Scene
    # =========================================================================
    sec3_y = 730
    draw.text((30, sec3_y), "3. The 4 Special Zones Rendered on Real In-Game Map (Columns 4 to 7)", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec3_y + 24), (canvas_w - 30, sec3_y + 24)], fill=(45, 45, 52), width=1)

    # Load clean scene and crop the 4 zone columns:
    # Cols 4, 5, 6, 7: x in [330, 605], y in [195, 365] (170 high, 275 wide)
    scene_path = Path("tools/assets/scene_clean.png")
    if scene_path.exists():
        scene_img = Image.open(scene_path).convert('RGBA')
        crop_box = (335, 195, 600, 365)
        crop_scene = scene_img.crop(crop_box)
        cw, ch = crop_scene.size

        # Render Left: Current RME Washes (No-PvP Green, PZ Yellow, No-Log Orange, PvP Red)
        left_scene = crop_scene.copy()
        l_arr = np.array(left_scene, dtype=float) / 255.0

        def blend_col(arr, x0, x1, y0, y1, zw, zb):
            arr[y0:y1, x0:x1, :3] = arr[y0:y1, x0:x1, :3] * (1 - zw[3]) + zw[:3] * zw[3]
            arr[y0, x0:x1, :3] = zb[:3]
            arr[y1-1, x0:x1, :3] = zb[:3]
            arr[y0:y1, x0, :3] = zb[:3]
            arr[y0:y1, x1-1, :3] = zb[:3]

        # Coordinates relative to crop (crop_box: x0=335, y0=195)
        # Col 4 No-PvP: [348-335, 382-335, 213-195, 349-195] = [13, 47, 18, 154]
        # Col 5 PZ:     [416-335, 450-335, 213-195, 349-195] = [81, 115, 18, 154]
        # Col 6 No-Log: [484-335, 518-335, 213-195, 349-195] = [149, 183, 18, 154]
        # Col 7 PvP:    [552-335, 586-335, 213-195, 349-195] = [217, 251, 18, 154]
        blend_col(l_arr, 13, 47, 18, 154, np.array([0.15, 0.90, 0.20, 0.28]), np.array([0.20, 1.00, 0.30, 0.95]))
        blend_col(l_arr, 81, 115, 18, 154, np.array([0.95, 0.85, 0.10, 0.28]), np.array([1.00, 0.90, 0.10, 0.95]))
        blend_col(l_arr, 149, 183, 18, 154, np.array([1.00, 0.50, 0.05, 0.28]), np.array([1.00, 0.55, 0.10, 0.95]))
        blend_col(l_arr, 217, 251, 18, 154, np.array([0.85, 0.05, 0.25, 0.28]), np.array([1.00, 0.15, 0.30, 0.95]))
        left_img = Image.fromarray(np.clip(l_arr * 255.0, 0, 255).astype(np.uint8))

        # Render Right: CipSoft Style Lined Squares on the 4 Columns!
        right_scene = crop_scene.copy()
        cols_spec = [
            (13, 47, ZONE_COLORS["NOPVP"]["core"]),
            (81, 115, ZONE_COLORS["PZ"]["core"]),
            (149, 183, ZONE_COLORS["NOLOG"]["core"]),
            (217, 251, ZONE_COLORS["PVP"]["core"]),
        ]
        tile_h = 34
        for cx0, cx1, c_col in cols_spec:
            for row in range(4):
                ry0 = 18 + row * tile_h
                ry1 = ry0 + tile_h
                sub_tile = right_scene.crop((cx0, ry0, cx1, ry1))
                bracketed = draw_cipsoft_tile_bracket(sub_tile, c_col, has_shadow=True, line_w=1, margin=1)
                right_scene.paste(bracketed, (cx0, ry0))

        # Scale up by 1.5x for clarity
        scale_f = 1.6
        nw = int(cw * scale_f)
        nh = int(ch * scale_f)
        left_scaled = left_img.resize((nw, nh), Image.Resampling.BILINEAR)
        right_scaled = right_scene.resize((nw, nh), Image.Resampling.BILINEAR)

        sec3_content_y = sec3_y + 40
        place(left_scaled, 60, sec3_content_y, "Current RME Style (Full Washes)", "No-PvP (Green) | PZ (Yellow) | No-Log (Orange) | PvP (Red)")
        place(right_scaled, 580, sec3_content_y, "CipSoft Style (Lined Squares)", "Sharp color-coded corner brackets, unshaded stone floor")

    return board


if __name__ == "__main__":
    out_dir = Path("tools/assets")
    out_dir.mkdir(parents=True, exist_ok=True)
    board = build_zone_study_board()
    
    out_path = out_dir / "cipsoft_4zones_prototype.png"
    board.save(out_path, quality=95)
    print(f"[Success] Saved 4-zones prototype to: {out_path.resolve()}")
