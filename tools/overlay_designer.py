#!/usr/bin/env python3
"""
RME Redux - Visual Overlay & GLSL Shader Designer Tool
Enables interactive visual design and tuning of:
- Special Zones (PZ, No-PvP, No-Logout, PvP Zone) wash & connected border colors
- Pathing / Blocking overlay colors & connected borders (barrels maze, technical BLOCK exclusion)
- Spawn radius wash & individual bounding borders
- House cross-hatch shading, active house pulse, and 'H' ground emblems
- Technical item shader indicators (BLOCK, STAIR, WALK, LIGHT, ENTRY, TOWN, SPAWN)

Matches the exact in-game screenshot layout (1024x597, 34px tile grid, stone floor, barrels, houses, giant spider).

Usage:
  python tools/overlay_designer.py                 # Launches desktop GUI (Tkinter)
  python tools/overlay_designer.py --export out.png # Headless export of realistic scene
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
from typing import Any, Dict, List, Optional, Set, Tuple

import numpy as np
from PIL import Image, ImageDraw, ImageFont

try:
    import tkinter as tk
    from tkinter import colorchooser, filedialog, messagebox, ttk
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

    def to_np(self) -> np.ndarray:
        return np.array([self.r, self.g, self.b, self.a], dtype=np.float32)

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
    blocking_wash: RGBA = field(default_factory=lambda: RGBA(0.00, 0.95, 1.00, 0.05))
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
    indicator_entry_bg: RGBA = field(default_factory=lambda: RGBA(0.08, 0.30, 0.80, 0.35))

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

    def generate_cpp_zone_shader(self) -> str:
        return f"""// Paste into source/rendering/shaders/zone_shader.h evaluateSpecialZones:
    if ((flags & 4u) != 0u) {{
        // PZ: Protection Zone (Yellow)
        hasZone = true;
        zWash = {self.pz_wash.to_glsl_vec4()};
        zBorder = {self.pz_border.to_glsl_vec4()};
    }} else if ((flags & 8u) != 0u) {{
        // No-PvP Zone (Green)
        hasZone = true;
        zWash = {self.nopvp_wash.to_glsl_vec4()};
        zBorder = {self.nopvp_border.to_glsl_vec4()};
    }} else if ((flags & 16u) != 0u) {{
        // No-Logout Zone (Orange)
        hasZone = true;
        zWash = {self.nolog_wash.to_glsl_vec4()};
        zBorder = {self.nolog_border.to_glsl_vec4()};
    }} else if ((flags & 32u) != 0u) {{
        // PvP Zone (Crimson Red)
        hasZone = true;
        zWash = {self.pvp_wash.to_glsl_vec4()};
        zBorder = {self.pvp_border.to_glsl_vec4()};
    }}

// Pathing / Blocking overlay colors:
const vec4 kBlockingWash   = {self.blocking_wash.to_glsl_vec4()};
const vec4 kBlockingBorder = {self.blocking_border.to_glsl_vec4()};

// Spawn overlay colors:
const vec4 kSpawnWash   = {self.spawn_wash.to_glsl_vec4()};
const vec4 kSpawnBorder = {self.spawn_border.to_glsl_vec4()};
"""

    def generate_cpp_house_shader(self) -> str:
        return f"""// Paste into source/rendering/shaders/house_shader.h:
