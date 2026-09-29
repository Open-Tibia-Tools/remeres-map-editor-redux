#!/usr/bin/env python3
"""
RME Redux - Bracket-Based Zero-Fill Overlay System Prototype
Prototypes:
1. The 4 Special Zones: 4 bracket colors with dedicated corner micro-badges:
   - Top-Left: PZ (Yellow)
   - Top-Right: No-PvP (Green)
   - Bottom-Left: No-Logout (Orange)
   - Bottom-Right: PvP (Red)
2. Technical Items: STAIR, WALK, BLOCK, TOWN, HOUSE with brackets + text (NO background fill)
3. Spawns: Center tile with SPAWN text + bracket; spawn area with just brackets (NO fill)
4. Connected vs. Perimeter comparison for multi-tile zones and pathing
5. Full in-game test scene rendered with the new zero-fill bracket system
"""

from __future__ import annotations
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np


# Palette
COLOR_PZ       = (255, 235, 60)     # Yellow
COLOR_NOPVP    = (50, 240, 100)     # Emerald Green
COLOR_NOLOG    = (255, 145, 30)     # Orange
COLOR_PVP      = (255, 45, 70)      # Crimson Red
COLOR_BLOCKING = (0, 240, 255)      # Cyan
COLOR_SPAWN    = (235, 50, 235)     # Magenta
COLOR_HOUSE    = (60, 245, 90)      # House Green
COLOR_STAIR    = (255, 235, 60)     # Stair Yellow
COLOR_WALK     = (0, 235, 235)      # Walk Cyan
COLOR_BLOCK    = (255, 45, 60)      # Block Red
COLOR_TOWN     = (255, 215, 40)     # Town Gold


def draw_tile_brackets(img: Image.Image,
                       color: tuple,
                       margin: int = 2,
                       c_ratio: float = 0.22,
                       m_ratio: float = 0.24,
                       line_w: int = 2,
                       has_mid_dashes: bool = True,
                       has_shadow: bool = True) -> Image.Image:
    """Draws high-contrast brackets on a tile with zero background fill."""
    out = img.copy().convert('RGBA')
    overlay = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    
    w, h = out.size
    x0, y0 = margin, margin
    x1, y1 = w - 1 - margin, h - 1 - margin
    
    c_len = max(3, int(w * c_ratio))
    m_len = max(3, int(w * m_ratio))
    
    segments = [
        ((x0, y0), (x0 + c_len, y0)),
        ((x0, y0), (x0, y0 + c_len)),
        ((x1 - c_len, y0), (x1, y0)),
        ((x1, y0), (x1, y0 + c_len)),
        ((x0, y1), (x0 + c_len, y1)),
        ((x0, y1 - c_len), (x0, y1)),
        ((x1 - c_len, y1), (x1, y1)),
        ((x1, y1 - c_len), (x1, y1)),
    ]
    
    if has_mid_dashes:
        mx0 = (w - m_len) // 2
        mx1 = mx0 + m_len
        my0 = (h - m_len) // 2
        my1 = my0 + m_len
        segments.extend([
            ((mx0, y0), (mx1, y0)),
            ((mx0, y1), (mx1, y1)),
            ((x0, my0), (x0, my1)),
            ((x1, my0), (x1, my1)),
        ])
        
    # Dark outline
    if has_shadow:
        for (sx0, sy0), (sx1, sy1) in segments:
            draw.line([(sx0, sy0), (sx1, sy1)], fill=(12, 12, 15, 240), width=line_w + 2)
            
    # Core stroke
    for (sx0, sy0), (sx1, sy1) in segments:
        draw.line([(sx0, sy0), (sx1, sy1)], fill=color + (255,), width=line_w)
        
    return Image.alpha_composite(out, overlay)


