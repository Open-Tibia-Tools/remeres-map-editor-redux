#!/usr/bin/env python3
"""
GUI for converting legacy Remere's Map Editor client data folders
into the modern modular XML layout. Cross-platform (Windows & Linux).
"""

from __future__ import annotations

import datetime
import os
import subprocess
import sys
import threading
from collections import Counter
from pathlib import Path
from typing import Any

# Ensure the directory containing convert_legacy_data.py is on sys.path
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))


def ensure_wx() -> bool:
    """Check if wxPython is installed. If missing, prompt the user and offer automatic install."""
    try:
        import wx  # noqa: F401
        return True
    except ImportError:
        pass

    install_cmd = f"{sys.executable} -m pip install wxPython"
    msg = (
        "wxPython is required to run the graphical user interface.\n\n"
        f"To install it manually, run in your terminal:\n"
        f"  {install_cmd}\n\n"
        "Or on Linux (Debian/Ubuntu):\n"
        "  sudo apt install python3-wxgtk4.0\n\n"
        "Would you like to attempt automatic installation via pip now?"
    )

    print("\n" + "=" * 60, file=sys.stderr)
    print("wxPython is not installed.", file=sys.stderr)
    print(f"Command to install: {install_cmd}", file=sys.stderr)
    print("On Debian/Ubuntu: sudo apt install python3-wxgtk4.0", file=sys.stderr)
    print("=" * 60 + "\n", file=sys.stderr)

    should_install = False
    if sys.platform.startswith("win"):
        try:
            import ctypes
            res = ctypes.windll.user32.MessageBoxW(
                0,
                msg,
                "RME Legacy Converter - Missing Dependency",
                0x24,  # MB_YESNO | MB_ICONQUESTION
            )
            should_install = (res == 6)
        except Exception:
            pass
    elif os.environ.get("DISPLAY") or sys.platform == "darwin":
        try:
            import tkinter
            from tkinter import messagebox
            root = tkinter.Tk()
            root.withdraw()
            should_install = messagebox.askyesno(
                "RME Legacy Converter - Missing Dependency",
                msg,
            )
            root.destroy()
        except Exception:
            pass

    if should_install:
        print("Installing wxPython via pip... Please wait.", file=sys.stderr)
        try:
            ret = subprocess.call([sys.executable, "-m", "pip", "install", "wxPython"])
            if ret == 0:
                print("wxPython installed successfully! Starting GUI...", file=sys.stderr)
                return True
            else:
                print("Failed to install wxPython via pip. Please install manually.", file=sys.stderr)
        except Exception as exc:
            print(f"Failed to run pip: {exc}", file=sys.stderr)

    return False


if not ensure_wx():
    sys.exit(1)

import wx

from convert_legacy_data import (
    LegacyVersionReader,
    ModularWriter,
    Normalizer,
    inspect_legacy_details,
    inspect_source_path,
    is_valid_folder_name,
    preview_normalized,
    run_conversion,
    summarize_issues,
)


def format_size(num_bytes: int) -> str:
    """Format bytes into a human-readable size string."""
    for unit in ("B", "KB", "MB", "GB"):
        if num_bytes < 1024.0:
            return f"{num_bytes:.1f} {unit}" if unit != "B" else f"{num_bytes} B"
        num_bytes /= 1024.0
    return f"{num_bytes:.1f} TB"


def open_folder_in_explorer(path: Path) -> None:
    """Open a directory in the native file browser (Windows, Linux, macOS)."""
    if not path.exists():
        return
    try:
        if sys.platform.startswith("win"):
            os.startfile(str(path))
        elif sys.platform.startswith("darwin"):
            subprocess.Popen(["open", str(path)])
        else:
            # Linux / BSD
            for cmd in (["xdg-open", str(path)], ["gio", "open", str(path)]):
                try:
                    subprocess.Popen(cmd)
                    return
                except FileNotFoundError:
                    continue
    except Exception as exc:
        print(f"Unable to open file browser: {exc}", file=sys.stderr)


def get_monospace_font(point_size: int = 9) -> wx.Font:
    """Return a portable monospace font across Windows, Linux (GTK), and macOS."""
    return wx.Font(wx.FontInfo(point_size).Family(wx.FONTFAMILY_TELETYPE))


