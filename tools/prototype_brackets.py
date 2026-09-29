#!/usr/bin/env python3
"""
RME Redux - CipSoft Bracket Indicator Prototype Generator
Generates a side-by-side visual study and variations of the CipSoft lined-square
indicator over Tibia items and floor terrains.
"""

from __future__ import annotations
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np


def create_stone_tile(size: int = 64) -> Image.Image:
    """Procedurally generates an authentic Tibia grey stone floor tile."""
    np.random.seed(42)
    base = np.full((size, size, 3), 135, dtype=np.float32)
    # Texture noise
    noise = np.random.normal(0, 7, (size, size, 3))
    base = np.clip(base + noise, 100, 165)
    # Cracks & stone seams
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    # Subtle border grid lines
    draw.rectangle([0, 0, size - 1, size - 1], outline=(105, 105, 105), width=1)
    # A couple of subtle diagonal crack lines
    draw.line([(int(size*0.2), int(size*0.3)), (int(size*0.45), int(size*0.6)), (int(size*0.7), int(size*0.65))], fill=(95, 95, 95), width=1)
    return img


def create_snow_tile(size: int = 64) -> Image.Image:
    """Procedurally generates a bright Tibia snow tile."""
    np.random.seed(101)
    base = np.full((size, size, 3), 235, dtype=np.float32)
    noise = np.random.normal(0, 4, (size, size, 3))
    base = np.clip(base + noise, 215, 250)
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size - 1, size - 1], outline=(200, 205, 215), width=1)
    return img


def create_cave_tile(size: int = 64) -> Image.Image:
    """Procedurally generates a dark Tibia cave / void tile."""
    np.random.seed(202)
    base = np.full((size, size, 3), 45, dtype=np.float32)
    noise = np.random.normal(0, 5, (size, size, 3))
    base = np.clip(base + noise, 30, 65)
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size - 1, size - 1], outline=(30, 30, 35), width=1)
    return img


def create_sand_tile(size: int = 64) -> Image.Image:
    """Procedurally generates a warm desert sand tile."""
    np.random.seed(303)
    base = np.zeros((size, size, 3), dtype=np.float32)
    base[:, :, 0] = 225  # R
    base[:, :, 1] = 205  # G
    base[:, :, 2] = 150  # B
    noise = np.random.normal(0, 6, (size, size, 3))
    base = np.clip(base + noise, 130, 245)
    img = Image.fromarray(base.astype(np.uint8))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size - 1, size - 1], outline=(185, 165, 120), width=1)
    return img


def create_chest_sprite(size: int = 64) -> Image.Image:
    """Renders a detailed Tibia-style wooden quest chest with gold trim."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Chest coordinates (centered)
    cx, cy = size // 2, size // 2 + 2
    w, h = int(size * 0.65), int(size * 0.50)
    x0, y0 = cx - w // 2, cy - h // 2
    x1, y1 = x0 + w, y0 + h
    
    # Shadow under chest
    draw.ellipse([x0 - 2, y1 - 4, x1 + 2, y1 + 6], fill=(15, 15, 20, 140))
    
    # Wooden body
    draw.rectangle([x0, y0 + 6, x1, y1], fill=(130, 75, 35), outline=(50, 25, 10), width=1)
    # Curved lid
    draw.polygon([(x0, y0 + 6), (x0 + 4, y0), (x1 - 4, y0), (x1, y0 + 6)], fill=(155, 95, 45), outline=(50, 25, 10))
    
    # Gold bands & trim
    band_w = max(2, size // 20)
    for bx in [x0 + int(w * 0.25), x0 + int(w * 0.75)]:
        draw.rectangle([bx - band_w // 2, y0, bx + band_w // 2, y1], fill=(225, 185, 40), outline=(80, 60, 10), width=1)
    
    # Gold Keyhole / Lock in center
    lx0, ly0 = cx - 4, y0 + 8
    draw.rectangle([lx0, ly0, lx0 + 8, ly0 + 10], fill=(245, 210, 60), outline=(60, 45, 10), width=1)
    draw.rectangle([cx - 1, ly0 + 3, cx + 1, ly0 + 7], fill=(30, 20, 5))
    return img


def create_stair_sprite(size: int = 64) -> Image.Image:
    """Renders a stone stairs / ramp tile."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # 3 stone step tiers
    tiers = 3
    th = size // (tiers + 1)
    for i in range(tiers):
        y = (i + 1) * th
        c_face = 110 - i * 15
        c_top = 160 - i * 15
        draw.rectangle([4, y, size - 4, y + 4], fill=(c_face, c_face, c_face), outline=(40, 40, 40), width=1)
        draw.rectangle([4, y + 4, size - 4, min(size - 2, y + th)], fill=(c_top, c_top, c_top), outline=(50, 50, 50), width=1)
    return img


