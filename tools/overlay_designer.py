#!/usr/bin/env python3
"""
RME Redux - Visual Overlay & GLSL Shader Designer Tool
Enables interactive visual design and tuning of:
- Special Zones (PZ, No-PvP, No-Logout, PvP Zone) wash & connected border colors
- Pathing / Blocking overlay colors & connected borders
- Spawn radius wash & individual bounding borders
- House cross-hatch shading, active house pulse, and 'H' ground emblems
- Technical item shader indicators (BLOCK, STAIR, WALK, LIGHT, ENTRY, TOWN, WAYPT)
- Invalid map content fills (Missing Ground, Missing Top Item, Invalid Zones)

Run:
  python tools/overlay_designer.py                 # Launches desktop GUI (Tkinter + ModernGL / Software)
  python tools/overlay_designer.py --export out.png # Headless export of showcase map
  python tools/overlay_designer.py --dump-glsl      # Print generated C++ GLSL code
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# Try importing ModernGL for GPU GLSL rendering
try:
    import moderngl
    HAS_MODERNGL = True
except ImportError:
    HAS_MODERNGL = False

try:
    import tkinter as tk
    from tkinter import colorchooser, messagebox, ttk
    from PIL import ImageTk
    HAS_TKINTER = True
except ImportError:
    HAS_TKINTER = False


# ==============================================================================
# 1. Shader Color Configuration
# ==============================================================================

@dataclass
class RGBA:
    r: float
    g: float
    b: float
    a: float

    def to_tuple(self) -> Tuple[float, float, float, float]:
        return (self.r, self.g, self.b, self.a)

    def to_hex(self) -> str:
        ir = int(round(max(0.0, min(1.0, self.r)) * 255))
        ig = int(round(max(0.0, min(1.0, self.g)) * 255))
        ib = int(round(max(0.0, min(1.0, self.b)) * 255))
        return f"#{ir:02x}{ig:02x}{ib:02x}"

    def to_glsl_vec4(self) -> str:
        return f"vec4({self.r:.2f}, {self.g:.2f}, {self.b:.2f}, {self.a:.2f})"

    @staticmethod
    def from_hex(hex_str: str, alpha: float = 1.0) -> RGBA:
        hex_str = hex_str.lstrip('#')
        ir = int(hex_str[0:2], 16) / 255.0
        ig = int(hex_str[2:4], 16) / 255.0
        ib = int(hex_str[4:6], 16) / 255.0
        return RGBA(ir, ig, ib, alpha)


@dataclass
class OverlayTheme:
    # Special Zones
    pz_wash: RGBA = field(default_factory=lambda: RGBA(0.95, 0.85, 0.10, 0.28))
    pz_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.90, 0.10, 0.95))

    nopvp_wash: RGBA = field(default_factory=lambda: RGBA(0.15, 0.90, 0.20, 0.28))
    nopvp_border: RGBA = field(default_factory=lambda: RGBA(0.20, 1.00, 0.30, 0.95))

    nolog_wash: RGBA = field(default_factory=lambda: RGBA(1.00, 0.50, 0.05, 0.28))
    nolog_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.55, 0.10, 0.95))

    pvp_wash: RGBA = field(default_factory=lambda: RGBA(0.85, 0.05, 0.25, 0.28))
    pvp_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.15, 0.30, 0.95))

    # Pathing / Blocking
    blocking_wash: RGBA = field(default_factory=lambda: RGBA(0.40, 0.40, 0.40, 0.35))
    blocking_border: RGBA = field(default_factory=lambda: RGBA(0.00, 0.95, 1.00, 0.95))

    # Spawns
    spawn_wash: RGBA = field(default_factory=lambda: RGBA(0.85, 0.15, 0.85, 0.25))
    spawn_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.20, 1.00, 0.95))

    # House Shading
    house_active_wash: RGBA = field(default_factory=lambda: RGBA(0.20, 0.95, 0.20, 0.28))
    house_inactive_wash: RGBA = field(default_factory=lambda: RGBA(1.00, 0.60, 0.00, 0.20))
    house_hatch_spacing: int = 4
    house_hatch_darkness: float = 0.90

    # Indicators
    indicator_entry_border: RGBA = field(default_factory=lambda: RGBA(0.15, 0.50, 1.00, 0.95))
    indicator_entry_bg: RGBA = field(default_factory=lambda: RGBA(0.08, 0.30, 0.80, 0.28))

    indicator_spawn_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.20, 1.00, 1.00))
    indicator_spawn_bg: RGBA = field(default_factory=lambda: RGBA(0.85, 0.15, 0.85, 0.35))

    indicator_town_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.85, 0.00, 1.00))
    indicator_town_bg: RGBA = field(default_factory=lambda: RGBA(1.00, 0.75, 0.10, 0.35))

    indicator_waypoint_border: RGBA = field(default_factory=lambda: RGBA(0.00, 1.00, 1.00, 1.00))
    indicator_waypoint_bg: RGBA = field(default_factory=lambda: RGBA(0.05, 0.80, 0.85, 0.35))

    indicator_stair_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.95, 0.10, 1.00))
    indicator_stair_bg: RGBA = field(default_factory=lambda: RGBA(1.00, 0.90, 0.15, 0.35))

    indicator_walk_border: RGBA = field(default_factory=lambda: RGBA(0.00, 0.95, 0.95, 1.00))
    indicator_walk_bg: RGBA = field(default_factory=lambda: RGBA(0.05, 0.75, 0.85, 0.35))

    indicator_block_border: RGBA = field(default_factory=lambda: RGBA(1.00, 0.15, 0.15, 1.00))
    indicator_block_bg: RGBA = field(default_factory=lambda: RGBA(0.95, 0.20, 0.20, 0.35))

    indicator_light_border: RGBA = field(default_factory=lambda: RGBA(0.35, 0.85, 1.00, 1.00))
    indicator_light_bg: RGBA = field(default_factory=lambda: RGBA(0.30, 0.75, 1.00, 0.35))

    def generate_cpp_zone_shader(self) -> str:
        return f"""// Paste into source/rendering/shaders/zone_shader.h evaluateSpecialZones:
    if ((flags & 4u) != 0u) {{
        // PZ: Protection Zone
        hasZone = true;
        zWash = {self.pz_wash.to_glsl_vec4()};
        zBorder = {self.pz_border.to_glsl_vec4()};
    }} else if ((flags & 8u) != 0u) {{
        // No-PvP Zone
        hasZone = true;
        zWash = {self.nopvp_wash.to_glsl_vec4()};
        zBorder = {self.nopvp_border.to_glsl_vec4()};
    }} else if ((flags & 16u) != 0u) {{
        // No-Logout Zone
        hasZone = true;
        zWash = {self.nolog_wash.to_glsl_vec4()};
        zBorder = {self.nolog_border.to_glsl_vec4()};
    }} else if ((flags & 32u) != 0u) {{
        // PvP Zone
        hasZone = true;
        zWash = {self.pvp_wash.to_glsl_vec4()};
        zBorder = {self.pvp_border.to_glsl_vec4()};
    }}