def draw_corner_badge(img: Image.Image,
                      corner: str,  # 'TL', 'TR', 'BL', 'BR'
                      label: str,
                      color: tuple) -> Image.Image:
    """Draws a compact high-contrast micro-badge in one of the 4 corners."""
    out = img.copy().convert('RGBA')
    draw = ImageDraw.Draw(out)
    w, h = out.size
    
    try:
        font = ImageFont.truetype("arialbd.ttf", max(8, int(w * 0.12)))
    except IOError:
        font = ImageFont.load_default()
        
    bbox = font.getbbox(label)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    bw, bh = tw + 6, th + 4
    
    margin = 3
    if corner == 'TL':
        bx0, by0 = margin, margin
    elif corner == 'TR':
        bx0, by0 = w - bw - margin, margin
    elif corner == 'BL':
        bx0, by0 = margin, h - bh - margin
    else: # BR
        bx0, by0 = w - bw - margin, h - bh - margin
        
    bx1, by1 = bx0 + bw, by0 + bh
    
    # 2-tone badge: Dark background with 1px colored border
    draw.rectangle([bx0, by0, bx1, by1], fill=(15, 15, 20, 245), outline=color + (255,), width=1)
    # Centered label
    draw.text((bx0 + (bw - tw) // 2, by0 + (bh - th) // 2 - 1), label, font=font, fill=color + (255,))
    return out


def draw_bracket_with_center_text(img: Image.Image,
                                   text: str,
                                   color: tuple,
                                   line_w: int = 2) -> Image.Image:
    """Draws brackets with crisp centered text (NO background wash)."""
    out = draw_tile_brackets(img, color, line_w=line_w)
    draw = ImageDraw.Draw(out)
    w, h = out.size
    
    try:
        font = ImageFont.truetype("arialbd.ttf", max(10, int(w * 0.16)))
    except IOError:
        font = ImageFont.load_default()
        
    bbox = font.getbbox(text)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (w - tw) // 2
    ty = (h - th) // 2 - 1
    
    # Text drop-shadow for contrast without a box
    for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)]:
        draw.text((tx + dx, ty + dy), text, font=font, fill=(10, 10, 15, 255))
    # Crisp colored / white text
    draw.text((tx, ty), text, font=font, fill=color + (255,))
    return out


def create_stone_base(size: int = 64) -> Image.Image:
    np.random.seed(55)
    base = np.full((size, size, 3), 132, dtype=np.float32)
    noise = np.random.normal(0, 5, (size, size, 3))
    base = np.clip(base + noise, 110, 155)
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size - 1, size - 1], outline=(100, 100, 105), width=1)
    return img


def create_book_base(size: int = 64) -> Image.Image:
    tile = create_stone_base(size)
    draw = ImageDraw.Draw(tile)
    cx, cy = size // 2, size // 2
    bw, bh = int(size * 0.52), int(size * 0.50)
    bx0, by0 = cx - bw // 2, cy - bh // 2
    draw.polygon([(bx0 + 4, by0 + bh), (cx, by0 + bh - 4), (bx0 + bw - 4, by0 + bh), (cx, by0 + 4)], fill=(210, 205, 190), outline=(50, 45, 40))
    draw.polygon([(bx0, by0 + 8), (cx - 2, by0 + 4), (cx - 2, by0 + bh - 2), (bx0, by0 + bh - 6)], fill=(120, 75, 45), outline=(40, 25, 15))
    draw.polygon([(cx + 2, by0 + 4), (bx0 + bw, by0 + 8), (bx0 + bw, by0 + bh - 6), (cx + 2, by0 + bh - 2)], fill=(135, 85, 50), outline=(40, 25, 15))
    draw.line([(cx - 1, by0 - 4), (cx + 3, by0 + 6)], fill=(210, 40, 50), width=2)
    draw.ellipse([cx + 6, cy - 2, cx + 16, cy + 8], fill=(225, 180, 40), outline=(60, 45, 10), width=1)
    return tile


def create_chest_base(size: int = 64) -> Image.Image:
    tile = create_stone_base(size)
    draw = ImageDraw.Draw(tile)
    cx, cy = size // 2, size // 2 + 2
    w, h = int(size * 0.62), int(size * 0.48)
    x0, y0 = cx - w // 2, cy - h // 2
    draw.ellipse([x0 - 2, y0 + h - 4, x0 + w + 2, y0 + h + 6], fill=(15, 15, 20, 140))
    draw.rectangle([x0, y0 + 6, x0 + w, y0 + h], fill=(130, 75, 35), outline=(50, 25, 10), width=1)
    draw.polygon([(x0, y0 + 6), (x0 + 4, y0), (x0 + w - 4, y0), (x0 + w, y0 + 6)], fill=(155, 95, 45), outline=(50, 25, 10))
    draw.rectangle([cx - 4, y0 + 8, cx + 4, y0 + 18], fill=(245, 210, 60), outline=(60, 45, 10), width=1)
    return tile


