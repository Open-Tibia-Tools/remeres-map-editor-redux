<p align="left">
  <a href="https://suppi.pl/karolak6612" style="text-decoration: none; border: none;"><img src="https://github.com/user-attachments/assets/39c46c55-bcdb-42d0-9f2a-b23bb05b4019" alt="Suppi" width="160" height="90" style="display: inline-block; vertical-align: middle;"></a>&nbsp;&nbsp;&nbsp;&nbsp;<a href="https://revolut.me/karolak6612" style="text-decoration: none; border: none;"><img width="160" height="90" alt="obraz" src="https://github.com/user-attachments/assets/6f2c7220-9108-4274-817d-fbabeb5cdcc3" /></a>
</p>




# Remere's Map Editor: Redux

**Note: This is an active work-in-progress. There is no final stable release yet, only experimental alpha builds.**

Hi everyone,

Building a map editor from scratch taught me a ton about optimizing tile engines and refactoring legacy code. I wanted to take those lessons and apply them to the gold standard of the Open Tibia community: Remere's Map Editor (RME).

RME: Redux is an ongoing project to overhaul RME's aging backend with modern tech, fix long-standing engine bottlenecks, and make mapping smoother and faster for everyone.

After nearly 1,500 commits, here is what has changed and what works today.

---

## What is new in Redux?

### Rendering & Performance
- Rewrote the map renderer with modern OpenGL (Core Profile), async sprite loading, sprite batching, and ring buffers. Runs easily at 160+ FPS without stuttering.
- Switched the lighting system to a GPU-based tile lighting engine with support for server light colors.
- Added visual shaders: Anti-aliasing, Retro-CRT, and 4xBRZ.
- Added configurable floor visibility modes so you can control how upper and lower floors look while editing.
- Replaced legacy UI overlay drawing with NanoVG (smoother tooltips, selection boxes, etc.).

### Modern Client & Asset Support
- Native Protobuf support: can load `appearances.protobuf` and modern sprite files directly (Canary / Tibia 11+) alongside classic `.dat`/`.spr` and `.otb`.
- Support for multiple asset modes: OTB+DAT, Protobuf+OTB, or Protobuf-only.
- Invalid OTBM tile safety: unrecognized or custom items on the map are preserved and marked instead of being silently deleted when saving.

### Palettes & Quality of Life
- Completely modular dynamic palettes: tilesets are no longer hardcoded into the binary. They are split into clean folders (`terrain`, `doodad`, `creatures`, `items`, `raw`).
- Live search bar and filter tool: search across all palettes and jump straight to the tile or creature you need.
- Palette toolbar: resize tile previews, sort alphabetically or by server ID, and toggle labels.
- Fallback looks: creatures without a looktype or with missing sprites show a placeholder (like the Citizen outfit) instead of disappearing.
- Advanced item finder with category filters and pagination.

### Tools, Minimap & In-Game Simulation
- Minimap exporter: export your map (or multiple floors) to OTMM, JPG, or WebP with adjustable scale and screenshot capture.
- In-game walking preview: simulate player movement with walk animations right on the canvas.
- Autoborder preview: see border transitions live before clicking.
- Legacy XML converter: CLI and GUI tools to convert old RME data into the new modular format.

### Built-in Lua Scripting
- Integrated Lua 5.4 scripting engine with Sol2 bindings.
- Procedural generation tools: build terrain, caves, and biomes using built-in Simplex and Perlin noise.
- Custom UI dialogs: create interactive scripts with windows and inputs using `app.create_dialog`.
- Full map automation API for tiles, items, creatures, selections, and HTTP requests.
- See `scripts/README.md` for full scripting documentation and examples.

### Codebase Cleanups
- Bumped standard to C++23.
- Upgraded wxWidgets from 2.9.x to 3.3.x.
- Cut out a massive amount of dead code, raw pointer leaks, and legacy workarounds.

---

## Downloads

Automated alpha builds are compiled for Windows and Linux on every update:

- **Releases page**: https://github.com/Open-Tibia-Tools/remeres-map-editor-redux/releases

Download the latest alpha archive (`.zip` for Windows, `.tar.gz` for Linux), extract it, and run the editor.

Remember that these are alpha builds. Keep backups of your maps.

---

## How to migrate brushes from RME to Redux?

RME Redux replaces the legacy monolithic XML format (where brushes, grounds, walls, doodads, and tilesets were mixed together across multiple files like `grounds.xml`, `walls.xml`, and `tilesets.xml`) with a **modular XML architecture**:

```text
<client_version>/
├── borders/
│   └── borders.xml
├── brushes/
│   └── brushes.xml
├── creatures/
│   └── creatures.xml
├── items/
│   └── items.xml
├── tilesets/
│   ├── terrain/
│   ├── doodad/
│   ├── creatures/
│   ├── items/
│   └── raw/
├── materials.xml
├── palettes.xml
├── items.otb
└── conversion_report.json
```

To migrate your custom brushes, tilesets, and client data from legacy RME into the Redux format, use the **Legacy Data XML Converter** located in `tools/`.

### 1. Using the Graphical User Interface (GUI)

A cross-platform (Windows & Linux) graphical tool is available:

```powershell
python tools/convert_legacy_data_gui.py
```