// Blocking / Pathing:
    outLayer = isBorder ? {self.blocking_border.to_glsl_vec4()} : {self.blocking_wash.to_glsl_vec4()};

// Spawn Radius:
    vec4 spawnWash = {self.spawn_wash.to_glsl_vec4()};
    vec4 spawnBorder = {self.spawn_border.to_glsl_vec4()};"""


# ==============================================================================
# 2. Mock Map & Tile Simulation
# ==============================================================================

# Zone flags matching zone_flags.h
FLAG_BLOCKING       = 1 << 0
FLAG_SPAWN          = 1 << 1
FLAG_PZ             = 1 << 2
FLAG_NOPVP          = 1 << 3
FLAG_NOLOGOUT       = 1 << 4
FLAG_PVPZONE        = 1 << 5

FLAG_BLOCK_BORDER_N = 1 << 6
FLAG_BLOCK_BORDER_S = 1 << 7
FLAG_BLOCK_BORDER_W = 1 << 8
FLAG_BLOCK_BORDER_E = 1 << 9

FLAG_ZONE_BORDER_N  = 1 << 10
FLAG_ZONE_BORDER_S  = 1 << 11
FLAG_ZONE_BORDER_W  = 1 << 12
FLAG_ZONE_BORDER_E  = 1 << 13

FLAG_SPAWN_BORDER_N = 1 << 14
FLAG_SPAWN_BORDER_S = 1 << 15
FLAG_SPAWN_BORDER_W = 1 << 16
FLAG_SPAWN_BORDER_E = 1 << 17


@dataclass
class SimTile:
    base_color: Tuple[int, int, int] = (108, 178, 60) # Grass green
    is_wall: bool = False
    is_water: bool = False
    house_id: int = 0
    is_house_wall: bool = False
    has_pz: bool = False
    has_nopvp: bool = False
    has_nolog: bool = False
    has_pvp: bool = False
    has_spawn: bool = False
    spawn_id: int = 0
    technical_badge: Optional[str] = None # 'BLOCK', 'STAIR', 'WALK', 'LIGHT', 'ENTRY', 'TOWN', 'WAYPT', 'INV_GROUND', 'INV_ITEM', 'INV_ZONE'


class ShowcaseMap:
    """Pre-built map demonstrating every zone, indicator, house, and blocking condition."""
    def __init__(self, width: int = 24, height: int = 16):
        self.width = width
        self.height = height
        self.tiles: List[List[SimTile]] = [[SimTile() for _ in range(width)] for _ in range(height)]
        self.setup_showcase()

    def setup_showcase(self):
        # 1. House Room with active house (house 101)
        for y in range(2, 7):
            for x in range(2, 8):
                t = self.tiles[y][x]
                t.base_color = (210, 185, 140) # Wood floor
                t.house_id = 101
                if y == 2 or y == 6 or x == 2 or x == 7:
                    if not (y == 6 and x == 4): # Doorway
                        t.is_wall = True
                        t.is_house_wall = True
                        t.base_color = (130, 95, 70)
        # Doorway has house entry badge
        self.tiles[6][4].technical_badge = 'ENTRY'

        # 2. Protection Zone (Depot / Temple area)
        for y in range(2, 8):
            for x in range(10, 16):
                t = self.tiles[y][x]
                t.base_color = (180, 180, 185) # Stone tiles
                t.has_pz = True
        self.tiles[4][13].technical_badge = 'TOWN'

        # 3. No-PvP Zone (Arena Spectator)
        for y in range(2, 8):
            for x in range(17, 22):
                t = self.tiles[y][x]
                t.base_color = (175, 195, 165)
                t.has_nopvp = True

        # 4. PvP Zone & No-Logout
        for y in range(9, 14):
            for x in range(2, 7):
                t = self.tiles[y][x]
                t.base_color = (195, 160, 160)
                t.has_pvp = True
            for x in range(7, 10):
                t = self.tiles[y][x]
                t.base_color = (210, 190, 150)
                t.has_nolog = True

        # 5. Spawn Area (3x3 with center spawn)
        for y in range(9, 14):
            for x in range(11, 16):
                t = self.tiles[y][x]
                t.has_spawn = True
                t.spawn_id = 1
        self.tiles[11][13].technical_badge = 'SPAWN'

        # 6. Technical items & invalid tiles row
        tech_badges = [
            (9, 17, 'BLOCK', (108, 178, 60)),
            (9, 19, 'STAIR', (108, 178, 60)),
            (9, 21, 'WALK', (108, 178, 60)),
            (11, 17, 'LIGHT', (108, 178, 60)),
            (11, 19, 'WAYPT', (108, 178, 60)),
            (11, 21, 'INV_GROUND', (40, 40, 40)),
            (13, 17, 'INV_ITEM', (108, 178, 60)),
            (13, 19, 'INV_ZONE', (108, 178, 60)),
        ]
        for y, x, badge, color in tech_badges:
            self.tiles[y][x].technical_badge = badge
            self.tiles[y][x].base_color = color

        # 7. Pathing blocking wall & water obstacle
        for y in range(13, 16):
            for x in range(22, 24):
                self.tiles[y][x].is_wall = True
                self.tiles[y][x].base_color = (90, 90, 100)


# ==============================================================================
# 3. High-Fidelity GLSL & Software Shader Renderer
# ==============================================================================

class OverlayRenderer:
    """Renders overlays with pixel-exact emulation of zone_shader, house_shader, and indicator_shader."""

    def __init__(self, theme: OverlayTheme):
        self.theme = theme
        self.gl_ctx: Optional[Any] = None
        self.gl_program: Optional[Any] = None
        if HAS_MODERNGL:
            try:
                self.gl_ctx = moderngl.create_context(standalone=True)
            except Exception as e:
                print(f"[Warning] ModernGL standalone init failed: {e}. Falling back to software renderer.")

    def compute_tile_flags(self, smap: ShowcaseMap, show_blocking: bool, show_zones: bool, show_spawns: bool) -> np.ndarray:
        w, h = smap.width, smap.height
        flags = np.zeros((h, w), dtype=np.uint32)

        def is_path_blocking(tile: SimTile) -> bool:
            # Exclude invisible wall ('BLOCK') from path blocking!
            if tile.technical_badge == 'BLOCK' and not tile.is_wall and not tile.is_water:
                return False
            return tile.is_wall or tile.is_water

        for y in range(h):
            for x in range(w):
                t = smap.tiles[y][x]
                f = 0

                # Pathing / Blocking
                if show_blocking and is_path_blocking(t):
                    f |= FLAG_BLOCKING
                    if y == 0 or not is_path_blocking(smap.tiles[y - 1][x]):   f |= FLAG_BLOCK_BORDER_N
                    if y == h - 1 or not is_path_blocking(smap.tiles[y + 1][x]): f |= FLAG_BLOCK_BORDER_S
                    if x == 0 or not is_path_blocking(smap.tiles[y][x - 1]):   f |= FLAG_BLOCK_BORDER_W
                    if x == w - 1 or not is_path_blocking(smap.tiles[y][x + 1]): f |= FLAG_BLOCK_BORDER_E

                # Special Zones
                if show_zones:
                    has_zone = False
                    if t.has_pz:
                        f |= FLAG_PZ; has_zone = True
                    elif t.has_nopvp:
                        f |= FLAG_NOPVP; has_zone = True
                    elif t.has_nolog:
                        f |= FLAG_NOLOGOUT; has_zone = True
                    elif t.has_pvp:
                        f |= FLAG_PVPZONE; has_zone = True

                    if has_zone:
                        def same_zone(other: SimTile) -> bool:
                            if t.has_pz: return other.has_pz
                            if t.has_nopvp: return other.has_nopvp
                            if t.has_nolog: return other.has_nolog
                            if t.has_pvp: return other.has_pvp
                            return False

                        if y == 0 or not same_zone(smap.tiles[y - 1][x]):   f |= FLAG_ZONE_BORDER_N
                        if y == h - 1 or not same_zone(smap.tiles[y + 1][x]): f |= FLAG_ZONE_BORDER_S
                        if x == 0 or not same_zone(smap.tiles[y][x - 1]):   f |= FLAG_ZONE_BORDER_W
                        if x == w - 1 or not same_zone(smap.tiles[y][x + 1]): f |= FLAG_ZONE_BORDER_E

                # Spawns
                if show_spawns and t.has_spawn:
                    f |= FLAG_SPAWN
                    def same_spawn(other: SimTile) -> bool:
                        return other.has_spawn and other.spawn_id == t.spawn_id

                    if y == 0 or not same_spawn(smap.tiles[y - 1][x]):   f |= FLAG_SPAWN_BORDER_N
                    if y == h - 1 or not same_spawn(smap.tiles[y + 1][x]): f |= FLAG_SPAWN_BORDER_S
                    if x == 0 or not same_spawn(smap.tiles[y][x - 1]):   f |= FLAG_SPAWN_BORDER_W
                    if x == w - 1 or not same_spawn(smap.tiles[y][x + 1]): f |= FLAG_SPAWN_BORDER_E

                flags[y, x] = f

        return flags

    def render(self, smap: ShowcaseMap,
               show_blocking: bool = True,
               show_zones: bool = True,
               show_spawns: bool = True,
               show_houses: bool = True,
               show_tech: bool = True,
               scale: int = 2) -> Image.Image:
        """Renders the showcase map using exact shader logic at 32px per tile scaled by `scale`."""
        tile_px = 32
        img_w = smap.width * tile_px
        img_h = smap.height * tile_px

        # Base image buffer: RGBA float [0.0, 1.0]
        buf = np.zeros((img_h, img_w, 4), dtype=np.float32)

        # 1. Fill base tile colors & grid
        for ty in range(smap.height):
            for tx in range(smap.width):
                t = smap.tiles[ty][tx]
                cr, cg, cb = [c / 255.0 for c in t.base_color]
                y0, y1 = ty * tile_px, (ty + 1) * tile_px
                x0, x1 = tx * tile_px, (tx + 1) * tile_px
                buf[y0:y1, x0:x1, 0] = cr
                buf[y0:y1, x0:x1, 1] = cg
                buf[y0:y1, x0:x1, 2] = cb
                buf[y0:y1, x0:x1, 3] = 1.0

                # Subtle grid line
                buf[y0, x0:x1, :3] *= 0.92
                buf[y0:y1, x0, :3] *= 0.92

        # 2. House Shading Layer (applyHouseOverlay)
        if show_houses:
            for ty in range(smap.height):
                for tx in range(smap.width):
                    t = smap.tiles[ty][tx]
                    if t.house_id == 0:
                        continue
                    is_active = (t.house_id == 101)
                    wash = self.theme.house_active_wash if is_active else self.theme.house_inactive_wash
                    y0, y1 = ty * tile_px, (ty + 1) * tile_px
                    x0, x1 = tx * tile_px, (tx + 1) * tile_px

                    # Atmosphere wash
                    buf[y0:y1, x0:x1, :3] = (
                        buf[y0:y1, x0:x1, :3] * (1.0 - wash.a) +
                        np.array([wash.r, wash.g, wash.b]) * wash.a
                    )

                    if t.is_house_wall:
                        # Diagonal cross hatching (items/walls)
                        spacing = max(2, self.theme.house_hatch_spacing)
                        hatch_a = self.theme.house_hatch_darkness
                        hatch_col = np.array([0.01, 0.05, 0.02]) if is_active else np.array([0.08, 0.04, 0.01])

                        for py in range(y0, y1):
                            for px in range(x0, x1):
                                if (px + py) % spacing == 0:
                                    buf[py, px, :3] = buf[py, px, :3] * (1.0 - hatch_a) + hatch_col * hatch_a
                    else:
                        # Ground 'H' emblem
                        for py in range(y0, y1):
                            ly = py - y0
                            for px in range(x0, x1):
                                lx = px - x0
                                dx = abs(lx - 16)
                                dy = abs(ly - 16)
                                is_inside = (dx >= 2 and dx <= 3 and dy <= 5) or (dx < 2 and dy <= 1)
                                is_outline = ((dy == 6 and dx >= 1 and dx <= 4) or
                                              (dx == 4 and dy <= 5) or
                                              (dx == 1 and dy >= 2 and dy <= 5) or
                                              (dx == 0 and dy == 2))
                                if is_inside:
                                    col = np.array([0.55, 1.00, 0.55]) if is_active else np.array([1.00, 0.80, 0.40])
                                    buf[py, px, :3] = buf[py, px, :3] * 0.05 + col * 0.95
                                elif is_outline:
                                    col = np.array([0.04, 0.32, 0.08]) if is_active else np.array([0.40, 0.20, 0.00])
                                    buf[py, px, :3] = buf[py, px, :3] * 0.05 + col * 0.95

        # 3. Dedicated On-Top Overlay Pass (zone_shader.h: evaluateZoneOverlay)
        tile_flags = self.compute_tile_flags(smap, show_blocking, show_zones, show_spawns)

        for ty in range(smap.height):
            for tx in range(smap.width):
                fl = tile_flags[ty, tx]
                if fl == 0:
                    continue

                y0, y1 = ty * tile_px, (ty + 1) * tile_px
                x0, x1 = tx * tile_px, (tx + 1) * tile_px

                for py in range(y0, y1):
                    ly = py - y0
                    b_north = (ly == 0)
                    b_south = (ly == 31)

                    for px in range(x0, x1):
                        lx = px - x0
                        b_west = (lx == 0)
                        b_east = (lx == 31)

                        # A. Pathing / Blocking
                        if (fl & FLAG_BLOCKING) != 0:
                            is_border = (
                                (b_north and (fl & FLAG_BLOCK_BORDER_N) != 0) or
                                (b_south and (fl & FLAG_BLOCK_BORDER_S) != 0) or
                                (b_west  and (fl & FLAG_BLOCK_BORDER_W) != 0) or
                                (b_east  and (fl & FLAG_BLOCK_BORDER_E) != 0)
                            )
                            c = self.theme.blocking_border if is_border else self.theme.blocking_wash
                            buf[py, px, :3] = buf[py, px, :3] * (1.0 - c.a) + np.array([c.r, c.g, c.b]) * c.a

                        # B. Special Zones (PZ, No-PvP, No-Logout, PvP)
                        if (fl & (FLAG_PZ | FLAG_NOPVP | FLAG_NOLOGOUT | FLAG_PVPZONE)) != 0:
                            if (fl & FLAG_PZ) != 0:
                                zw, zb = self.theme.pz_wash, self.theme.pz_border
                            elif (fl & FLAG_NOPVP) != 0:
                                zw, zb = self.theme.nopvp_wash, self.theme.nopvp_border
                            elif (fl & FLAG_NOLOGOUT) != 0:
                                zw, zb = self.theme.nolog_wash, self.theme.nolog_border
                            else:
                                zw, zb = self.theme.pvp_wash, self.theme.pvp_border

                            is_border = (
                                (b_north and (fl & FLAG_ZONE_BORDER_N) != 0) or
                                (b_south and (fl & FLAG_ZONE_BORDER_S) != 0) or
                                (b_west  and (fl & FLAG_ZONE_BORDER_W) != 0) or
                                (b_east  and (fl & FLAG_ZONE_BORDER_E) != 0)
                            )
                            c = zb if is_border else zw
                            buf[py, px, :3] = buf[py, px, :3] * (1.0 - c.a) + np.array([c.r, c.g, c.b]) * c.a

                        # C. Spawns
                        if (fl & FLAG_SPAWN) != 0:
                            is_border = (
                                (b_north and (fl & FLAG_SPAWN_BORDER_N) != 0) or
                                (b_south and (fl & FLAG_SPAWN_BORDER_S) != 0) or
                                (b_west  and (fl & FLAG_SPAWN_BORDER_W) != 0) or
                                (b_east  and (fl & FLAG_SPAWN_BORDER_E) != 0)
                            )
                            c = self.theme.spawn_border if is_border else self.theme.spawn_wash
                            buf[py, px, :3] = buf[py, px, :3] * (1.0 - c.a) + np.array([c.r, c.g, c.b]) * c.a

        # Convert to 8-bit PIL Image
        img_np = np.clip(buf * 255.0, 0, 255).astype(np.uint8)
        img = Image.fromarray(img_np, mode='RGBA')

        # 4. Technical Item Indicators (Crisp NanoVG badge styling)
        if show_tech:
            draw = ImageDraw.Draw(img)
            try:
                font = ImageFont.truetype("arial.ttf", 9)
                font_bold = ImageFont.truetype("arialbd.ttf", 9)
            except IOError:
                font = ImageFont.load_default()
                font_bold = font

            badge_styles = {
                'ENTRY': (self.theme.indicator_entry_border, self.theme.indicator_entry_bg, "ENTRY"),
                'SPAWN': (self.theme.indicator_spawn_border, self.theme.indicator_spawn_bg, "SPAWN"),
                'TOWN':  (self.theme.indicator_town_border, self.theme.indicator_town_bg, "TOWN"),
                'WAYPT': (self.theme.indicator_waypoint_border, self.theme.indicator_waypoint_bg, "WAYPT"),
                'STAIR': (self.theme.indicator_stair_border, self.theme.indicator_stair_bg, "STAIR"),
                'WALK':  (self.theme.indicator_walk_border, self.theme.indicator_walk_bg, "WALK"),
                'BLOCK': (self.theme.indicator_block_border, self.theme.indicator_block_bg, "BLOCK"),
                'LIGHT': (self.theme.indicator_light_border, self.theme.indicator_light_bg, "LIGHT"),
                'INV_GROUND': (RGBA(1.0, 0.0, 0.0, 1.0), RGBA(1.0, 0.0, 0.0, 0.67), ""),
                'INV_ITEM':   (RGBA(1.0, 0.65, 0.0, 1.0), RGBA(1.0, 0.65, 0.0, 0.67), ""),
                'INV_ZONE':   (RGBA(1.0, 0.0, 1.0, 1.0), RGBA(1.0, 0.0, 1.0, 0.67), ""),
            }

            for ty in range(smap.height):
                for tx in range(smap.width):
                    t = smap.tiles[ty][tx]
                    if not t.technical_badge:
                        continue
                    style = badge_styles.get(t.technical_badge)
                    if not style:
                        continue
                    border_c, bg_c, label = style
                    x0, y0 = tx * tile_px, ty * tile_px
                    x1, y1 = x0 + tile_px - 1, y0 + tile_px - 1

                    # Semi-transparent overlay with PIL
                    overlay = Image.new('RGBA', img.size, (0, 0, 0, 0))
                    odraw = ImageDraw.Draw(overlay)

                    bg_rgba = (int(bg_c.r * 255), int(bg_c.g * 255), int(bg_c.b * 255), int(bg_c.a * 255))
                    bdr_rgba = (int(border_c.r * 255), int(border_c.g * 255), int(border_c.b * 255), int(border_c.a * 255))

                    if label:
                        # 1px border + wash + label
                        odraw.rectangle([x0 + 1, y0 + 1, x1 - 1, y1 - 1], fill=bg_rgba)
                        odraw.rectangle([x0, y0, x1, y1], outline=bdr_rgba, width=1)
                        img = Image.alpha_composite(img, overlay)
                        draw = ImageDraw.Draw(img)

                        # Centered shadowed text
                        bbox = font_bold.getbbox(label)
                        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
                        tx_pos = x0 + (tile_px - tw) // 2
                        ty_pos = y0 + (tile_px - th) // 2 - 1

                        for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                            draw.text((tx_pos + dx, ty_pos + dy), label, font=font_bold, fill=(10, 10, 20, 240))
                        draw.text((tx_pos, ty_pos), label, font=font_bold, fill=(255, 255, 255, 255))
                    else:
                        # Flat invalid content fill
                        odraw.rectangle([x0, y0, x1, y1], fill=bg_rgba)
                        img = Image.alpha_composite(img, overlay)
                        draw = ImageDraw.Draw(img)

        if scale != 1:
            img = img.resize((img_w * scale, img_h * scale), Image.Resampling.NEAREST)

        return img


# ==============================================================================
# 4. Interactive Desktop GUI (Tkinter)
# ==============================================================================

class OverlayDesignerGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("RME Redux - Visual Overlay & GLSL Shader Designer")
        self.root.geometry("1200x820")
        self.root.minsize(980, 680)

        self.theme = OverlayTheme()
        self.smap = ShowcaseMap(24, 16)
        self.renderer = OverlayRenderer(self.theme)

        # Options
        self.show_blocking = tk.BooleanVar(value=True)
        self.show_zones = tk.BooleanVar(value=True)
        self.show_spawns = tk.BooleanVar(value=True)
        self.show_houses = tk.BooleanVar(value=True)
        self.show_tech = tk.BooleanVar(value=True)
        self.zoom_scale = tk.IntVar(value=2)
        self.current_tool = tk.StringVar(value="select")

        self.build_ui()
        self.update_preview()

    def build_ui(self):
        # Main split container: Left Controls (Scrollable) | Right Canvas
        main_paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main_paned.pack(fill=tk.BOTH, expand=True)

        # Left Control Frame
        left_frame = ttk.Frame(main_paned, width=420)
        main_paned.add(left_frame, weight=0)

        # Right Preview Frame
        right_frame = ttk.Frame(main_paned)
        main_paned.add(right_frame, weight=1)

        # Build Left Notebook
        notebook = ttk.Notebook(left_frame)
        notebook.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        # Tabs
        self.tab_zones = ttk.Frame(notebook)
        self.tab_blocking = ttk.Frame(notebook)
        self.tab_houses = ttk.Frame(notebook)
        self.tab_indicators = ttk.Frame(notebook)
        self.tab_export = ttk.Frame(notebook)

        notebook.add(self.tab_zones, text="Zones")
        notebook.add(self.tab_blocking, text="Blocking & Spawns")
        notebook.add(self.tab_houses, text="Houses")
        notebook.add(self.tab_indicators, text="Indicators")
        notebook.add(self.tab_export, text="Export C++ GLSL")

        self.build_zones_tab()
        self.build_blocking_tab()
        self.build_houses_tab()
        self.build_indicators_tab()
        self.build_export_tab()

        # Build Right Preview Area
        # Top toolbar
        toolbar = ttk.Frame(right_frame)
        toolbar.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(toolbar, text="Layer Toggles:").pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(toolbar, text="Special Zones", variable=self.show_zones, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Pathing/Blocking", variable=self.show_blocking, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Spawns", variable=self.show_spawns, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Houses", variable=self.show_houses, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Tech Items", variable=self.show_tech, command=self.update_preview).pack(side=tk.LEFT, padx=3)

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)

        ttk.Label(toolbar, text="Zoom:").pack(side=tk.LEFT, padx=4)
        for z in [1, 2, 3]:
            ttk.Radiobutton(toolbar, text=f"{z*100}%", variable=self.zoom_scale, value=z, command=self.update_preview).pack(side=tk.LEFT)

        # Canvas with Scrollbars
        canvas_container = ttk.Frame(right_frame)
        canvas_container.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.canvas = tk.Canvas(canvas_container, bg="#1e1e1e", highlightthickness=0)
        self.v_scroll = ttk.Scrollbar(canvas_container, orient=tk.VERTICAL, command=self.canvas.yview)
        self.h_scroll = ttk.Scrollbar(canvas_container, orient=tk.HORIZONTAL, command=self.canvas.xview)
        self.canvas.configure(xscrollcommand=self.h_scroll.set, yscrollcommand=self.v_scroll.set)

        self.v_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.h_scroll.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.canvas.bind("<Button-1>", self.on_canvas_click)
        self.canvas.bind("<B1-Motion>", self.on_canvas_click)

        # Status Bar
        self.status_bar = ttk.Label(right_frame, text="Ready. Click on canvas with selected tool to paint tiles.", relief=tk.SUNKEN, anchor=tk.W)
        self.status_bar.pack(fill=tk.X, side=tk.BOTTOM, padx=4, pady=2)

    def create_rgba_controls(self, parent: ttk.Frame, label_text: str, rgba_obj: RGBA):
        row = ttk.LabelFrame(parent, text=label_text)
        row.pack(fill=tk.X, padx=4, pady=3)

        # Swatch button
        swatch = tk.Button(row, bg=rgba_obj.to_hex(), width=4, relief=tk.RAISED)

        def pick_color():
            chosen = colorchooser.askcolor(color=rgba_obj.to_hex())
            if chosen and chosen[1]:
                hex_c = chosen[1]
                new_c = RGBA.from_hex(hex_c, rgba_obj.a)
                rgba_obj.r = new_c.r
                rgba_obj.g = new_c.g
                rgba_obj.b = new_c.b
                swatch.config(bg=hex_c)
                self.update_preview()

        swatch.config(command=pick_color)
        swatch.grid(row=0, column=0, padx=6, pady=4)

        # Alpha slider
        ttk.Label(row, text="Opacity (Alpha):").grid(row=0, column=1, padx=4)
        alpha_var = tk.DoubleVar(value=rgba_obj.a)

        def on_alpha(val):
            rgba_obj.a = float(val)
            self.update_preview()

        scale = ttk.Scale(row, from_=0.0, to=1.0, value=rgba_obj.a, command=on_alpha)
        scale.grid(row=0, column=2, padx=4, sticky="ew")
        row.columnconfigure(2, weight=1)

    def build_zones_tab(self):
        f = ttk.Frame(self.tab_zones)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="Protection Zone (PZ):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "PZ Wash (Background Fill)", self.theme.pz_wash)
        self.create_rgba_controls(f, "PZ Border (Connected Boundary)", self.theme.pz_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="No-Combat Zone (No-PvP):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "No-PvP Wash (Background Fill)", self.theme.nopvp_wash)
        self.create_rgba_controls(f, "No-PvP Border (Connected Boundary)", self.theme.nopvp_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="No-Logout Zone:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "No-Logout Wash", self.theme.nolog_wash)
        self.create_rgba_controls(f, "No-Logout Border", self.theme.nolog_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="PvP Zone (Hardcore):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "PvP Zone Wash", self.theme.pvp_wash)
        self.create_rgba_controls(f, "PvP Zone Border", self.theme.pvp_border)

    def build_blocking_tab(self):
        f = ttk.Frame(self.tab_blocking)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="Pathing / Blocking Overlay:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Blocking Wash (Terrain Unpassable)", self.theme.blocking_wash)
        self.create_rgba_controls(f, "Blocking Border (Outer Boundary)", self.theme.blocking_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=8)

        ttk.Label(f, text="Spawn Areas:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Spawn Radius Wash", self.theme.spawn_wash)
        self.create_rgba_controls(f, "Spawn Boundary Border", self.theme.spawn_border)

    def build_houses_tab(self):
        f = ttk.Frame(self.tab_houses)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="House Shading & Atmosphere:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Active House Wash (Selected)", self.theme.house_active_wash)
        self.create_rgba_controls(f, "Inactive House Wash (Default)", self.theme.house_inactive_wash)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=8)

        # Cross-hatch controls
        hf = ttk.LabelFrame(f, text="Wall / Extended Item Cross-Hatch")
        hf.pack(fill=tk.X, padx=4, pady=4)

        ttk.Label(hf, text="Hatch Line Spacing (px):").grid(row=0, column=0, padx=4, pady=4, sticky=tk.W)
        sp_var = tk.IntVar(value=self.theme.house_hatch_spacing)
        sp_spin = ttk.Spinbox(hf, from_=2, to=12, textvariable=sp_var, width=5)
        sp_spin.grid(row=0, column=1, padx=4)

        def on_spacing():
            self.theme.house_hatch_spacing = sp_var.get()
            self.update_preview()
        sp_spin.config(command=on_spacing)

        ttk.Label(hf, text="Hatch Darkness:").grid(row=1, column=0, padx=4, pady=4, sticky=tk.W)
        dark_scale = ttk.Scale(hf, from_=0.2, to=1.0, value=self.theme.house_hatch_darkness,
                               command=lambda v: [setattr(self.theme, 'house_hatch_darkness', float(v)), self.update_preview()])
        dark_scale.grid(row=1, column=1, padx=4, sticky="ew")

    def build_indicators_tab(self):
        f = ttk.Frame(self.tab_indicators)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="Shader Indicator Badges:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "BLOCK (Invisible Wall 1548)", self.theme.indicator_block_border)
        self.create_rgba_controls(f, "STAIR (Invisible Stairs)", self.theme.indicator_stair_border)
        self.create_rgba_controls(f, "WALK (Invisible Walkable)", self.theme.indicator_walk_border)
        self.create_rgba_controls(f, "LIGHT (Primal Light)", self.theme.indicator_light_border)
        self.create_rgba_controls(f, "ENTRY (House Entry)", self.theme.indicator_entry_border)
        self.create_rgba_controls(f, "TOWN (Town Temple)", self.theme.indicator_town_border)
        self.create_rgba_controls(f, "WAYPT (Waypoint)", self.theme.indicator_waypoint_border)

    def build_export_tab(self):
        f = ttk.Frame(self.tab_export)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="Generated C++ GLSL Code:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))

        self.txt_glsl = tk.Text(f, height=18, wrap=tk.NONE, font=("Consolas", 9), bg="#181818", fg="#d4d4d4")
        self.txt_glsl.pack(fill=tk.BOTH, expand=True, pady=4)

        btn_bar = ttk.Frame(f)
        btn_bar.pack(fill=tk.X, pady=4)

        def copy_glsl():
            self.root.clipboard_clear()
            self.root.clipboard_append(self.txt_glsl.get("1.0", tk.END))
            messagebox.showinfo("Copied", "C++ GLSL shader snippet copied to clipboard!")

        ttk.Button(btn_bar, text="📋 Copy GLSL to Clipboard", command=copy_glsl).pack(side=tk.LEFT, padx=4)

        def apply_to_cpp():
            ans = messagebox.askyesno("Update C++", "Automatically write updated colors to source/rendering/shaders/zone_shader.h?")
            if ans:
                self.save_to_cpp_shader()

        ttk.Button(btn_bar, text="💾 Update zone_shader.h Directly", command=apply_to_cpp).pack(side=tk.LEFT, padx=4)

    def save_to_cpp_shader(self):
        shader_path = Path(__file__).resolve().parent.parent / "source" / "rendering" / "shaders" / "zone_shader.h"
        if not shader_path.exists():
            messagebox.showerror("Error", f"Shader header not found: {shader_path}")
            return

        content = shader_path.read_text(encoding="utf-8")
        # Replace colors in zone_shader.h
        import re
        content = re.sub(r'zWash = vec4\([^)]+\); \s*// PZ', f'zWash = {self.theme.pz_wash.to_glsl_vec4()};', content)
        content = re.sub(r'zBorder = vec4\([^)]+\); \s*// PZ', f'zBorder = {self.theme.pz_border.to_glsl_vec4()};', content)
        content = re.sub(r'zWash = vec4\([^)]+\); \s*// No-PvP', f'zWash = {self.theme.nopvp_wash.to_glsl_vec4()};', content)
        content = re.sub(r'zBorder = vec4\([^)]+\); \s*// No-PvP', f'zBorder = {self.theme.nopvp_border.to_glsl_vec4()};', content)

        shader_path.write_text(content, encoding="utf-8")
        messagebox.showinfo("Success", f"Updated {shader_path.name} successfully!")

    def on_canvas_click(self, event):
        scale = self.zoom_scale.get()
        tile_px = 32 * scale
        cx = self.canvas.canvasx(event.x)
        cy = self.canvas.canvasy(event.y)
        tx = int(cx // tile_px)
        ty = int(cy // tile_px)

        if 0 <= tx < self.smap.width and 0 <= ty < self.smap.height:
            tile = self.smap.tiles[ty][tx]
            tool = self.current_tool.get()
            if tool == "pz":
                tile.has_pz = not tile.has_pz
            elif tool == "nopvp":
                tile.has_nopvp = not tile.has_nopvp
            elif tool == "block":
                tile.is_wall = not tile.is_wall
            self.update_preview()
            self.status_bar.config(text=f"Tile ({tx}, {ty}) | Wall: {tile.is_wall} | PZ: {tile.has_pz} | No-PvP: {tile.has_nopvp}")

    def update_preview(self):
        scale = self.zoom_scale.get()
        img = self.renderer.render(
            self.smap,
            show_blocking=self.show_blocking.get(),
            show_zones=self.show_zones.get(),
            show_spawns=self.show_spawns.get(),
            show_houses=self.show_houses.get(),
            show_tech=self.show_tech.get(),
            scale=scale
        )
        self.preview_image = ImageTk.PhotoImage(img)
        self.canvas.delete("all")
        self.canvas.create_image(0, 0, anchor=tk.NW, image=self.preview_image)
        self.canvas.config(scrollregion=(0, 0, img.width, img.height))

        # Update GLSL code box
        glsl_code = self.theme.generate_cpp_zone_shader()
        if hasattr(self, 'txt_glsl'):
            self.txt_glsl.delete("1.0", tk.END)
            self.txt_glsl.insert("1.0", glsl_code)


# ==============================================================================
# 5. CLI Entrypoint
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="RME Redux Visual Overlay & GLSL Shader Designer")
    parser.add_argument("--export", type=str, help="Export rendered showcase image to file and exit")
    parser.add_argument("--dump-glsl", action="store_true", help="Print current C++ GLSL code and exit")
    parser.add_argument("--scale", type=int, default=2, help="Image export scale factor (1=32px, 2=64px, 3=96px)")
    args = parser.parse_args()

    theme = OverlayTheme()
    smap = ShowcaseMap(24, 16)
    renderer = OverlayRenderer(theme)

    if args.dump_glsl:
        print(theme.generate_cpp_zone_shader())
        return 0

    if args.export:
        out_path = Path(args.export).resolve()
        img = renderer.render(smap, scale=args.scale)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        img.save(out_path)
        print(f"[Success] Exported showcase map to: {out_path}")
        return 0

    if not HAS_TKINTER:
        print("[Error] Tkinter is not available in this Python environment.")
        print("You can still use --export to render images: python tools/overlay_designer.py --export showcase.png")
        return 1

    root = tk.Tk()
    app = OverlayDesignerGUI(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