def create_stair_base(size: int = 64) -> Image.Image:
    tile = create_stone_base(size)
    draw = ImageDraw.Draw(tile)
    tiers = 3
    th = size // (tiers + 1)
    for i in range(tiers):
        y = (i + 1) * th
        draw.rectangle([4, y, size - 4, y + 4], fill=(100 - i * 15, 100 - i * 15, 100 - i * 15), outline=(40, 40, 40), width=1)
        draw.rectangle([4, y + 4, size - 4, min(size - 2, y + th)], fill=(155 - i * 15, 155 - i * 15, 155 - i * 15), outline=(50, 50, 50), width=1)
    return tile


def create_barrel_base(size: int = 64) -> Image.Image:
    tile = create_stone_base(size)
    draw = ImageDraw.Draw(tile)
    cx, cy = size // 2, size // 2
    bw, bh = int(size * 0.55), int(size * 0.65)
    bx0, by0 = cx - bw // 2, cy - bh // 2
    draw.ellipse([bx0, by0 + bh - 6, bx0 + bw, by0 + bh + 4], fill=(20, 20, 25, 120))
    draw.ellipse([bx0, by0, bx0 + bw, by0 + bh], fill=(145, 95, 50), outline=(45, 25, 10), width=1)
    draw.line([(bx0 + 2, by0 + int(bh * 0.3)), (bx0 + bw - 2, by0 + int(bh * 0.3))], fill=(80, 80, 85), width=2)
    draw.line([(bx0 + 2, by0 + int(bh * 0.7)), (bx0 + bw - 2, by0 + int(bh * 0.7))], fill=(80, 80, 85), width=2)
    return tile