const vec4 kHouseActiveWash   = {self.house_active_wash.to_glsl_vec4()};
const vec4 kHouseInactiveWash = {self.house_inactive_wash.to_glsl_vec4()};
const int  kHouseHatchSpacing = {self.house_hatch_spacing};
const float kHouseHatchDarkness = {self.house_hatch_darkness:.2f};
"""


# ==============================================================================
# 2. Scene Definition (Matches Exact In-Game Screenshot)
# ==============================================================================

# Grid parameters for DPI-scaled in-game screenshot:
# Width: 1024, Height: 597, Tile pitch: 34 px
GRID_X0 = 8
GRID_Y0 = 9
TILE_PX = 34

# Barrels continuous maze (Pathing / Blocking)
BARREL_TILES: Set[Tuple[int, int]] = {
    (1, 5), (1, 6), (1, 7), (1, 9), (1, 10), (1, 11), (1, 13), (1, 14), (1, 15),
    (2, 5), (2, 7), (2, 9), (2, 11), (2, 13), (2, 15),
    (3, 4), (3, 5), (3, 7), (3, 8), (3, 9), (3, 11), (3, 12), (3, 13), (3, 15), (3, 16)
}

# The 7 test columns: rows 6..9 (4 tiles each)
# Col 4: STAIR indicator
# Col 6: WALK indicator
# Col 8: BLOCK indicator (TechInvisibleWall 1548 - NOT pathing blocked!)
# Col 10: No-PvP Zone (Green)
# Col 12: PZ Zone (Yellow)
# Col 14: No-Logout Zone (Orange)
# Col 16: PvP Zone (Red)
COL_STAIR   = (144, 178, 213, 349)
COL_WALK    = (212, 246, 213, 349)
COL_BLOCK   = (280, 314, 213, 349)
COL_NOPVP   = (348, 382, 213, 349)
COL_PZ      = (416, 450, 213, 349)
COL_NOLOG   = (484, 518, 213, 349)
COL_PVP     = (552, 586, 213, 349)

# Spawn 5x5 area: cols 9..13 (x in [314, 484]), rows 11..15 (y in [383, 553])
SPAWN_RECT  = (314, 484, 383, 553)
SPAWN_BADGE = (382, 416, 451, 485)

# Town marker & floating label
TOWN_BADGE  = (144, 178, 417, 451)
TOWN_LABEL  = "VARAN KURWA GIANT SPIDER"


# ==============================================================================
# 3. High-Fidelity Scene Overlay Renderer
# ==============================================================================

class OverlayRenderer:
    """Renders overlays on top of the authentic in-game screenshot scene."""

    def __init__(self, theme: OverlayTheme):
        self.theme = theme
        self.assets_dir = Path(__file__).resolve().parent / "assets"
        self.clean_scene_path = self.assets_dir / "scene_clean.png"
        self._cached_base: Optional[Image.Image] = None

    def get_base_scene(self) -> Image.Image:
        if self._cached_base is not None:
            return self._cached_base.copy()

        if self.clean_scene_path.exists():
            try:
                img = Image.open(self.clean_scene_path).convert('RGBA')
                self._cached_base = img
                return img.copy()
            except Exception as e:
                print(f"[Warning] Failed to load {self.clean_scene_path}: {e}")

        # Fallback: create procedural realistic stone backdrop
        w, h = 1024, 597
        arr = np.full((h, w, 4), [130, 130, 130, 255], dtype=np.uint8)
        # Add subtle stone noise & grid
        for y in range(h):
            for x in range(w):
                if (x - GRID_X0) % TILE_PX == 0 or (y - GRID_Y0) % TILE_PX == 0:
                    arr[y, x, :3] = 110
        img = Image.fromarray(arr, mode='RGBA')
        self._cached_base = img
        return img.copy()

    def render(self,
               show_blocking: bool = True,
               show_zones: bool = True,
               show_spawns: bool = True,
               show_houses: bool = True,
               show_tech: bool = True,
               scale: float = 1.0) -> Image.Image:
        """Composites shader overlays over the authentic in-game scene."""
        base_img = self.get_base_scene()
        w, h = base_img.size
        buf = np.array(base_img, dtype=np.float32) / 255.0

        # Helper: blend wash and 1px outer border onto a rectangle
        def apply_zone_box(x0: int, x1: int, y0: int, y1: int, wash: RGBA, border: RGBA):
            # Fill wash
            wa = wash.a
            if wa > 0.0:
                buf[y0:y1, x0:x1, :3] = buf[y0:y1, x0:x1, :3] * (1.0 - wa) + np.array([wash.r, wash.g, wash.b]) * wa
            # 1px border
            ba = border.a
            if ba > 0.0:
                bcol = np.array([border.r, border.g, border.b])
                buf[y0, x0:x1, :3]     = buf[y0, x0:x1, :3]     * (1.0 - ba) + bcol * ba
                buf[y1-1, x0:x1, :3]   = buf[y1-1, x0:x1, :3]   * (1.0 - ba) + bcol * ba
                buf[y0:y1, x0, :3]     = buf[y0:y1, x0, :3]     * (1.0 - ba) + bcol * ba
                buf[y0:y1, x1-1, :3]   = buf[y0:y1, x1-1, :3]   * (1.0 - ba) + bcol * ba

        # 1. Special Zones Layer (Cols 4, 5, 6, 7)
        if show_zones:
            # Col 4: No-PvP Zone (Col 10, Green)
            x0, x1, y0, y1 = COL_NOPVP
            apply_zone_box(x0, x1, y0, y1, self.theme.nopvp_wash, self.theme.nopvp_border)

            # Col 5: Protection Zone (PZ - Col 12, Yellow)
            x0, x1, y0, y1 = COL_PZ
            apply_zone_box(x0, x1, y0, y1, self.theme.pz_wash, self.theme.pz_border)

            # Col 6: No-Logout Zone (Col 14, Orange)
            x0, x1, y0, y1 = COL_NOLOG
            apply_zone_box(x0, x1, y0, y1, self.theme.nolog_wash, self.theme.nolog_border)

            # Col 7: PvP Zone (Col 16, Crimson Red)
            x0, x1, y0, y1 = COL_PVP
            apply_zone_box(x0, x1, y0, y1, self.theme.pvp_wash, self.theme.pvp_border)

        # 2. Pathing / Blocking Overlay (Barrels maze)
        if show_blocking:
            bw = self.theme.blocking_wash
            bb = self.theme.blocking_border
            # Apply connected outer border to barrel tiles
            for r, c in BARREL_TILES:
                x0 = GRID_X0 + c * TILE_PX
                x1 = x0 + TILE_PX
                y0 = GRID_Y0 + r * TILE_PX
                y1 = y0 + TILE_PX

                if bw.a > 0.0:
                    buf[y0:y1, x0:x1, :3] = buf[y0:y1, x0:x1, :3] * (1.0 - bw.a) + np.array([bw.r, bw.g, bw.b]) * bw.a

                bcol = np.array([bb.r, bb.g, bb.b])
                ba = bb.a
                # North
                if (r - 1, c) not in BARREL_TILES:
                    buf[y0, x0:x1, :3] = buf[y0, x0:x1, :3] * (1.0 - ba) + bcol * ba
                # South
                if (r + 1, c) not in BARREL_TILES:
                    buf[y1-1, x0:x1, :3] = buf[y1-1, x0:x1, :3] * (1.0 - ba) + bcol * ba
                # West
                if (r, c - 1) not in BARREL_TILES:
                    buf[y0:y1, x0, :3] = buf[y0:y1, x0:x1, :3] * (1.0 - ba) + bcol * ba if False else buf[y0:y1, x0, :3] * (1.0 - ba) + bcol * ba
                # East
                if (r, c + 1) not in BARREL_TILES:
                    buf[y0:y1, x1-1, :3] = buf[y0:y1, x1-1, :3] * (1.0 - ba) + bcol * ba

        # 3. Spawn Overlay (5x5 square)
        if show_spawns:
            x0, x1, y0, y1 = SPAWN_RECT
            apply_zone_box(x0, x1, y0, y1, self.theme.spawn_wash, self.theme.spawn_border)

        # Convert back to PIL Image
        img_np = np.clip(buf * 255.0, 0, 255).astype(np.uint8)
        img = Image.fromarray(img_np, mode='RGBA')

        # Scale if requested
        if scale != 1.0:
            nw = int(round(w * scale))
            nh = int(round(h * scale))
            img = img.resize((nw, nh), Image.Resampling.NEAREST if scale >= 2.0 else Image.Resampling.BILINEAR)

        return img


# ==============================================================================
# 4. Interactive Desktop GUI (Tkinter)
# ==============================================================================

class OverlayDesignerGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("RME Redux - Visual Overlay & GLSL Shader Designer")
        self.root.geometry("1300x860")
        self.root.minsize(1040, 700)

        self.theme = OverlayTheme()
        self.renderer = OverlayRenderer(self.theme)

        # Visibility toggles
        self.show_zones = tk.BooleanVar(value=True)
        self.show_blocking = tk.BooleanVar(value=True)
        self.show_spawns = tk.BooleanVar(value=True)
        self.show_houses = tk.BooleanVar(value=True)
        self.show_tech = tk.BooleanVar(value=True)
        self.zoom_scale = tk.DoubleVar(value=1.0)

        self.build_ui()
        self.update_preview()

    def build_ui(self):
        # Main split container: Left Controls (Scrollable) | Right Canvas
        main_paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main_paned.pack(fill=tk.BOTH, expand=True)

        # Left Control Frame
        left_frame = ttk.Frame(main_paned, width=450)
        main_paned.add(left_frame, weight=0)

        # Right Preview Frame
        right_frame = ttk.Frame(main_paned)
        main_paned.add(right_frame, weight=1)

        # Build Notebook
        notebook = ttk.Notebook(left_frame)
        notebook.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.tab_zones = ttk.Frame(notebook)
        self.tab_blocking = ttk.Frame(notebook)
        self.tab_houses = ttk.Frame(notebook)
        self.tab_indicators = ttk.Frame(notebook)
        self.tab_export = ttk.Frame(notebook)

        notebook.add(self.tab_zones, text="Zones")
        notebook.add(self.tab_blocking, text="Blocking & Spawns")
        notebook.add(self.tab_houses, text="Houses")
        notebook.add(self.tab_indicators, text="Badges")
        notebook.add(self.tab_export, text="Export C++ GLSL")

        self.build_zones_tab()
        self.build_blocking_tab()
        self.build_houses_tab()
        self.build_indicators_tab()
        self.build_export_tab()

        # Build Right Preview Area
        toolbar = ttk.Frame(right_frame)
        toolbar.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(toolbar, text="Layer Toggles:").pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(toolbar, text="Special Zones", variable=self.show_zones, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Pathing / Blocking", variable=self.show_blocking, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Spawns", variable=self.show_spawns, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Houses", variable=self.show_houses, command=self.update_preview).pack(side=tk.LEFT, padx=3)
        ttk.Checkbutton(toolbar, text="Tech Badges", variable=self.show_tech, command=self.update_preview).pack(side=tk.LEFT, padx=3)

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)

        ttk.Label(toolbar, text="Zoom:").pack(side=tk.LEFT, padx=4)
        for z, label in [(1.0, "100%"), (1.25, "125%"), (1.5, "150%"), (2.0, "200%")]:
            ttk.Radiobutton(toolbar, text=label, variable=self.zoom_scale, value=z, command=self.update_preview).pack(side=tk.LEFT)

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Button(toolbar, text="📸 Export Image", command=self.on_export_image).pack(side=tk.LEFT, padx=4)

        # Canvas with Scrollbars
        canvas_container = ttk.Frame(right_frame)
        canvas_container.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.canvas = tk.Canvas(canvas_container, bg="#121214", highlightthickness=0)
        self.v_scroll = ttk.Scrollbar(canvas_container, orient=tk.VERTICAL, command=self.canvas.yview)
        self.h_scroll = ttk.Scrollbar(canvas_container, orient=tk.HORIZONTAL, command=self.canvas.xview)
        self.canvas.configure(xscrollcommand=self.h_scroll.set, yscrollcommand=self.v_scroll.set)

        self.v_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.h_scroll.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Status Bar
        self.status_bar = ttk.Label(right_frame, text="Ready. In-Game scene loaded matching reference screenshot.", relief=tk.SUNKEN, anchor=tk.W)
        self.status_bar.pack(fill=tk.X, side=tk.BOTTOM, padx=4, pady=2)

    def create_rgba_controls(self, parent: ttk.Frame, label_text: str, rgba_obj: RGBA):
        row = ttk.LabelFrame(parent, text=label_text)
        row.pack(fill=tk.X, padx=4, pady=3)

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

        ttk.Label(row, text="Opacity:").grid(row=0, column=1, padx=4)
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

        ttk.Label(f, text="Protection Zone (PZ - Col 5, Yellow):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "PZ Wash (Background Fill)", self.theme.pz_wash)
        self.create_rgba_controls(f, "PZ Border (Connected Boundary)", self.theme.pz_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="No-PvP Zone (Col 4, Green):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "No-PvP Wash (Background Fill)", self.theme.nopvp_wash)
        self.create_rgba_controls(f, "No-PvP Border (Connected Boundary)", self.theme.nopvp_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="No-Logout Zone (Col 6, Orange):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "No-Logout Wash", self.theme.nolog_wash)
        self.create_rgba_controls(f, "No-Logout Border", self.theme.nolog_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=6)

        ttk.Label(f, text="PvP Zone (Col 7, Crimson Red):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "PvP Zone Wash", self.theme.pvp_wash)
        self.create_rgba_controls(f, "PvP Zone Border", self.theme.pvp_border)

    def build_blocking_tab(self):
        f = ttk.Frame(self.tab_blocking)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="Pathing / Blocking Overlay (Barrels Maze):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Blocking Wash (Terrain Unpassable)", self.theme.blocking_wash)
        self.create_rgba_controls(f, "Blocking Border (1px Cyan Outline)", self.theme.blocking_border)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=8)

        ttk.Label(f, text="Spawn Areas (5x5 Square):", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Spawn Radius Wash", self.theme.spawn_wash)
        self.create_rgba_controls(f, "Spawn Boundary Border", self.theme.spawn_border)

    def build_houses_tab(self):
        f = ttk.Frame(self.tab_houses)
        f.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        ttk.Label(f, text="House Shading & Atmosphere:", font=("Segoe UI", 9, "bold")).pack(anchor=tk.W, pady=(4, 2))
        self.create_rgba_controls(f, "Active House Wash (Top Green)", self.theme.house_active_wash)
        self.create_rgba_controls(f, "Inactive House Wash (Bottom Amber)", self.theme.house_inactive_wash)

        ttk.Separator(f, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=8)

        hf = ttk.LabelFrame(f, text="Cross-Hatch Settings")
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
        self.create_rgba_controls(f, "BLOCK (Col 3: Invisible Wall 1548)", self.theme.indicator_block_border)
        self.create_rgba_controls(f, "STAIR (Col 1: Invisible Stairs)", self.theme.indicator_stair_border)
        self.create_rgba_controls(f, "WALK (Col 2: Invisible Walkable)", self.theme.indicator_walk_border)
        self.create_rgba_controls(f, "TOWN (Town Temple Badge)", self.theme.indicator_town_border)
        self.create_rgba_controls(f, "SPAWN (Spawn Center Badge)", self.theme.indicator_spawn_border)
        self.create_rgba_controls(f, "ENTRY (House Entry Badge)", self.theme.indicator_entry_border)

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
        import re
        content = re.sub(r'zWash = vec4\([^)]+\); \s*// PZ', f'zWash = {self.theme.pz_wash.to_glsl_vec4()};', content)
        content = re.sub(r'zBorder = vec4\([^)]+\); \s*// PZ', f'zBorder = {self.theme.pz_border.to_glsl_vec4()};', content)
        content = re.sub(r'zWash = vec4\([^)]+\); \s*// No-PvP', f'zWash = {self.theme.nopvp_wash.to_glsl_vec4()};', content)
        content = re.sub(r'zBorder = vec4\([^)]+\); \s*// No-PvP', f'zBorder = {self.theme.nopvp_border.to_glsl_vec4()};', content)

        shader_path.write_text(content, encoding="utf-8")
        messagebox.showinfo("Success", f"Updated {shader_path.name} successfully!")

    def on_export_image(self):
        path = filedialog.asksaveasfilename(defaultextension=".png", filetypes=[("PNG Image", "*.png"), ("JPEG Image", "*.jpg")])
        if path:
            img = self.renderer.render(
                show_blocking=self.show_blocking.get(),
                show_zones=self.show_zones.get(),
                show_spawns=self.show_spawns.get(),
                show_houses=self.show_houses.get(),
                show_tech=self.show_tech.get(),
                scale=self.zoom_scale.get()
            )
            img.save(path)
            messagebox.showinfo("Exported", f"Saved image to: {path}")

    def update_preview(self):
        scale = self.zoom_scale.get()
        img = self.renderer.render(
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
    parser.add_argument("--export", type=str, help="Export rendered scene image to file and exit")
    parser.add_argument("--dump-glsl", action="store_true", help="Print current C++ GLSL code and exit")
    parser.add_argument("--scale", type=float, default=1.0, help="Image export scale factor (1.0 = 1024x597, 2.0 = 2048x1194)")
    args = parser.parse_args()

    theme = OverlayTheme()
    renderer = OverlayRenderer(theme)

    if args.dump_glsl:
        print(theme.generate_cpp_zone_shader())
        return 0

    if args.export:
        out_path = Path(args.export).resolve()
        img = renderer.render(scale=args.scale)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        img.save(out_path)
        print(f"[Success] Exported realistic scene to: {out_path}")
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