*(If `wxPython` is not installed on your system, the tool will automatically offer to install it for you on startup).*
<img width="1208" height="838" alt="obraz" src="https://github.com/user-attachments/assets/32e030db-84ff-40e3-9289-c7ddbb9a1bba" />

#### Step-by-Step Guide:
1. **Choose Source Path**: Click **Browse...** to select your legacy client folder (e.g. `800` containing `materials.xml`) or a directory containing multiple client versions.
2. **Choose Target Output Directory**: Select where you want the converted modular folders to be saved.
3. **Set Output Folder Name**: If converting an individual version, choose the subfolder name (defaults to the version name, e.g. `800`).
4. **Review Before & After**:
   - **Before**: Inspects your legacy files, detected `<border>`, `<brush>`, `<tileset>`, `<creature>`, and item counts, and flags any legacy XML issues (like NUL bytes or unescaped ampersands).
   - **After**: Previews the modular layout and palette mappings that will be generated.
5. **Start Conversion**: Click **Start Conversion**. The tool runs in the background while real-time progress and XML smoke-test validation stream to the **Conversion Log** tab.
6. **Open Output Folder**: Click **Open Output Folder** to view the converted files.

---

### 2. Using the Command-Line Interface (CLI)

For headless environments or automation, use `tools/convert_legacy_data.py` (requires only standard Python 3.8+, no external libraries needed):

```powershell
# Convert a specific client version:
python tools/convert_legacy_data.py --base-dir C:\path\to\legacy\800 --output-dir C:\path\to\output --output-name 800

# Batch convert all versions in a legacy data folder:
python tools/convert_legacy_data.py --base-dir C:\path\to\legacy_data --output-dir C:\path\to\output --versions 740 800 1098
```

For complete documentation and technical details, see [`tools/convert_legacy_data_README.md`](tools/convert_legacy_data_README.md).

---

## Building from Source

Check `BUILDING.md` for complete step-by-step instructions.

### Windows (Visual Studio 2022)
1. Install VS 2022 with C++ desktop workload, CMake, and vcpkg.
2. Run `build_windows.bat`.
3. The executable will be in `build\Release\rme.exe` (or open `build\rme.sln`).

### Linux (Ubuntu)
1. Run `./setup_conan.sh` to install dependencies.
2. Run `./build_clang.sh` (fast Clang+Ninja build) or `./linux_build.sh` (GCC build).
3. The executable will be in `build_clang/build/Debug/rme`.

---

## Current Status & Roadmap

### What is done:
- Modern OpenGL core renderer with batching and high FPS.
- Tile-based lighting with server light colors.
- Dynamic modular palettes with search and sorting.
- Native Protobuf appearances and asset bundles.
- Minimap export (OTMM, JPG, WebP).
- Lua 5.4 scripting engine with procedural noise tools.
- Invalid OTBM item protection.
- Legacy data conversion tools (GUI and CLI).

### Still being worked on:
- Unified properties and browser window.
- Right-click WYSIWYG editor for palettes, tilesets, brushes, and borders.
- CipSoft file format support (items.srv, map.sec).
- Custom autoborder rule editor.
- Tool option buttons refinements.

---

## Why did I start this?

As a casual map maker, many things bothered me about the original RME for years. AI tools finally gave me a way to implement the improvements I wanted to see without getting stuck in 15-year-old legacy roadblocks.

The project is 100% open source under the GPLv3 license.

## Disclaimers

- This project is largely "vibe-coded." Much of the code was generated and refactored using various AI coding agents following solid engineering practices.
- This project does not aim for 1:1 bug-for-bug compatibility with old RME. Some systems are rewritten completely. However, opening, editing, and saving standard OTBM maps should work normally.
- Since so much has changed under the hood, there will be bugs. Please report anything you find on the GitHub Issues page so we can fix it.

## Support the Project

I work on this project for free as a hobby in my spare time.

If you want to support development, donations go directly toward funding the AI licenses (Claude Code, Cursor, Google Pro/Ultra) that make this fast development pace possible:

- Suppi by Patronite (PL): https://suppi.pl/karolak6612
- Revolut (Global): https://revolut.me/karolak6612

No private edits or gatekept features - anything funded is merged into the public repo for everyone.

---

## Media & Demos

### Video Previews
- Autoborder preview: https://imgur.com/7bQrM09
- UI preview: https://imgur.com/qeaMwws
- Performance comparison vs RME OTA: https://imgur.com/Y3zecwk
- Player walking simulation: https://imgur.com/5ZPcuxa

### Screenshots

<img width="2553" height="1390" alt="RME Redux Overview" src="https://github.com/user-attachments/assets/ee37c4f8-4997-4358-965d-f462dac4b603" />

<img width="1485" height="850" alt="Map Editing" src="https://github.com/user-attachments/assets/4e6e8aa9-6353-4a87-9175-ba3e8dc5fbb4" />

<img width="1798" height="1044" alt="Lighting and Shaders" src="https://github.com/user-attachments/assets/4118bff9-6873-450a-8ba9-627c065bf40e" />

<img width="1114" height="929" alt="Palette Tools" src="https://github.com/user-attachments/assets/7d61d223-7d74-4660-8a06-cbc1b57f36f3" />