def build_system_showcase() -> Image.Image:
    canvas_w = 1160
    canvas_h = 1280
    board = Image.new('RGB', (canvas_w, canvas_h), (20, 20, 24))
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
    draw.rectangle([0, 0, canvas_w, 80], fill=(28, 28, 34))
    draw.line([(0, 80), (canvas_w, 80)], fill=(45, 45, 55), width=1)
    draw.text((30, 16), "RME Redux — Zero-Fill Bracket Overlay System", font=title_font, fill=(255, 255, 255))
    draw.text((30, 48), "Color-coded brackets with 4 corner micro-badges • Zero muddy washes • Technical items with text", font=body_font, fill=(160, 165, 175))

    def place_tile(t: Image.Image, x: int, y: int, top: str = "", bottom: str = ""):
        board.paste(t, (x, y))
        draw.rectangle([x - 1, y - 1, x + t.width, y + t.height], outline=(55, 55, 65), width=1)
        if top:
            draw.text((x + t.width // 2, y - 18), top, font=bold_font, fill=(225, 225, 235), anchor="mt")
        if bottom:
            draw.text((x + t.width // 2, y + t.height + 5), bottom, font=mono_font, fill=(140, 145, 155), anchor="mt")

    # =========================================================================
    # PART 1: The 4 Special Zones (4 Bracket Colors + 4 Dedicated Corners)
    # =========================================================================
    sec1_y = 110
    draw.text((30, sec1_y - 20), "1. The 4 Special Zones: 4 Bracket Colors & 4 Dedicated Corner Micro-Badges", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec1_y + 4), (canvas_w - 30, sec1_y + 4)], fill=(42, 42, 50), width=1)
    
    sz = 86
    base_book = create_book_base(sz)
    base_chest = create_chest_base(sz)
    
    # 1. PZ (Top-Left: [PZ], Yellow)
    pz_tile = draw_tile_brackets(base_book, COLOR_PZ)
    pz_tile = draw_corner_badge(pz_tile, 'TL', 'PZ', COLOR_PZ)
    
    # 2. No-PvP (Top-Right: [NP], Green)
    nopvp_tile = draw_tile_brackets(base_chest, COLOR_NOPVP)
    nopvp_tile = draw_corner_badge(nopvp_tile, 'TR', 'NP', COLOR_NOPVP)
    
    # 3. No-Logout (Bottom-Left: [NL], Orange)
    nolog_tile = draw_tile_brackets(base_book, COLOR_NOLOG)
    nolog_tile = draw_corner_badge(nolog_tile, 'BL', 'NL', COLOR_NOLOG)
    
    # 4. PvP Zone (Bottom-Right: [PvP], Red)
    pvp_tile = draw_tile_brackets(base_chest, COLOR_PVP)
    pvp_tile = draw_corner_badge(pvp_tile, 'BR', 'PvP', COLOR_PVP)
    
    # 5. Multi-Zone Overlap Example (e.g. PZ + No-Logout on same tile!)
    multi_tile = draw_tile_brackets(base_book, COLOR_PZ) # primary yellow
    multi_tile = draw_corner_badge(multi_tile, 'TL', 'PZ', COLOR_PZ)
    multi_tile = draw_corner_badge(multi_tile, 'BL', 'NL', COLOR_NOLOG)

    col_gap = 215
    y_pos = sec1_y + 35
    place_tile(pz_tile, 45, y_pos, "Protection Zone", "TL Corner: [PZ] Yellow")
    place_tile(nopvp_tile, 45 + col_gap, y_pos, "No-PvP Zone", "TR Corner: [NP] Green")
    place_tile(nolog_tile, 45 + col_gap * 2, y_pos, "No-Logout Zone", "BL Corner: [NL] Orange")
    place_tile(pvp_tile, 45 + col_gap * 3, y_pos, "PvP Zone (Hardcore)", "BR Corner: [PvP] Red")
    place_tile(multi_tile, 45 + col_gap * 4, y_pos, "Zone Overlap (PZ + NL)", "Dedicated corners never collide!")

    # =========================================================================
    # PART 2: Technical Items & Spawns (Brackets + Text, ZERO Fill)
    # =========================================================================
    sec2_y = 295
    draw.text((30, sec2_y), "2. Technical Items & Spawns: Brackets with Text (Zero Background Fill)", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec2_y + 24), (canvas_w - 30, sec2_y + 24)], fill=(42, 42, 50), width=1)
    
    stone_sz = create_stone_base(sz)
    stair_sz = create_stair_base(sz)
    
    stair_t = draw_bracket_with_center_text(stair_sz, "STAIR", COLOR_STAIR)
    walk_t  = draw_bracket_with_center_text(stone_sz, "WALK", COLOR_WALK)
    block_t = draw_bracket_with_center_text(stone_sz, "BLOCK", COLOR_BLOCK)
    town_t  = draw_bracket_with_center_text(stone_sz, "TOWN", COLOR_TOWN)
    spawn_t = draw_bracket_with_center_text(stone_sz, "SPAWN", COLOR_SPAWN)
    
    # House tile (wood floor + 'H' bracket, zero fill)
    house_base = stone_sz.copy()
    house_t = draw_bracket_with_center_text(house_base, "H", COLOR_HOUSE)

    y_pos2 = sec2_y + 50
    gap2 = 180
    place_tile(stair_t, 45, y_pos2, "STAIR", "Stairs visible underneath")
    place_tile(walk_t, 45 + gap2, y_pos2, "WALK", "Cyan brackets + text")
    place_tile(block_t, 45 + gap2 * 2, y_pos2, "BLOCK (1548)", "Red brackets + text")
    place_tile(town_t, 45 + gap2 * 3, y_pos2, "TOWN", "Gold brackets + text")
    place_tile(spawn_t, 45 + gap2 * 4, y_pos2, "SPAWN Center", "Magenta brackets + text")
    place_tile(house_t, 45 + gap2 * 5, y_pos2, "HOUSE Floor", "Green brackets + 'H'")

    # =========================================================================
    # PART 3: The Big Question: Connected vs. Perimeter for Multi-Tile Zones & Pathing
    # =========================================================================
    sec3_y = 485
    draw.text((30, sec3_y), "3. Multi-Tile Layouts: Connected (Per-Tile) vs. Outer Perimeter Only", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec3_y + 24), (canvas_w - 30, sec3_y + 24)], fill=(42, 42, 50), width=1)

    # 3.1: PATHING / BLOCKING on 3 Barrels: Connected vs Perimeter
    bar_sz = 64
    b0 = create_barrel_base(bar_sz)
    b1 = create_barrel_base(bar_sz)
    b2 = create_barrel_base(bar_sz)
    
    # Case A: Per-Tile Brackets (Each barrel has full brackets)
    pat_pertile = Image.new('RGB', (bar_sz * 3, bar_sz))
    for i in range(3):
        bt = draw_tile_brackets(b0, COLOR_BLOCKING, line_w=2)
        pat_pertile.paste(bt, (i * bar_sz, 0))
        
    # Case B: Perimeter Contour (Only outer boundary of the 3 connected barrels has brackets)
    pat_perim = Image.new('RGB', (bar_sz * 3, bar_sz))
    for i in range(3):
        pat_perim.paste(b0, (i * bar_sz, 0))
    # Draw connected perimeter dashes
    pw = bar_sz * 3
    ph = bar_sz
    p_over = Image.new('RGBA', (pw, ph), (0, 0, 0, 0))
    pdraw = ImageDraw.Draw(p_over)
    # Corners
    c_len = 14
    # TL, BL
    pdraw.line([(2, 2), (2 + c_len, 2)], fill=(10, 10, 15, 240), width=4); pdraw.line([(2, 2), (2 + c_len, 2)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(2, 2), (2, 2 + c_len)], fill=(10, 10, 15, 240), width=4); pdraw.line([(2, 2), (2, 2 + c_len)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(2, ph - 3), (2 + c_len, ph - 3)], fill=(10, 10, 15, 240), width=4); pdraw.line([(2, ph - 3), (2 + c_len, ph - 3)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(2, ph - 3 - c_len), (2, ph - 3)], fill=(10, 10, 15, 240), width=4); pdraw.line([(2, ph - 3 - c_len), (2, ph - 3)], fill=COLOR_BLOCKING + (255,), width=2)
    # TR, BR
    pdraw.line([(pw - 3 - c_len, 2), (pw - 3, 2)], fill=(10, 10, 15, 240), width=4); pdraw.line([(pw - 3 - c_len, 2), (pw - 3, 2)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(pw - 3, 2), (pw - 3, 2 + c_len)], fill=(10, 10, 15, 240), width=4); pdraw.line([(pw - 3, 2), (pw - 3, 2 + c_len)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(pw - 3 - c_len, ph - 3), (pw - 3, ph - 3)], fill=(10, 10, 15, 240), width=4); pdraw.line([(pw - 3 - c_len, ph - 3), (pw - 3, ph - 3)], fill=COLOR_BLOCKING + (255,), width=2)
    pdraw.line([(pw - 3, ph - 3 - c_len), (pw - 3, ph - 3)], fill=(10, 10, 15, 240), width=4); pdraw.line([(pw - 3, ph - 3 - c_len), (pw - 3, ph - 3)], fill=COLOR_BLOCKING + (255,), width=2)
    # Dashes on top/bottom
    for tx in [bar_sz * 0.5, bar_sz * 1.5, bar_sz * 2.5]:
        pdraw.line([(int(tx - 7), 2), (int(tx + 7), 2)], fill=(10, 10, 15, 240), width=4)
        pdraw.line([(int(tx - 7), 2), (int(tx + 7), 2)], fill=COLOR_BLOCKING + (255,), width=2)
        pdraw.line([(int(tx - 7), ph - 3), (int(tx + 7), ph - 3)], fill=(10, 10, 15, 240), width=4)
        pdraw.line([(int(tx - 7), ph - 3), (int(tx + 7), ph - 3)], fill=COLOR_BLOCKING + (255,), width=2)
    pat_perim = Image.alpha_composite(pat_perim.convert('RGBA'), p_over)

    # 3.2: 3x3 ZONE AREA (PZ Room): Connected vs Perimeter
    pz_room_sz = 3 * 64
    base_pz_room = Image.new('RGB', (pz_room_sz, pz_room_sz))
    for ry in range(3):
        for rx in range(3):
            if rx == 1 and ry == 1:
                t = create_book_base(64)
            elif rx == 0 and ry == 0:
                t = create_chest_base(64)
            elif rx == 2 and ry == 2:
                t = create_stair_base(64)
            else:
                t = create_stone_base(64)
            base_pz_room.paste(t, (rx * 64, ry * 64))

    # Room Option A: Per-Tile Brackets + Corner Badge on each tile
    room_pertile = base_pz_room.copy().convert('RGBA')
    for ry in range(3):
        for rx in range(3):
            sub_t = base_pz_room.crop((rx * 64, ry * 64, (rx + 1) * 64, (ry + 1) * 64))
            b_t = draw_tile_brackets(sub_t, COLOR_PZ, line_w=1, margin=1)
            b_t = draw_corner_badge(b_t, 'TL', 'PZ', COLOR_PZ)
            room_pertile.paste(b_t, (rx * 64, ry * 64))

    # Room Option B: Perimeter Only (Outer dashed frame + single corner badge in room's top-left)
    room_perim = base_pz_room.copy().convert('RGBA')
    r_over = Image.new('RGBA', (pz_room_sz, pz_room_sz), (0, 0, 0, 0))
    rdraw = ImageDraw.Draw(r_over)
    # Dashed outer box
    rw = pz_room_sz
    for x in range(2, rw - 2, 22):
        rdraw.line([(x, 2), (min(rw - 2, x + 12), 2)], fill=(10, 10, 15, 240), width=4)
        rdraw.line([(x, 2), (min(rw - 2, x + 12), 2)], fill=COLOR_PZ + (255,), width=2)
        rdraw.line([(x, rw - 3), (min(rw - 2, x + 12), rw - 3)], fill=(10, 10, 15, 240), width=4)
        rdraw.line([(x, rw - 3), (min(rw - 2, x + 12), rw - 3)], fill=COLOR_PZ + (255,), width=2)
    for y in range(2, rw - 2, 22):
        rdraw.line([(2, y), (2, min(rw - 2, y + 12))], fill=(10, 10, 15, 240), width=4)
        rdraw.line([(2, y), (2, min(rw - 2, y + 12))], fill=COLOR_PZ + (255,), width=2)
        rdraw.line([(rw - 3, y), (rw - 3, min(rw - 2, y + 12))], fill=(10, 10, 15, 240), width=4)
        rdraw.line([(rw - 3, y), (rw - 3, min(rw - 2, y + 12))], fill=COLOR_PZ + (255,), width=2)
    room_perim = Image.alpha_composite(room_perim, r_over)
    room_perim = draw_corner_badge(room_perim, 'TL', 'PROTECTION ZONE', COLOR_PZ)

    # Place Pathing comparisons
    draw.text((45, sec3_y + 35), "Pathing / Blocking (Barrels):", font=bold_font, fill=(200, 205, 215))
    place_tile(pat_pertile, 45, sec3_y + 70, "Option A: Per-Tile Brackets", "Each barrel has 4 brackets")
    place_tile(pat_perim, 45 + bar_sz * 3 + 40, sec3_y + 70, "Option B: Perimeter Contour", "Single outer dashed frame (clean inside)")

    # Place Zone Area comparisons
    draw.text((580, sec3_y + 35), "Special Zones (3x3 Room):", font=bold_font, fill=(200, 205, 215))
    place_tile(room_pertile, 580, sec3_y + 70, "Option A: Per-Tile Brackets", "Grid clarity, [PZ] on each tile")
    place_tile(room_perim, 580 + pz_room_sz + 40, sec3_y + 70, "Option B: Perimeter Contour", "Clean room interior, single badge")

    # =========================================================================
    # PART 4: Real Editor Scene Full Preview (Columns, Barrels, Spawn, Town)
    # =========================================================================
    sec4_y = 810
    draw.text((30, sec4_y), "4. Real Editor Scene: Complete Zero-Fill Overlay Mockup", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec4_y + 24), (canvas_w - 30, sec4_y + 24)], fill=(42, 42, 50), width=1)

    scene_path = Path("tools/assets/scene_clean.png")
    if scene_path.exists():
        scene_img = Image.open(scene_path).convert('RGBA')
        crop_box = (130, 35, 600, 565)
        crop = scene_img.crop(crop_box)
        cw, ch = crop.size

        # Apply the new bracket system onto the crop:
        # A. Columns (relative to x0=130, y0=35):
        # Col 1 STAIR: x in [144-130, 178-130] = [14, 48], y in [213-35, 349-35] = [178, 314]
        # Col 2 WALK:  [82, 116], [178, 314]
        # Col 3 BLOCK: [150, 184], [178, 314]
        # Col 4 No-PvP:[218, 252], [178, 314]
        # Col 5 PZ:    [286, 320], [178, 314]
        # Col 6 No-Log:[354, 388], [178, 314]
        # Col 7 PvP:   [422, 456], [178, 314]
        
        # Draw tech items with brackets + text (no fill)
        tile_h = 34
        for row in range(4):
            ry0 = 178 + row * tile_h
            ry1 = ry0 + tile_h
            
            # STAIR
            st = crop.crop((14, ry0, 48, ry1))
            crop.paste(draw_bracket_with_center_text(st, "STAIR", COLOR_STAIR, line_w=1), (14, ry0))
            
            # WALK
            wt = crop.crop((82, ry0, 116, ry1))
            crop.paste(draw_bracket_with_center_text(wt, "WALK", COLOR_WALK, line_w=1), (82, ry0))
            
            # BLOCK
            bt = crop.crop((150, ry0, 184, ry1))
            crop.paste(draw_bracket_with_center_text(bt, "BLOCK", COLOR_BLOCK, line_w=1), (150, ry0))
            
            # No-PvP (Green bracket + TR [NP])
            nt = crop.crop((218, ry0, 252, ry1))
            nt = draw_tile_brackets(nt, COLOR_NOPVP, line_w=1, margin=1)
            nt = draw_corner_badge(nt, 'TR', 'NP', COLOR_NOPVP)
            crop.paste(nt, (218, ry0))
            
            # PZ (Yellow bracket + TL [PZ])
            pt = crop.crop((286, ry0, 320, ry1))
            pt = draw_tile_brackets(pt, COLOR_PZ, line_w=1, margin=1)
            pt = draw_corner_badge(pt, 'TL', 'PZ', COLOR_PZ)
            crop.paste(pt, (286, ry0))
            
            # No-Logout (Orange bracket + BL [NL])
            lt = crop.crop((354, ry0, 388, ry1))
            lt = draw_tile_brackets(lt, COLOR_NOLOG, line_w=1, margin=1)
            lt = draw_corner_badge(lt, 'BL', 'NL', COLOR_NOLOG)
            crop.paste(lt, (354, ry0))
            
            # PvP (Red bracket + BR [PvP])
            vt = crop.crop((422, ry0, 456, ry1))
            vt = draw_tile_brackets(vt, COLOR_PVP, line_w=1, margin=1)
            vt = draw_corner_badge(vt, 'BR', 'PvP', COLOR_PVP)
            crop.paste(vt, (422, ry0))

        # Town badge (x in [178-130, 212-130] = [48, 82], y in [420-35, 454-35] = [385, 419])
        tt = crop.crop((48, 385, 82, 419))
        crop.paste(draw_bracket_with_center_text(tt, "TOWN", COLOR_TOWN, line_w=1), (48, 385))

        # Spawn center badge (x in [386-130, 420-130] = [256, 290], y in [455-35, 489-35] = [420, 454])
        spt = crop.crop((256, 420, 290, 454))
        crop.paste(draw_bracket_with_center_text(spt, "SPAWN", COLOR_SPAWN, line_w=1), (256, 420))

        # Spawn 5x5 boundary (x in [314-130, 484-130] = [184, 354], y in [383-35, 553-35] = [348, 518])
        sp_over = Image.new('RGBA', crop.size, (0, 0, 0, 0))
        sp_draw = ImageDraw.Draw(sp_over)
        sp_x0, sp_x1, sp_y0, sp_y1 = 184, 354, 348, 518
        # Dashed perimeter for spawn radius (NO fill!)
        for x in range(sp_x0, sp_x1, 18):
            sp_draw.line([(x, sp_y0), (min(sp_x1, x + 10), sp_y0)], fill=(10, 10, 15, 240), width=3)
            sp_draw.line([(x, sp_y0), (min(sp_x1, x + 10), sp_y0)], fill=COLOR_SPAWN + (255,), width=1)
            sp_draw.line([(x, sp_y1 - 1), (min(sp_x1, x + 10), sp_y1 - 1)], fill=(10, 10, 15, 240), width=3)
            sp_draw.line([(x, sp_y1 - 1), (min(sp_x1, x + 10), sp_y1 - 1)], fill=COLOR_SPAWN + (255,), width=1)
        for y in range(sp_y0, sp_y1, 18):
            sp_draw.line([(sp_x0, y), (sp_x0, min(sp_y1, y + 10))], fill=(10, 10, 15, 240), width=3)
            sp_draw.line([(sp_x0, y), (sp_x0, min(sp_y1, y + 10))], fill=COLOR_SPAWN + (255,), width=1)
            sp_draw.line([(sp_x1 - 1, y), (sp_x1 - 1, min(sp_y1, y + 10))], fill=(10, 10, 15, 240), width=3)
            sp_draw.line([(sp_x1 - 1, y), (sp_x1 - 1, min(sp_y1, y + 10))], fill=COLOR_SPAWN + (255,), width=1)
        crop = Image.alpha_composite(crop, sp_over)

        # Scale for crisp presentation
        scale_f = 1.35
        sc_w = int(cw * scale_f)
        sc_h = int(ch * scale_f)
        crop_scaled = crop.resize((sc_w, sc_h), Image.Resampling.BILINEAR)
        place_tile(crop_scaled, 45, sec4_y + 40, "Live Editor Scene with Zero-Fill Bracket Architecture", "No muddy washes • Pristine sprites • 4 corner badges • Unobscured spawn/town")

        # Explanatory card on right
        card_x = 45 + sc_w + 35
        card_w = canvas_w - card_x - 35
        card_h = sc_h
        draw.rectangle([card_x, sec4_y + 40, card_x + card_w, sec4_y + 40 + card_h], fill=(28, 28, 34), outline=(50, 50, 60), width=1)
        
        cx_text = card_x + 20
        draw.text((cx_text, sec4_y + 55), "Key Advantages of This System:", font=bold_font, fill=(255, 255, 255))
        
        points = [
            ("1. 100% Unobscured Sprites", "Zero background wash means stone, wood, books, and creatures retain original Tibia art vibrancy."),
            ("2. 4 Dedicated Corner Badges", "PZ (Top-Left), No-PvP (Top-Right), No-Log (Bottom-Left), PvP (Bottom-Right). Never overlap or collide!"),
            ("3. Clean Technical Items", "STAIR, WALK, BLOCK, TOWN, and SPAWN render crisp text with brackets, without solid banners."),
            ("4. Connected vs. Perimeter", "Perimeter contour gives large rooms a clean architectural boundary. Per-tile gives granular grid precision.")
        ]
        
        cy_p = sec4_y + 90
        for title, desc in points:
            draw.text((cx_text, cy_p), title, font=bold_font, fill=(90, 200, 255))
            cy_p += 18
            draw.text((cx_text, cy_p), desc, font=body_font, fill=(175, 180, 190))
            cy_p += 36

    return board


if __name__ == "__main__":
    out_dir = Path("tools/assets")
    out_dir.mkdir(parents=True, exist_ok=True)
    board = build_system_showcase()
    
    out_path = out_dir / "cipsoft_zero_fill_system_prototype.png"
    board.save(out_path, quality=95)
    print(f"[Success] Saved bracket system prototype to: {out_path.resolve()}")
