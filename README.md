<p align="left">
  <a href="https://suppi.pl/karolak6612" style="text-decoration: none; border: none;"><img src="https://github.com/user-attachments/assets/39c46c55-bcdb-42d0-9f2a-b23bb05b4019" alt="Suppi" width="160" height="90" style="display: inline-block; vertical-align: middle;"></a>&nbsp;&nbsp;&nbsp;&nbsp;<a href="https://revolut.me/karolak6612" style="text-decoration: none; border: none;"><img width="160" height="90" alt="obraz" src="https://github.com/user-attachments/assets/6f2c7220-9108-4274-817d-fbabeb5cdcc3" /></a>
</p>




# Remere's Map Editor: Redux

**This is WIP. No stable release yet.**

Hi everyone,

Building a Tibia map editor from scratch gave me a massive boost in knowledge regarding tile-based system optimization and using agentic workflows to upgrade legacy code in a slow, precise, and verifiable manner.

I decided to apply this knowledge to the "gold standard" of the OT community - Remere's Map Editor (RME) - using a modern approach. I am proud to announce **RME: Redux**, a project aimed at rewriting the RME backend with modern technologies.

After ~900 commits, here is a brief summary of what has been achieved:

- Decoupled the monolithic codebase into smaller, single-responsibility modules for better maintainability.
- Organized the code into a cleaner, more logical folder hierarchy.
- Upgraded OpenGL from 1.x to 4.x and WxWidgets from 2.9.x to 3.3.x.
- Introduced new rendering for GUI and overlay passes using NanoVG.
- Completely rewrote the map renderer to support async sprite loading, sprite batching, ring buffers, and more. You can now run RME at 160+ FPS.
- Rewrote the light renderer into a pure GPU-based system.

**New Features:**
- New tooltip system (NanoVG).
- In-game preview with player walking simulation.
- Autoborder preview.
- New tile replacement with a custom similarity-finder algorithm.
- Shader system (Anti-aliasing, Retro-CRT, 4xBRZ).
- SVG support.
- and more :)

- Refactored a significant portion of the code into C++20.
- Removed extensive dead code, raw pointers, "hacks," and poor memory management. I’ve fixed more bugs than I can count.
- Numerous Quality-of-Life changes to both the code and the UI.

**Note:** Since I’ve changed so much at once, I have inevitably introduced some new bugs! :)

## Known Issues

- Zone brushes are currently non-functional.
- Tool options buttons (changing doors, arches, etc.) are not working.
- Changing clients/map versions requires an app restart.
- The fog system and minimap export are currently disabled.
- There are likely more bugs I haven't caught yet

## The Roadmap

- Unified properties and browser field window.
- Tileset Overhaul: No more hardcoded tilesets.
- Integrated Palette/Tileset/Brush/Border editor with a WYSIWYG right-click interface.
- Enhanced autoborder with custom rules.
- Native Tibia assets/Protobuf loading.
- CipSoft format support (items.srv, map.sec, etc.).

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

<!-- PLACEHOLDER: Paste GUI screenshot here -->
<!-- Example: <img src="docs/images/legacy_converter_gui.png" alt="RME Legacy Converter GUI" width="800" /> -->

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

## Why did I start this?

As a casual map maker, many things bothered me about the original RME for years. AI technology finally allowed me to implement the changes I wanted to see. Building a map editor with ImGui (which is not abandoned) taught me a lot, and I wanted to bring that level of modernization to RME.

The project is **100% open-source** and available here:

**GitHub:** https://github.com/karolak6612/remeres-map-editor-redux

Latest compiled binary is here:

**Release Performance Test 0.0.1d** → https://github.com/karolak6612/remeres-map-editor-redux/releases/tag/Test

Keep in mind - this is not release build. Its buggy.

## Disclaimers

- This project is largely "vibe-coded." About 90% of the code was generated by various coding agents following SOLID principles.
- This project does not aim for 1:1 RME compatibility. Some features are completely reinvented and may introduce breaking changes. However, loading and editing OTBM maps should work in both the original RME and this version.
- **Bug Reports:** Please use the GitHub Issues page for bug reports.

## Support the Project

I develop this project for free as a hobby in my spare time. Many people on the OTA Discord have asked how they can support the project, and now you can.

The terms are simple:

- If you need a specific feature, consider a donation. We can discuss if it’s feasible and fits the project’s vision. I do not make private edits. I am firmly against gatekeeping; all funded features will be merged into the public repo for everyone.
- All donations go toward funding AI licenses (Cursor, Claude Code, Google Pro/Ultra) to keep this hobby free.

**Links:**

- [PL] Suppi by Patronite → https://suppi.pl/karolak6612
- [World] https://revolut.me/karolak6612

## Some media

**Autoborder preview:**

<!-- Replace with direct image link if you upload them to repo or use raw.githubusercontent -->
<!-- For now using imgur placeholders as in original text -->
https://imgur.com/7bQrM09

**UI preview:**

https://imgur.com/qeaMwws

**Performance comparison vs RME OTA:**

https://imgur.com/Y3zecwk

**Player walking:**

https://imgur.com/5ZPcuxa

## Some screenshots

<img width="2553" height="1390" alt="obraz" src="https://github.com/user-attachments/assets/ee37c4f8-4997-4358-965d-f462dac4b603" />

<img width="1485" height="850" alt="obraz" src="https://github.com/user-attachments/assets/4e6e8aa9-6353-4a87-9175-ba3e8dc5fbb4" />

<img width="1798" height="1044" alt="obraz" src="https://github.com/user-attachments/assets/4118bff9-6873-450a-8ba9-627c065bf40e" />

<img width="1114" height="929" alt="obraz" src="https://github.com/user-attachments/assets/7d61d223-7d74-4660-8a06-cbc1b57f36f3" />


---