class ConverterFrame(wx.Frame):
    """Main window for RME Legacy Data XML Converter."""

    def __init__(self, initial_base_dir: Path | None = None, initial_output_dir: Path | None = None) -> None:
        super().__init__(
            None,
            title="RME Legacy Data XML Converter",
            size=(1120, 820),
            style=wx.DEFAULT_FRAME_STYLE | wx.TAB_TRAVERSAL,
        )

        self.SetMinSize((880, 640))
        self.Center()

        # State: paths are NOT auto-guessed; user chooses paths
        self.source_path: Path | None = initial_base_dir.resolve() if initial_base_dir else None
        self.output_root: Path | None = initial_output_dir.resolve() if initial_output_dir else None

        self.is_single_version = False
        self.discovered_versions: list[str] = []
        self.version_paths: dict[str, Path] = {}
        self.selected_version_for_preview: str | None = None
        self.last_converted_path: Path | None = None
        self.is_converting = False

        self._init_ui()

        # Only scan if initial_base_dir was explicitly passed (e.g. from CLI)
        if self.source_path and self.source_path.exists():
            self._scan_source_path()
        else:
            self._clear_views()
            self.lbl_versions_status.SetLabel("Please select a legacy client data folder or base directory.")
            self.lbl_versions_status.SetForegroundColour(wx.Colour(120, 120, 120))

    def _init_ui(self) -> None:
        panel = wx.Panel(self)
        main_sizer = wx.BoxSizer(wx.VERTICAL)

        # 1. Top Settings & Paths Box
        settings_box = wx.StaticBox(panel, label=" Paths & Configuration ")
        settings_sizer = wx.StaticBoxSizer(settings_box, wx.VERTICAL)
        grid_sizer = wx.FlexGridSizer(rows=4, cols=3, vgap=6, hgap=8)
        grid_sizer.AddGrowableCol(1, 1)

        # Source Path
        grid_sizer.Add(wx.StaticText(settings_box, label="Source Path:"), 0, wx.ALIGN_CENTER_VERTICAL)
        self.txt_source = wx.TextCtrl(
            settings_box,
            value=str(self.source_path) if self.source_path else "",
            style=wx.TE_PROCESS_ENTER,
        )
        self.txt_source.Bind(wx.EVT_KILL_FOCUS, self._on_source_text_changed)
        self.txt_source.Bind(wx.EVT_TEXT_ENTER, self._on_source_text_changed)
        grid_sizer.Add(self.txt_source, 1, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)

        source_btn_sizer = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_browse_source = wx.Button(settings_box, label="Browse...")
        self.btn_browse_source.Bind(wx.EVT_BUTTON, self._on_browse_source)
        source_btn_sizer.Add(self.btn_browse_source, 0, wx.RIGHT, 4)
        self.btn_scan = wx.Button(settings_box, label="Scan")
        self.btn_scan.Bind(wx.EVT_BUTTON, lambda e: self._scan_source_path())
        source_btn_sizer.Add(self.btn_scan, 0)
        grid_sizer.Add(source_btn_sizer, 0, wx.ALIGN_CENTER_VERTICAL)

        # Output Target Folder
        grid_sizer.Add(wx.StaticText(settings_box, label="Target Output Dir:"), 0, wx.ALIGN_CENTER_VERTICAL)
        self.txt_output = wx.TextCtrl(
            settings_box,
            value=str(self.output_root) if self.output_root else "",
            style=wx.TE_PROCESS_ENTER,
        )
        self.txt_output.Bind(wx.EVT_KILL_FOCUS, lambda e: (self._refresh_preview(), e.Skip()))
        self.txt_output.Bind(wx.EVT_TEXT_ENTER, lambda e: (self._refresh_preview(), e.Skip()))
        grid_sizer.Add(self.txt_output, 1, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)
        self.btn_browse_output = wx.Button(settings_box, label="Browse...")
        self.btn_browse_output.Bind(wx.EVT_BUTTON, self._on_browse_output)
        grid_sizer.Add(self.btn_browse_output, 0, wx.ALIGN_CENTER_VERTICAL)

        # Output Folder Name (subfolder)
        grid_sizer.Add(wx.StaticText(settings_box, label="Output Folder Name:"), 0, wx.ALIGN_CENTER_VERTICAL)
        self.txt_folder_name = wx.TextCtrl(settings_box, value="", style=wx.TE_PROCESS_ENTER)
        self.txt_folder_name.Bind(wx.EVT_KILL_FOCUS, lambda e: (self._refresh_preview(), e.Skip()))
        self.txt_folder_name.Bind(wx.EVT_TEXT_ENTER, lambda e: (self._refresh_preview(), e.Skip()))
        grid_sizer.Add(self.txt_folder_name, 1, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)
        self.lbl_folder_hint = wx.StaticText(settings_box, label="(Subfolder created in target output directory)")
        self.lbl_folder_hint.SetForegroundColour(wx.Colour(120, 120, 120))
        grid_sizer.Add(self.lbl_folder_hint, 0, wx.ALIGN_CENTER_VERTICAL)

        # Discovered Versions Selection (for multi-version folders)
        grid_sizer.Add(wx.StaticText(settings_box, label="Client Versions:"), 0, wx.ALIGN_CENTER_VERTICAL)
        version_row = wx.BoxSizer(wx.HORIZONTAL)
        self.lbl_versions_status = wx.StaticText(settings_box, label="No source path selected")
        version_row.Add(self.lbl_versions_status, 1, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 8)
        self.btn_select_all = wx.Button(settings_box, label="Select All", size=(-1, 24))
        self.btn_select_all.Bind(wx.EVT_BUTTON, self._on_select_all_versions)
        version_row.Add(self.btn_select_all, 0, wx.RIGHT, 4)
        self.btn_select_none = wx.Button(settings_box, label="Deselect All", size=(-1, 24))
        self.btn_select_none.Bind(wx.EVT_BUTTON, self._on_select_none_versions)
        version_row.Add(self.btn_select_none, 0)
        grid_sizer.Add(version_row, 1, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)

        # Inspect Version Picker
        inspect_picker_sizer = wx.BoxSizer(wx.HORIZONTAL)
        inspect_picker_sizer.Add(wx.StaticText(settings_box, label="Preview:"), 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 4)
        self.choice_inspect_version = wx.Choice(settings_box)
        self.choice_inspect_version.Bind(wx.EVT_CHOICE, self._on_inspect_version_changed)
        inspect_picker_sizer.Add(self.choice_inspect_version, 1, wx.EXPAND)
        grid_sizer.Add(inspect_picker_sizer, 0, wx.EXPAND | wx.ALIGN_CENTER_VERTICAL)

        settings_sizer.Add(grid_sizer, 0, wx.EXPAND | wx.ALL, 4)

        # Version CheckListBox (collapsible / visible when multiple versions)
        self.version_check_sizer = wx.BoxSizer(wx.VERTICAL)
        self.check_list_versions = wx.CheckListBox(settings_box, size=(-1, 75))
        self.check_list_versions.Bind(wx.EVT_CHECKLISTBOX, self._on_version_checked)
        self.version_check_sizer.Add(self.check_list_versions, 0, wx.EXPAND | wx.TOP, 2)
        settings_sizer.Add(self.version_check_sizer, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 4)

        main_sizer.Add(settings_sizer, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.TOP, 8)

        # 2. Main Notebook (Before & After View, Conversion Log, Diagnostics)
        self.notebook = wx.Notebook(panel)

        # Tab 1: Before & After Splitter
        self.tab_comparison = self._create_comparison_tab(self.notebook)
        self.notebook.AddPage(self.tab_comparison, "Before & After View")

        # Tab 2: Conversion Log
        self.tab_log = self._create_log_tab(self.notebook)
        self.notebook.AddPage(self.tab_log, "Conversion Log")

        # Tab 3: Diagnostics & Unresolved References
        self.tab_diagnostics = self._create_diagnostics_tab(self.notebook)
        self.notebook.AddPage(self.tab_diagnostics, "Diagnostics & Unresolved References")

        main_sizer.Add(self.notebook, 1, wx.EXPAND | wx.ALL, 8)

        # 3. Bottom Action Bar & Progress
        bottom_sizer = wx.BoxSizer(wx.VERTICAL)

        # Gauge & Status Text
        progress_row = wx.BoxSizer(wx.HORIZONTAL)
        self.gauge = wx.Gauge(panel, range=100, size=(-1, 18))
        progress_row.Add(self.gauge, 1, wx.EXPAND | wx.RIGHT, 10)
        self.lbl_progress_status = wx.StaticText(panel, label="Ready")
        progress_row.Add(self.lbl_progress_status, 0, wx.ALIGN_CENTER_VERTICAL)
        bottom_sizer.Add(progress_row, 0, wx.EXPAND | wx.BOTTOM, 6)

        # Buttons Row
        btn_row = wx.BoxSizer(wx.HORIZONTAL)

        self.btn_preview = wx.Button(panel, label="Refresh Preview", size=(120, 30))
        self.btn_preview.Bind(wx.EVT_BUTTON, lambda e: self._refresh_preview())
        btn_row.Add(self.btn_preview, 0, wx.RIGHT, 8)

        btn_row.AddStretchSpacer(1)

        self.btn_open_output = wx.Button(panel, label="Open Output Folder", size=(140, 30))
        self.btn_open_output.Bind(wx.EVT_BUTTON, self._on_open_output_folder)
        self.btn_open_output.Enable(False)
        btn_row.Add(self.btn_open_output, 0, wx.RIGHT, 8)

        self.btn_convert = wx.Button(panel, label="Start Conversion", size=(150, 30))
        self.btn_convert.SetFont(self.btn_convert.GetFont().Bold())
        self.btn_convert.Bind(wx.EVT_BUTTON, self._on_start_conversion)
        btn_row.Add(self.btn_convert, 0, wx.RIGHT, 8)

        self.btn_exit = wx.Button(panel, label="Exit", size=(90, 30))
        self.btn_exit.Bind(wx.EVT_BUTTON, lambda e: self.Close())
        btn_row.Add(self.btn_exit, 0)

        bottom_sizer.Add(btn_row, 0, wx.EXPAND)
        main_sizer.Add(bottom_sizer, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 8)

        panel.SetSizer(main_sizer)

    def _create_comparison_tab(self, parent: wx.Window) -> wx.Window:
        """Create the split 'Before' and 'After' view."""
        splitter = wx.SplitterWindow(parent, style=wx.SP_3D | wx.SP_LIVE_UPDATE)
        splitter.SetMinimumPaneSize(320)

        # Left: BEFORE (Legacy)
        left_panel = wx.Panel(splitter)
        left_sizer = wx.BoxSizer(wx.VERTICAL)

        left_header = wx.StaticText(left_panel, label="BEFORE: Legacy Source Data")
        left_header.SetFont(left_header.GetFont().Bold())
        left_header.SetForegroundColour(wx.Colour(180, 70, 0))
        left_sizer.Add(left_header, 0, wx.ALL, 6)

        # Source Files List
        self.list_before_files = wx.ListCtrl(left_panel, style=wx.LC_REPORT | wx.LC_SINGLE_SEL | wx.BORDER_SUNKEN)
        self.list_before_files.InsertColumn(0, "Source File", width=180)
        self.list_before_files.InsertColumn(1, "Size", width=90)
        self.list_before_files.InsertColumn(2, "Role / Note", width=160)
        left_sizer.Add(self.list_before_files, 1, wx.EXPAND | wx.LEFT | wx.RIGHT, 6)

        # Source Summary Stats
        stats_box_before = wx.StaticBox(left_panel, label=" Source Statistics ")
        stats_sizer_before = wx.StaticBoxSizer(stats_box_before, wx.VERTICAL)
        self.txt_before_stats = wx.TextCtrl(
            stats_box_before,
            style=wx.TE_MULTILINE | wx.TE_READONLY | wx.BORDER_NONE,
            size=(-1, 140),
        )
        self.txt_before_stats.SetBackgroundColour(left_panel.GetBackgroundColour())
        stats_sizer_before.Add(self.txt_before_stats, 1, wx.EXPAND | wx.ALL, 4)
        left_sizer.Add(stats_sizer_before, 0, wx.EXPAND | wx.ALL, 6)

        left_panel.SetSizer(left_sizer)

        # Right: AFTER (Modular)
        right_panel = wx.Panel(splitter)
        right_sizer = wx.BoxSizer(wx.VERTICAL)

        right_header = wx.StaticText(right_panel, label="AFTER: Modular Target Structure")
        right_header.SetFont(right_header.GetFont().Bold())
        right_header.SetForegroundColour(wx.Colour(0, 130, 40))
        right_sizer.Add(right_header, 0, wx.ALL, 6)

        # Target Modular Files Tree
        self.tree_after = wx.TreeCtrl(right_panel, style=wx.TR_DEFAULT_STYLE | wx.BORDER_SUNKEN)
        right_sizer.Add(self.tree_after, 1, wx.EXPAND | wx.LEFT | wx.RIGHT, 6)

        # Target Summary Stats
        stats_box_after = wx.StaticBox(right_panel, label=" Target Statistics & Validation ")
        stats_sizer_after = wx.StaticBoxSizer(stats_box_after, wx.VERTICAL)
        self.txt_after_stats = wx.TextCtrl(
            stats_box_after,
            style=wx.TE_MULTILINE | wx.TE_READONLY | wx.BORDER_NONE,
            size=(-1, 140),
        )
        self.txt_after_stats.SetBackgroundColour(right_panel.GetBackgroundColour())
        stats_sizer_after.Add(self.txt_after_stats, 1, wx.EXPAND | wx.ALL, 4)
        right_sizer.Add(stats_sizer_after, 0, wx.EXPAND | wx.ALL, 6)

        right_panel.SetSizer(right_sizer)

        splitter.SplitVertically(left_panel, right_panel, 540)
        return splitter

    def _create_log_tab(self, parent: wx.Window) -> wx.Window:
        """Create the real-time conversion log view."""
        panel = wx.Panel(parent)
        sizer = wx.BoxSizer(wx.VERTICAL)

        # Toolbar
        toolbar_sizer = wx.BoxSizer(wx.HORIZONTAL)
        lbl = wx.StaticText(panel, label="Conversion Log Output:")
        lbl.SetFont(lbl.GetFont().Bold())
        toolbar_sizer.Add(lbl, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 10)

        toolbar_sizer.AddStretchSpacer(1)

        btn_copy = wx.Button(panel, label="Copy All", size=(90, 24))
        btn_copy.Bind(wx.EVT_BUTTON, self._on_copy_log)
        toolbar_sizer.Add(btn_copy, 0, wx.RIGHT, 4)

        btn_save = wx.Button(panel, label="Save Log...", size=(100, 24))
        btn_save.Bind(wx.EVT_BUTTON, self._on_save_log)
        toolbar_sizer.Add(btn_save, 0, wx.RIGHT, 4)

        btn_clear = wx.Button(panel, label="Clear", size=(80, 24))
        btn_clear.Bind(wx.EVT_BUTTON, lambda e: self.txt_log.Clear())
        toolbar_sizer.Add(btn_clear, 0)

        sizer.Add(toolbar_sizer, 0, wx.EXPAND | wx.ALL, 6)

        # Cross-platform Monospace Text Box
        self.txt_log = wx.TextCtrl(
            panel,
            style=wx.TE_MULTILINE | wx.TE_READONLY | wx.HSCROLL | wx.BORDER_SUNKEN,
        )
        self.txt_log.SetFont(get_monospace_font(9))
        sizer.Add(self.txt_log, 1, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 6)

        panel.SetSizer(sizer)
        return panel

    def _create_diagnostics_tab(self, parent: wx.Window) -> wx.Window:
        """Create the tab displaying unresolved references and XML sanitization issues."""
        panel = wx.Panel(parent)
        sizer = wx.BoxSizer(wx.VERTICAL)

        lbl = wx.StaticText(panel, label="Legacy Diagnostics, Sanitization Repaired & Unresolved References:")
        lbl.SetFont(lbl.GetFont().Bold())
        sizer.Add(lbl, 0, wx.ALL, 6)

        self.txt_diagnostics = wx.TextCtrl(
            panel,
            style=wx.TE_MULTILINE | wx.TE_READONLY | wx.HSCROLL | wx.BORDER_SUNKEN,
        )
        self.txt_diagnostics.SetFont(get_monospace_font(9))
        sizer.Add(self.txt_diagnostics, 1, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 6)

        panel.SetSizer(sizer)
        return panel

    # --- Scanning & Preview Logic ---

    def _on_source_text_changed(self, event: wx.Event) -> None:
        val = self.txt_source.GetValue().strip()
        if val and Path(val).exists():
            self.source_path = Path(val)
            self._scan_source_path()
        event.Skip()

    def _on_browse_source(self, event: wx.Event) -> None:
        default_dir = str(self.source_path) if self.source_path else os.getcwd()
        dlg = wx.DirDialog(self, "Select Legacy Client Folder or Base Data Folder", default_dir)
        if dlg.ShowModal() == wx.ID_OK:
            self.source_path = Path(dlg.GetPath())
            self.txt_source.SetValue(str(self.source_path))
            self._scan_source_path()
        dlg.Destroy()

    def _on_browse_output(self, event: wx.Event) -> None:
        default_dir = str(self.output_root) if self.output_root else (str(self.source_path.parent) if self.source_path else os.getcwd())
        dlg = wx.DirDialog(self, "Select Target Output Directory", default_dir)
        if dlg.ShowModal() == wx.ID_OK:
            self.output_root = Path(dlg.GetPath())
            self.txt_output.SetValue(str(self.output_root))
            self._refresh_preview()
        dlg.Destroy()

    def _scan_source_path(self) -> None:
        """Inspect the source path to detect single vs multi-version client folders."""
        raw_val = self.txt_source.GetValue().strip()
        if not raw_val:
            self.lbl_versions_status.SetLabel("Please select a legacy client data folder or base directory.")
            self.lbl_versions_status.SetForegroundColour(wx.Colour(120, 120, 120))
            self._clear_views()
            return

        path = Path(raw_val)
        self.source_path = path
        info = inspect_source_path(path)

        if not info["is_valid"]:
            self.lbl_versions_status.SetLabel(f"Error: {info['error']}")
            self.lbl_versions_status.SetForegroundColour(wx.Colour(200, 0, 0))
            self.check_list_versions.Clear()
            self.choice_inspect_version.Clear()
            self._clear_views()
            return

        self.is_single_version = bool(info["is_single_version"])
        self.discovered_versions = list(info["versions"])  # type: ignore[arg-type]
        self.version_paths = info["version_paths"]  # type: ignore[assignment]

        self.check_list_versions.Clear()
        self.choice_inspect_version.Clear()

        for v in self.discovered_versions:
            self.check_list_versions.Append(v)
            self.choice_inspect_version.Append(v)

        if self.is_single_version:
            # Direct client folder (contains materials.xml)
            ver_name = self.discovered_versions[0]
            self.lbl_versions_status.SetLabel(f"Single client version folder: {ver_name}")
            self.lbl_versions_status.SetForegroundColour(wx.Colour(0, 130, 0))
            self.btn_select_all.Disable()
            self.btn_select_none.Disable()
            self.check_list_versions.Check(0, True)
            self.choice_inspect_version.SetSelection(0)
            self.txt_folder_name.SetValue(ver_name)
            self.txt_folder_name.Enable(True)
            self.lbl_folder_hint.SetLabel("(Subfolder name in output directory)")
        else:
            # Base directory containing version subfolders
            self.lbl_versions_status.SetLabel(f"Found {len(self.discovered_versions)} version folder(s)")
            self.lbl_versions_status.SetForegroundColour(wx.Colour(0, 0, 0))
            self.btn_select_all.Enable(True)
            self.btn_select_none.Enable(True)
            for i in range(len(self.discovered_versions)):
                self.check_list_versions.Check(i, True)
            if self.discovered_versions:
                self.choice_inspect_version.SetSelection(0)
            self._update_folder_name_state()

        self._refresh_preview()

    def _on_select_all_versions(self, event: wx.Event) -> None:
        for i in range(self.check_list_versions.GetCount()):
            self.check_list_versions.Check(i, True)
        self._update_folder_name_state()

    def _on_select_none_versions(self, event: wx.Event) -> None:
        for i in range(self.check_list_versions.GetCount()):
            self.check_list_versions.Check(i, False)
        self._update_folder_name_state()

    def _on_version_checked(self, event: wx.Event) -> None:
        self._update_folder_name_state()

    def _update_folder_name_state(self) -> None:
        if self.is_single_version:
            self.txt_folder_name.Enable(True)
            self.lbl_folder_hint.SetLabel("(Subfolder name in output directory)")
            return

        checked = [self.check_list_versions.GetString(i) for i in range(self.check_list_versions.GetCount()) if self.check_list_versions.IsChecked(i)]
        if len(checked) == 1:
            self.txt_folder_name.SetValue(checked[0])
            self.txt_folder_name.Enable(True)
            self.lbl_folder_hint.SetLabel("(Subfolder name in output directory)")
        else:
            # Multiple versions selected: do NOT put placeholder text inside txt_folder_name
            self.txt_folder_name.SetValue("")
            self.txt_folder_name.Enable(False)
            self.lbl_folder_hint.SetLabel("(Batch mode: subfolders will be named after each version)")

    def _on_inspect_version_changed(self, event: wx.Event) -> None:
        self._refresh_preview()

    def _clear_views(self) -> None:
        self.list_before_files.DeleteAllItems()
        self.txt_before_stats.SetValue("Select a source folder above to inspect legacy client data.")
        self.tree_after.DeleteAllItems()
        self.txt_after_stats.SetValue("Select a source folder and output directory above to preview modular structure.")
        self.txt_diagnostics.SetValue("Select a source folder above to view diagnostics.")

    def _refresh_preview(self) -> None:
        """Inspect and preview the currently selected version."""
        sel_idx = self.choice_inspect_version.GetSelection()
        if sel_idx == wx.NOT_FOUND or not self.discovered_versions:
            self._clear_views()
            return

        version_name = self.choice_inspect_version.GetString(sel_idx)
        version_dir = self.version_paths.get(version_name)
        if not version_dir or not version_dir.exists():
            return

        self._load_before_view(version_dir)
        self._load_after_preview(version_dir)

    def _load_before_view(self, version_dir: Path) -> None:
        """Populate the 'Before' panel with legacy files and node stats."""
        try:
            details = inspect_legacy_details(version_dir)
        except Exception as exc:
            self.txt_before_stats.SetValue(f"Failed to inspect legacy data: {exc}")
            return

        # Populate file list
        self.list_before_files.DeleteAllItems()
        for f in details["files"]:  # type: ignore[union-attr]
            idx = self.list_before_files.InsertItem(self.list_before_files.GetItemCount(), f["name"])
            self.list_before_files.SetItem(idx, 1, format_size(f["size"]))

            # Describe role
            name = f["name"].lower()
            role = "Data file"
            if name == "materials.xml":
                role = "Main Manifest (includes)"
            elif name in ("items.xml", "items2.xml"):
                role = "Item Registry"
            elif name == "creatures.xml":
                role = "Creature Registry"
            elif name == "items.otb":
                role = "OTB Binary Items"
            elif name == "tilesets.xml":
                role = "Tileset Wrappers"
            elif "palette" in name:
                role = "Legacy Palette"
            elif name in ("grounds.xml", "walls.xml", "doodads.xml", "borders.xml"):
                role = "Brushes & Borders"
            self.list_before_files.SetItem(idx, 2, role)

        # Populate stats text
        issues_summary = "\n".join(
            f"  • {issue['path']}: {', '.join(issue['messages'])}"
            for issue in details["issues"]  # type: ignore[union-attr]
        ) or "  • None (clean XML parsing)"

        metaitems_str = ", ".join(str(m) for m in details["metaitems"]) or "None"  # type: ignore[union-attr]

        stats = [
            f"Version Folder: {details['version']}",
            f"Files Present: {len(details['files'])} file(s)",  # type: ignore[arg-type]
            f"Included Manifest Files: {len(details['include_files'])} file(s)",  # type: ignore[arg-type]
            f"Metaitems: {metaitems_str}",
            f"Legacy Borders (<border>): {details['borders_count']}",
            f"Legacy Brushes (<brush>): {details['brushes_count']}",
            f"Legacy Tileset Wrappers: {details['tilesets_count']}",
            f"Item Registry (<item>): {details['items_count']}",
            f"Creature Registry (<creature>): {details['creatures_count']}",
            f"Items.otb Present: {'Yes' if details['has_items_otb'] else 'No'}",
            f"Source XML Sanitization Fixes:\n{issues_summary}",
        ]
        self.txt_before_stats.SetValue("\n".join(stats))

    def _load_after_preview(self, version_dir: Path) -> None:
        """Populate the 'After' panel with expected modular structure and stats."""
        try:
            preview = preview_normalized(version_dir)
        except Exception as exc:
            self.txt_after_stats.SetValue(f"Failed to generate preview: {exc}")
            return

        # Target subfolder name
        subfolder = preview["version"]
        custom = self.txt_folder_name.GetValue().strip()
        if custom and is_valid_folder_name(custom):
            subfolder = custom

        out_root_str = self.txt_output.GetValue().strip()
        if not out_root_str:
            target_display = f"<Choose Output Dir>/{subfolder}"
        else:
            target_display = f"{out_root_str}/{subfolder}"

        # Populate tree
        self.tree_after.DeleteAllItems()
        root_id = self.tree_after.AddRoot(target_display)

        folder_nodes: dict[str, wx.TreeItemId] = {}

        for rel_file in preview["expected_files"]:  # type: ignore[union-attr]
            parts = rel_file.split("/")
            if len(parts) == 1:
                self.tree_after.AppendItem(root_id, parts[0])
            else:
                curr_id = root_id
                for i, part in enumerate(parts[:-1]):
                    subpath = "/".join(parts[: i + 1])
                    if subpath not in folder_nodes:
                        folder_nodes[subpath] = self.tree_after.AppendItem(curr_id, part + "/")
                    curr_id = folder_nodes[subpath]
                self.tree_after.AppendItem(curr_id, parts[-1])

        self.tree_after.Expand(root_id)
        for node in folder_nodes.values():
            self.tree_after.Expand(node)

        # Palette counts breakdown
        palettes_str = ", ".join(f"{k}: {v}" for k, v in preview["palettes"].items())  # type: ignore[union-attr]
        wrappers_str = ", ".join(f"{k}: {v}" for k, v in preview["tilesets_by_wrapper"].items())  # type: ignore[union-attr]

        validation = preview["validation"]  # type: ignore[union-attr]
        norm_counts = validation.get("normalized_counts", {})
        unresolved = validation.get("unresolved", {})
        unresolved_total = sum(len(v) for v in unresolved.values())

        smoke_status = "Pending conversion execution"
        # Check if already converted on disk
        if out_root_str:
            actual_output = Path(out_root_str) / subfolder
            report_file = actual_output / "conversion_report.json"
            if report_file.exists():
                try:
                    import json
                    data = json.loads(report_file.read_text(encoding="utf-8"))
                    failures = data.get("smoke_test", {}).get("failures", [])
                    if failures:
                        smoke_status = f"FAILED on disk ({len(failures)} invalid XML)"
                    else:
                        smoke_status = f"PASSED on disk ({len(data.get('generated_files', []))} files verified)"
                except Exception:
                    smoke_status = "Converted on disk (report available)"

        stats = [
            f"Target Location: {target_display}",
            f"Expected Generated Files: {len(preview['expected_files'])} file(s)",  # type: ignore[arg-type]
            f"Modular Tilesets: {preview['tilesets_count']} files across categories",
            f"Tilesets by Category: {wrappers_str}",
            f"Palettes Mapped: {palettes_str}",
            f"Assigned Creatures: {norm_counts.get('assigned_creatures', 0)} (Fallbacks: NPCs & Others)",
            f"Assigned Raw Items: {norm_counts.get('assigned_raw_items', 0)} (Fallback: Others.xml)",
            f"Unresolved References: {unresolved_total} legacy reference issue(s)",
            f"XML Smoke-Test Validation: {smoke_status}",
        ]
        self.txt_after_stats.SetValue("\n".join(stats))

        # Diagnostics Tab
        diag_lines = [
            f"=== DIAGNOSTICS & VALIDATION REPORT FOR VERSION {preview['version']} ===",
            f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "--- UNRESOLVED LEGACY REFERENCES ---",
        ]

        has_unresolved = False
        for ref_category, items in unresolved.items():
            if items:
                has_unresolved = True
                diag_lines.append(f"[{ref_category}] ({len(items)}):")
                for item in items[:15]:
                    diag_lines.append(f"  • {item}")
                if len(items) > 15:
                    diag_lines.append(f"  ... and {len(items) - 15} more")
        if not has_unresolved:
            diag_lines.append("  No unresolved references found. All brush, creature, and border references resolved cleanly!")

        diag_lines.append("")
        diag_lines.append("--- SOURCE XML REPAIRS / SANITIZATION MESSAGES ---")
        if preview["issues"]:
            for issue in preview["issues"]:
                diag_lines.append(f"  • {issue['path']}: {', '.join(issue['messages'])}")
        else:
            diag_lines.append("  No XML sanitization issues found.")

        self.txt_diagnostics.SetValue("\n".join(diag_lines))

    # --- Conversion Execution & Threading ---

    def _append_log(self, level: str, message: str) -> None:
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        prefix = f"[{timestamp}] [{level}] "
        self.txt_log.AppendText(f"{prefix}{message}\n")

    def _update_progress(self, current: int, total: int, status: str) -> None:
        if total > 0:
            pct = int((current / total) * 100)
            self.gauge.SetValue(pct)
        self.lbl_progress_status.SetLabel(status)

    def _on_start_conversion(self, event: wx.Event) -> None:
        if self.is_converting:
            return

        # 1. Validate Source Path
        src_val = self.txt_source.GetValue().strip()
        if not src_val:
            wx.MessageBox("Please select a legacy client data source folder.", "Source Folder Missing", wx.OK | wx.ICON_WARNING)
            return

        source_path = Path(src_val)
        if not source_path.exists() or not source_path.is_dir():
            wx.MessageBox(f"The selected source folder does not exist or is not a directory:\n{source_path}", "Invalid Source Folder", wx.OK | wx.ICON_ERROR)
            return

        # 2. Determine versions to convert
        checked_versions: list[str] = []
        for i in range(self.check_list_versions.GetCount()):
            if self.check_list_versions.IsChecked(i):
                checked_versions.append(self.check_list_versions.GetString(i))

        if not checked_versions:
            wx.MessageBox("Please select at least one client version to convert.", "No Version Selected", wx.OK | wx.ICON_WARNING)
            return

        # 3. Validate Output Target Directory
        out_dir_str = self.txt_output.GetValue().strip()
        if not out_dir_str:
            wx.MessageBox("Please select a target output directory.", "Missing Output Directory", wx.OK | wx.ICON_WARNING)
            return

        output_root = Path(out_dir_str)

        # 4. Determine custom output folder name (only valid for single version)
        custom_folder_name: str | None = None
        if len(checked_versions) == 1:
            raw_folder_name = self.txt_folder_name.GetValue().strip()
            if raw_folder_name:
                if not is_valid_folder_name(raw_folder_name):
                    wx.MessageBox(
                        f"The folder name '{raw_folder_name}' contains invalid characters.\n"
                        "Please use a valid directory name (without < > : \" / \\ | ? *).",
                        "Invalid Folder Name",
                        wx.OK | wx.ICON_ERROR,
                    )
                    return
                custom_folder_name = raw_folder_name
            else:
                custom_folder_name = checked_versions[0]
        else:
            # Batch conversion: each version gets its own folder named after the version
            custom_folder_name = None

        # Lock UI
        self.is_converting = True
        self.btn_convert.Disable()
        self.btn_browse_source.Disable()
        self.btn_browse_output.Disable()
        self.btn_scan.Disable()
        self.btn_preview.Disable()
        self.gauge.SetValue(0)
        self.lbl_progress_status.SetLabel("Starting conversion...")

        # Switch to log tab
        self.notebook.SetSelection(1)
        self._append_log("INFO", f"=== Conversion job started for {len(checked_versions)} version(s) ===")
        self._append_log("INFO", f"Source: {source_path}")
        self._append_log("INFO", f"Target Output: {output_root}")
        if custom_folder_name:
            self._append_log("INFO", f"Output Folder Name: {custom_folder_name}")

        # Start worker thread
        thread = threading.Thread(
            target=self._worker_thread,
            args=(source_path, output_root, checked_versions, custom_folder_name),
            daemon=True,
        )
        thread.start()

    def _worker_thread(
        self,
        base_dir: Path,
        output_root: Path,
        versions: list[str],
        custom_folder_name: str | None,
    ) -> None:
        success = False
        error_msg: str | None = None
        results: list[tuple[str, Path, list[str]]] = []

        try:
            def log_cb(level: str, msg: str) -> None:
                wx.CallAfter(self._append_log, level, msg)

            def progress_cb(cur: int, tot: int, status: str) -> None:
                wx.CallAfter(self._update_progress, cur, tot, status)

            results = run_conversion(
                base_dir=base_dir,
                output_root=output_root,
                versions=versions,
                target_folder_name=custom_folder_name,
                log_cb=log_cb,
                progress_cb=progress_cb,
            )
            success = True
        except Exception as exc:
            error_msg = str(exc)

        wx.CallAfter(self._on_conversion_finished, success, results, error_msg, output_root)

    def _on_conversion_finished(
        self,
        success: bool,
        results: list[tuple[str, Path, list[str]]],
        error_msg: str | None,
        output_root: Path,
    ) -> None:
        self.is_converting = False
        self.btn_convert.Enable()
        self.btn_browse_source.Enable()
        self.btn_browse_output.Enable()
        self.btn_scan.Enable()
        self.btn_preview.Enable()

        if success:
            self.gauge.SetValue(100)
            self.lbl_progress_status.SetLabel(f"Done! Converted {len(results)} version(s).")
            self._append_log("SUCCESS", f"All {len(results)} version(s) successfully converted!")

            if results:
                self.last_converted_path = results[0][1]
            else:
                self.last_converted_path = output_root

            self.btn_open_output.Enable(True)
            self._refresh_preview()

            wx.MessageBox(
                f"Conversion completed successfully for {len(results)} version(s)!\nOutput written to:\n{output_root}",
                "Conversion Complete",
                wx.OK | wx.ICON_INFORMATION,
            )
        else:
            self.lbl_progress_status.SetLabel("Conversion failed!")
            self._append_log("ERROR", f"Conversion aborted with error: {error_msg}")
            wx.MessageBox(
                f"Conversion failed:\n{error_msg}",
                "Conversion Error",
                wx.OK | wx.ICON_ERROR,
            )

    def _on_open_output_folder(self, event: wx.Event) -> None:
        if self.last_converted_path and self.last_converted_path.exists():
            open_folder_in_explorer(self.last_converted_path)
        else:
            target = Path(self.txt_output.GetValue().strip())
            if target.exists():
                open_folder_in_explorer(target)

    def _on_copy_log(self, event: wx.Event) -> None:
        text = self.txt_log.GetValue()
        if wx.TheClipboard.Open():
            wx.TheClipboard.SetData(wx.TextDataObject(text))
            wx.TheClipboard.Close()
            self.lbl_progress_status.SetLabel("Log copied to clipboard.")

    def _on_save_log(self, event: wx.Event) -> None:
        dlg = wx.FileDialog(
            self,
            "Save Conversion Log",
            wildcard="Text files (*.txt)|*.txt|All files (*.*)|*.*",
            style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT,
        )
        if dlg.ShowModal() == wx.ID_OK:
            path = Path(dlg.GetPath())
            path.write_text(self.txt_log.GetValue(), encoding="utf-8")
            self._append_log("INFO", f"Log saved to {path}")
        dlg.Destroy()


def launch_gui(initial_base_dir: Path | None = None, initial_output_dir: Path | None = None) -> int:
    """Launch the wxPython application."""
    app = wx.App(False)
    frame = ConverterFrame(initial_base_dir=initial_base_dir, initial_output_dir=initial_output_dir)
    frame.Show()
    app.MainLoop()
    return 0


if __name__ == "__main__":
    sys.exit(launch_gui())