def create_lever_sprite(size: int = 64) -> Image.Image:
    """Renders a ground lever / switch."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    # Stone base plate
    draw.rectangle([cx - 12, cy - 8, cx + 12, cy + 8], fill=(90, 90, 95), outline=(30, 30, 35), width=1)
    # Lever pivot
    draw.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=(50, 50, 55), outline=(20, 20, 25), width=1)
    # Lever stick pointing up-right
    draw.line([(cx, cy), (cx + 10, cy - 14)], fill=(180, 180, 190), width=3)
    # Red handle ball
    draw.ellipse([cx + 8, cy - 17, cx + 15, cy - 10], fill=(220, 40, 40), outline=(80, 10, 10), width=1)
    return img


def draw_cipsoft_bracket(img: Image.Image,
                         color: tuple = (255, 255, 175),
                         style: str = "cipsoft",
                         line_width: int = 2,
                         has_shadow: bool = True,
                         corner_ratio: float = 0.22,
                         mid_ratio: float = 0.22,
                         margin: int = 2) -> Image.Image:
    """
    Renders high-contrast bracket indicators onto an image.
    style:
      - 'cipsoft': 4 corners + 4 midpoint dashes (authentic official style)
      - 'corners_only': 4 corners only (minimalist HUD / CAD reticle)
      - 'micro_pip': corners + tiny letter badge in top-right
    """
    out = img.copy()
    overlay = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    
    w, h = out.size
    x0, y0 = margin, margin
    x1, y1 = w - 1 - margin, h - 1 - margin
    
    c_len = max(3, int(w * corner_ratio))
    m_len = max(3, int(w * mid_ratio))
    
    # 4 Corner brackets (each has 2 line segments)
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
    ]
    
    if style == 'cipsoft':
        mx0 = (w - m_len) // 2
        mx1 = mx0 + m_len
        my0 = (h - m_len) // 2
        my1 = my0 + m_len
        segments.extend([
            # Top mid dash
            ((mx0, y0), (mx1, y0)),
            # Bottom mid dash
            ((mx0, y1), (mx1, y1)),
            # Left mid dash
            ((x0, my0), (x0, my1)),
            # Right mid dash
            ((x1, my0), (x1, my1)),
        ])
    
    # Pass 1: Dark drop-shadow / outer outline for 100% contrast on any floor
    if has_shadow:
        shadow_w = line_width + 2
        for (sx0, sy0), (sx1, sy1) in segments:
            draw.line([(sx0, sy0), (sx1, sy1)], fill=(10, 10, 15, 230), width=shadow_w)
            
    # Pass 2: Bright colored core
    for (sx0, sy0), (sx1, sy1) in segments:
        draw.line([(sx0, sy0), (sx1, sy1)], fill=color + (255,), width=line_width)
        
    out = Image.alpha_composite(out.convert('RGBA'), overlay)
    return out


def draw_rme_badge_style(img: Image.Image,
                         label: str = "AID: 2000",
                         wash_color: tuple = (240, 200, 20, 110),
                         border_color: tuple = (255, 220, 20, 240)) -> Image.Image:
    """Simulates traditional RME indicator style with opaque wash and text badge."""
    out = img.copy().convert('RGBA')
    overlay = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    w, h = out.size
    
    # Heavy semi-opaque wash over entire tile
    draw.rectangle([1, 1, w - 2, h - 2], fill=wash_color)
    draw.rectangle([0, 0, w - 1, h - 1], outline=border_color, width=1)
    out = Image.alpha_composite(out, overlay)
    
    # Big text badge in center
    draw = ImageDraw.Draw(out)
    try:
        font = ImageFont.truetype("arialbd.ttf", max(9, int(w * 0.16)))
    except IOError:
        font = ImageFont.load_default()
        
    bbox = font.getbbox(label)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    bx0, by0 = (w - tw) // 2 - 4, (h - th) // 2 - 3
    bx1, by1 = bx0 + tw + 8, by0 + th + 6
    
    draw.rectangle([bx0, by0, bx1, by1], fill=(20, 20, 25, 240), outline=(240, 220, 40, 255), width=1)
    draw.text(((w - tw) // 2, by0 + 2), label, font=font, fill=(255, 255, 255, 255))
    return out


def draw_micro_pip(img: Image.Image, letter: str = "A", color: tuple = (255, 230, 40)) -> Image.Image:
    """Draws a subtle 12x12 micro-pip badge in the top-right corner."""
    out = img.copy()
    draw = ImageDraw.Draw(out)
    w, _ = out.size
    pw, ph = 14, 14
    px0 = w - pw - 3
    py0 = 3
    draw.rectangle([px0, py0, px0 + pw, py0 + ph], fill=(15, 15, 20, 240), outline=color + (255,), width=1)
    try:
        font = ImageFont.truetype("arialbd.ttf", 9)
    except IOError:
        font = ImageFont.load_default()
    bbox = font.getbbox(letter)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text((px0 + (pw - tw) // 2, py0 + (ph - th) // 2 - 1), letter, font=font, fill=color + (255,))
    return out


def build_prototype_board() -> Image.Image:
    """Generates the comprehensive visual prototype comparison sheet."""
    tile_size = 96  # High-res display size
    
    # Fonts
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

    canvas_w = 1100
    canvas_h = 1060
    board = Image.new('RGB', (canvas_w, canvas_h), (24, 24, 28))
    draw = ImageDraw.Draw(board)
    
    # 1. Header Banner
    draw.rectangle([0, 0, canvas_w, 75], fill=(32, 32, 38))
    draw.line([(0, 75), (canvas_w, 75)], fill=(50, 50, 58), width=1)
    draw.text((30, 16), "RME Redux — CipSoft Lined Square / Reticle Indicator Study", font=title_font, fill=(255, 255, 255))
    draw.text((30, 46), "Analysis of CipSoft map editor's segmented corner-bracket indicators vs. traditional RME badges", font=body_font, fill=(160, 165, 175))
    
    # Helper to composite tile onto board
    def place_tile(tile: Image.Image, x: int, y: int, label: str = "", sublabel: str = ""):
        board.paste(tile, (x, y))
        # 1px border around tile slot
        draw.rectangle([x - 1, y - 1, x + tile.width, y + tile.height], outline=(65, 65, 75), width=1)
        if label:
            draw.text((x + tile.width // 2, y + tile.height + 6), label, font=bold_font, fill=(230, 230, 235), anchor="mt")
        if sublabel:
            draw.text((x + tile.width // 2, y + tile.height + 22), sublabel, font=mono_font, fill=(140, 145, 155), anchor="mt")

    # =========================================================================
    # SECTION 1: The Core Problem — Side-by-Side Comparison
    # =========================================================================
    sec1_y = 95
    draw.text((30, sec1_y), "1. Problem Analysis: Sprite Occlusion vs. Unobstructed Reticle", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec1_y + 24), (canvas_w - 30, sec1_y + 24)], fill=(45, 45, 52), width=1)
    
    # Prepare base tiles with items
    stone = create_stone_tile(tile_size)
    chest = create_chest_sprite(tile_size)
    
    # Tile A: Clean Chest (Unmarked)
    tile_unmarked = stone.copy()
    tile_unmarked.paste(chest, (0, 0), chest)
    
    # Tile B: Traditional RME (Wash + Badge)
    tile_rme = draw_rme_badge_style(tile_unmarked, "AID: 2000")
    
    # Tile C: Authentic CipSoft Style (Corners + Mid Dashes)
    tile_cip = draw_cipsoft_bracket(tile_unmarked, color=(255, 255, 160), style="cipsoft", line_width=2)
    
    # Tile D: Minimalist Corners-Only (Reticle)
    tile_corners = draw_cipsoft_bracket(tile_unmarked, color=(255, 255, 160), style="corners_only", line_width=2)
    
    # Tile E: Bracket + Micro Pip [A]
    tile_pip = draw_cipsoft_bracket(tile_unmarked, color=(255, 255, 160), style="cipsoft", line_width=2)
    tile_pip = draw_micro_pip(tile_pip, "A", (255, 235, 60))

    col_x = 45
    spacing_x = 205
    place_tile(tile_unmarked, col_x, sec1_y + 40, "Unmarked Item", "Raw chest sprite")
    place_tile(tile_rme, col_x + spacing_x, sec1_y + 40, "Current RME Style", "Wash + heavy text badge")
    place_tile(tile_cip, col_x + spacing_x * 2, sec1_y + 40, "CipSoft Style", "Corners + mid dashes")
    place_tile(tile_corners, col_x + spacing_x * 3, sec1_y + 40, "Minimalist CAD", "Corners only (HUD)")
    place_tile(tile_pip, col_x + spacing_x * 4, sec1_y + 40, "Bracket + Micro Pip", "Unblocked + 'A' badge")

    # Callout text below section 1
    callout_y = sec1_y + 190
    draw.rectangle([30, callout_y, canvas_w - 30, callout_y + 45], fill=(30, 32, 40), outline=(50, 55, 70), width=1)
    draw.text((45, callout_y + 8), "Key Insight:", font=bold_font, fill=(90, 180, 255))
    draw.text((130, callout_y + 8), "Traditional RME obscures ~70% of the sprite beneath text and color washes.", font=body_font, fill=(220, 225, 230))
    draw.text((45, callout_y + 25), "CipSoft's open frame preserves 100% of the sprite details (lock, wood, latch) while clearly communicating the marker.", font=body_font, fill=(160, 165, 175))

    # =========================================================================
    # SECTION 2: Color-Coded Semantic Indicators (Different Attributes)
    # =========================================================================
    sec2_y = 360
    draw.text((30, sec2_y), "2. Semantic Color Coding (Identify Attribute Types Without Text)", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec2_y + 24), (canvas_w - 30, sec2_y + 24)], fill=(45, 45, 52), width=1)
    
    # 5 semantic examples:
    # 1. Gold: Action ID / Quest Chest
    gold_tile = draw_cipsoft_bracket(tile_unmarked, color=(255, 220, 40), style="cipsoft", line_width=2)
    # 2. Crimson: Blocking / Tech Wall (Invisible wall)
    block_tile = stone.copy()
    block_tile = draw_cipsoft_bracket(block_tile, color=(255, 50, 70), style="cipsoft", line_width=2)
    # 3. Cyan: Stairs / Teleport
    stair = create_stair_sprite(tile_size)
    stair_base = stone.copy()
    stair_base.paste(stair, (0, 0), stair)
    stair_tile = draw_cipsoft_bracket(stair_base, color=(0, 240, 255), style="cipsoft", line_width=2)
    # 4. Emerald Green: Lever / Interaction Trigger
    lever = create_lever_sprite(tile_size)
    lever_base = stone.copy()
    lever_base.paste(lever, (0, 0), lever)
    lever_tile = draw_cipsoft_bracket(lever_base, color=(50, 255, 90), style="cipsoft", line_width=2)
    # 5. Amethyst Purple: Spawn / Special Script
    spawn_base = stone.copy()
    spawn_tile = draw_cipsoft_bracket(spawn_base, color=(220, 60, 255), style="cipsoft", line_width=2)
    
    place_tile(gold_tile, col_x, sec2_y + 40, "Gold / Yellow", "Action / Unique ID")
    place_tile(block_tile, col_x + spacing_x, sec2_y + 40, "Crimson Red", "Tech BLOCK (1548)")
    place_tile(stair_tile, col_x + spacing_x * 2, sec2_y + 40, "Electric Cyan", "Stairs / Walkable")
    place_tile(lever_tile, col_x + spacing_x * 3, sec2_y + 40, "Emerald Green", "Switch / Interaction")
    place_tile(spawn_tile, col_x + spacing_x * 4, sec2_y + 40, "Amethyst Purple", "Spawn / Script Trigger")

    # =========================================================================
    # SECTION 3: Universal Contrast Across Different Floor Terrains
    # =========================================================================
    sec3_y = 625
    draw.text((30, sec3_y), "3. Contrast Test: Dark Drop-Shadow Ensures Visibility Across All Terrains", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec3_y + 24), (canvas_w - 30, sec3_y + 24)], fill=(45, 45, 52), width=1)
    
    # Terrains
    t_stone = stone.copy()
    t_stone.paste(chest, (0, 0), chest)
    t_stone = draw_cipsoft_bracket(t_stone, color=(255, 255, 160))
    
    t_snow = create_snow_tile(tile_size)
    t_snow.paste(chest, (0, 0), chest)
    t_snow = draw_cipsoft_bracket(t_snow, color=(255, 255, 160))
    
    t_cave = create_cave_tile(tile_size)
    t_cave.paste(chest, (0, 0), chest)
    t_cave = draw_cipsoft_bracket(t_cave, color=(255, 255, 160))
    
    t_sand = create_sand_tile(tile_size)
    t_sand.paste(chest, (0, 0), chest)
    t_sand = draw_cipsoft_bracket(t_sand, color=(255, 255, 160))
    
    # Contrast Failure demonstration: without dark shadow on white snow!
    t_snow_noshadow = create_snow_tile(tile_size)
    t_snow_noshadow.paste(chest, (0, 0), chest)
    t_snow_noshadow = draw_cipsoft_bracket(t_snow_noshadow, color=(255, 255, 160), has_shadow=False)

    place_tile(t_cave, col_x, sec3_y + 40, "Dark Cave / Void", "Bright core pops")
    place_tile(t_stone, col_x + spacing_x, sec3_y + 40, "Grey Dungeon Stone", "Standard contrast")
    place_tile(t_sand, col_x + spacing_x * 2, sec3_y + 40, "Desert Sand", "Warm background")
    place_tile(t_snow, col_x + spacing_x * 3, sec3_y + 40, "Bright White Snow", "Dark shadow saves it!")
    place_tile(t_snow_noshadow, col_x + spacing_x * 4, sec3_y + 40, "NO Shadow (Bad)", "Invisible on snow!")

    # =========================================================================
    # SECTION 4: In-Context Map Multi-Item View (Small Dungeon Room)
    # =========================================================================
    sec4_y = 890
    draw.text((30, sec4_y), "4. In-Context Room Preview: Clean Multi-Item Layout Without Clutter", font=h2_font, fill=(255, 215, 60))
    draw.line([(30, sec4_y + 24), (canvas_w - 30, sec4_y + 24)], fill=(45, 45, 52), width=1)
    
    # Draw a 6-tile horizontal strip showing a dungeon corridor
    strip_w = 6 * tile_size
    strip_h = tile_size
    strip = Image.new('RGB', (strip_w, strip_h))
    
    # 6 tiles:
    # 0: Stairs (Cyan)
    # 1: Clean stone
    # 2: Quest Chest (Gold)
    # 3: Clean stone
    # 4: Lever (Green)
    # 5: Invisible Wall BLOCK (Red)
    s0 = stair_tile
    s1 = stone.copy()
    s2 = gold_tile
    s3 = stone.copy()
    s4 = lever_tile
    s5 = block_tile
    
    for idx, t in enumerate([s0, s1, s2, s3, s4, s5]):
        strip.paste(t, (idx * tile_size, 0))
        draw_rect = [col_x + idx * tile_size, sec4_y + 40, col_x + (idx + 1) * tile_size, sec4_y + 40 + tile_size]
        board.paste(t, (col_x + idx * tile_size, sec4_y + 40))
        draw.rectangle(draw_rect, outline=(55, 55, 65), width=1)
        
    labels = ["Stairs (Cyan)", "Stone Floor", "Quest Chest (Gold)", "Stone Floor", "Lever (Green)", "BLOCK (Red)"]
    for idx, lbl in enumerate(labels):
        tx = col_x + idx * tile_size + tile_size // 2
        draw.text((tx, sec4_y + 40 + tile_size + 6), lbl, font=bold_font, fill=(200, 205, 215), anchor="mt")

    # Legend / takeaway on the right of the strip
    leg_x = col_x + 6 * tile_size + 30
    draw.rectangle([leg_x, sec4_y + 40, canvas_w - 30, sec4_y + 40 + tile_size], fill=(30, 32, 38), outline=(50, 55, 65), width=1)
    draw.text((leg_x + 15, sec4_y + 48), "Notice how calm and readable the map looks:", font=bold_font, fill=(255, 255, 255))
    draw.text((leg_x + 15, sec4_y + 70), "• No giant text boxes blocking the stairs or chest", font=body_font, fill=(180, 185, 195))
    draw.text((leg_x + 15, sec4_y + 90), "• High-tech CAD reticle aesthetic", font=body_font, fill=(180, 185, 195))
    draw.text((leg_x + 15, sec4_y + 110), "• Every marked item is instantly recognized", font=body_font, fill=(180, 185, 195))

    return board


if __name__ == "__main__":
    out_dir = Path("tools/assets")
    out_dir.mkdir(parents=True, exist_ok=True)
    board = build_prototype_board()
    
    out_path = out_dir / "cipsoft_bracket_prototype.png"
    board.save(out_path, quality=95)
    print(f"[Success] Saved prototype comparison to: {out_path.resolve()}")
