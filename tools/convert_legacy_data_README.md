# Legacy Data XML Converter

`convert_legacy_data.py` and `convert_legacy_data_gui.py` convert legacy Remere's Map Editor client data folders into the modern modular XML layout used by RME Redux.

The scripts are located in the `tools/` directory. They can convert an individual client folder (such as `800` or `1098`) or scan a directory containing multiple numeric version folders, generating clean, modular XML packages.

## Old XML Structure

Legacy client folders are version-specific and not fully uniform. Each folder contains `items.otb`, `items.xml`, `creatures.xml`, `materials.xml`, and several material XML files.

Typical examples:

```text
800/
  borders.xml
  collections.xml
  creature_palette.xml
  creatures.xml
  doodads.xml
  grounds.xml
  item_palette.xml
  items.otb
  items.xml
  materials.xml
  raw_palette.xml
  tilesets.xml
  walls.xml
  walls_extra.xml
```

```text
1098/
  borders.xml
  creatures.xml
  doodads.xml
  grounds.xml
  items.otb
  items.xml
  materials.xml
  tilesets.xml
  walls.xml
```

### Legacy Entry Point

`materials.xml` is the legacy material manifest. It contains ordered includes and, in many versions, editor-only metaitems:

```xml
<materials>
    <metaitem id="80"/>
    <include file="borders.xml"/>
    <include file="grounds.xml"/>
    <include file="walls.xml"/>
    <include file="doodads.xml"/>
    <include file="tilesets.xml"/>
</materials>
```

The include order matters. The converter preserves that order when collecting borders, brushes, and tilesets.

### Legacy Responsibilities

Legacy files are coupled and often mix different concepts:

| File | Common role |
|---|---|
| `borders.xml` | Defines `<border>` entries and `<borderitem>` edge mappings. |
| `grounds.xml` | Defines ground brushes, border behavior, friends/enemies, and special ground logic. |
| `walls.xml` | Defines wall brushes and can also define wall-related tilesets. |
| `walls_extra.xml` | Defines additional wall detail and archway doodad brushes in `760` and `800`. |
| `doodads.xml` | Defines doodad/table/carpet brushes and, in `760` and `800`, embedded doodad tilesets. |
| `tilesets.xml` | Defines user-facing tileset groupings. Newer versions consolidate most palette structure here. |
| `creature_palette.xml` | Defines creature tilesets in `760` and `800`. Other versions do not have explicit creature palette files. |
| `item_palette.xml` | Defines item palette tilesets in `760` and `800`. |
| `raw_palette.xml` | Defines raw item tilesets in `760` and `800`. |
| `collections.xml` | Defines collection palette tilesets in `760` and `800`. |
| `items.xml` | Raw item registry. `740` also has `items2.xml`, which is merged after `items.xml`. |
| `creatures.xml` | Raw creature registry. |

### Legacy Tileset Wrappers

Legacy `<tileset>` nodes contain category wrapper nodes. These wrappers decide which palette category receives the entries:

```xml
<tileset name="Nature">
    <terrain>
        <brush name="grass"/>
        <brush name="sand"/>
    </terrain>
    <doodad>
        <brush name="trees"/>
    </doodad>
    <raw>
        <item fromid="1285" toid="1359"/>
    </raw>
</tileset>
```

Common wrapper names:

| Wrapper | Target palette |
|---|---|
| `terrain` | Terrain |
| `doodad` | Doodad |
| `collections` | Collection |
| `items` | Item |
| `creatures` | Creature |
| `raw` | Raw |
| `terrain_and_raw` | Terrain and Raw |
| `collections_and_terrain` | Collection and Terrain |
| `doodad_and_raw` | Doodad and Raw |
| `items_and_raw` | Item and Raw |

The converter handles hybrid wrapper names generically by splitting on `_and_`.

## New XML Structure

Converted folders are written to `new_data/<client_version>/`.

Example:

```text
new_data/800/
  borders/
    borders.xml
  brushes/
    brushes.xml
  creatures/
    creatures.xml
  items/
    items.xml
  tilesets/
    collections/
    collections_and_terrain/
    creatures/
    doodad/
    doodad_and_raw/
    items/
    items_and_raw/
    raw/
    terrain/
  conversion_report.json
  items.otb
  materials.xml
  palettes.xml
```

### New Entry Point

`materials.xml` becomes a modular manifest:

```xml
<materials>
    <borders>
        <include folder="borders/" />
    </borders>
    <brushes>
        <include folder="brushes/" />
    </brushes>
    <creatures>
        <include folder="creatures/" />
    </creatures>
    <items>
        <include folder="items/" />
    </items>
    <tilesets>
        <include folder="tilesets/" subfolders="true" />
    </tilesets>
    <palettes>
        <include file="palettes.xml" />
    </palettes>
</materials>
```

Metaitems from legacy `materials.xml` are preserved at the top of the generated `materials.xml`.

### Modular Files

| Output path | Contents |
|---|---|
| `borders/borders.xml` | All legacy `<border>` definitions. |
| `brushes/brushes.xml` | All legacy `<brush>` definitions from material include files. |
| `creatures/creatures.xml` | Raw creature registry from legacy `creatures.xml`. |
| `items/items.xml` | Raw item registry from legacy `items.xml` and, for `740`, `items2.xml`. |
| `tilesets/<category>/<tileset name>.xml` | One modular tileset per legacy tileset wrapper. |
| `palettes.xml` | Top-level palette layout with explicit file includes. |
| `conversion_report.json` | Counts, generated files, smoke-test results, repaired input notes, and unresolved legacy references. |

Tileset file names use the visible tileset name only. Category suffixes such as `_raw` or `_doodad` are not appended. Slashes in tileset names are replaced with spaces.

If two generated files would have the same path in the same folder, the later file receives a numeric suffix such as `_2`.

### Generated Fallback Tilesets

Some legacy versions do not explicitly define creature palette tilesets. In those cases, the converter creates:

```text
tilesets/creatures/Others.xml
```

If the creature registry contains NPC entries, it also creates:

```text
tilesets/creatures/NPCs.xml
```

The converter also creates:

```text
tilesets/raw/Others.xml
```

for raw item ids from `items.xml` that were not assigned to any explicit raw-capable legacy tileset.

## Prerequisites & Fresh System Setup

- **CLI Tool (`convert_legacy_data.py`)**:
  - Requires only **standard Python 3.8+** (no external libraries or packages needed).
- **GUI Application (`convert_legacy_data_gui.py`)**:
  - Requires `wxPython`.
  - **Auto-Installation**: If `wxPython` is not found, the GUI will prompt you and offer to install it automatically via `pip`.
  - **Manual Installation**:
    ```powershell
    # Windows / macOS / Linux via pip:
    pip install -r tools/requirements.txt
    # or
    pip install wxPython
    ```
    On Debian / Ubuntu systems:
    ```bash
    sudo apt install python3-wxgtk4.0
    ```

## GUI & Script Usage

### Graphical User Interface (wxPython)

A graphical interface is available in `tools/convert_legacy_data_gui.py` (or by running `convert_legacy_data.py --gui` or executing without arguments).

```powershell
# From the repository root:
python tools/convert_legacy_data_gui.py

# Or via the CLI launcher:
python tools/convert_legacy_data.py --gui
```

#### GUI Preview:

<!-- PLACEHOLDER: Paste GUI screenshot here -->
<!-- Example: ![RME Legacy Converter GUI](screenshot.png) -->

#### GUI Features:
- **Source Path Selection**: Choose either a base directory containing multiple client version folders (e.g. `data/`) or directly select an individual legacy client folder (e.g. `data/800`).
- **Discovered Versions Checklist**: When scanning a folder containing multiple versions, choose all or specific versions to convert.
- **Target Output Directory & Folder Name**: Select the destination folder and customize the output folder name for single-version conversions.
- **Before & After View**:
  - **Before (Legacy Data)**: Lists all discovered legacy files (`materials.xml`, `items.xml`, `creatures.xml`, `grounds.xml`, `walls.xml`, etc.) with file sizes, plus element counts (`<border>`, `<brush>`, `<tileset>`, `<creature>`, item coverage, metaitem IDs, and detected XML sanitization fixes).
  - **After (Modular Layout)**: Previews the expected modular folder tree (`borders/`, `brushes/`, `creatures/`, `items/`, `tilesets/<category>/...`, `materials.xml`, `palettes.xml`), palette mappings, fallback tilesets, and smoke-test validation results.
- **Conversion Log**: Live scrolling log window with timestamps, progress gauge, step status messages, and options to copy or save the log.
- **Diagnostics Tab**: Summarizes unresolved legacy references (missing brush names, creature references, border references) and XML sanitization repairs.
- **Open Output Folder**: One-click button to open the converted folder in Windows Explorer / system file manager upon completion.

---

### Command-Line Usage

From repository root:

```powershell
# Convert specific versions from legacy data directory:
python tools/convert_legacy_data.py --base-dir C:\path\to\legacy_data --output-dir C:\path\to\output --versions 740 800 1098

# Convert a single client folder with custom output subfolder name:
python tools/convert_legacy_data.py --base-dir C:\path\to\legacy\800 --output-dir C:\path\to\output --output-name 800_modular
```

## Validation

The converter validates generated output as part of each run:

1. It writes `conversion_report.json` for every converted version.
2. It smoke-parses every generated XML file.
3. It records source repairs, such as removed NUL bytes, fallback `cp1252` decoding, or escaped malformed ampersands.
4. It records unresolved legacy references without stopping conversion.

To compile-check the script:

```powershell
python -m py_compile .\convert_legacy_data.py
```

To inspect whether a converted version had XML smoke-test failures, open:

```text
new_data/<client_version>/conversion_report.json
```

and check:

```json
"smoke_test": {
  "failures": []
}
```

## Conversion Notes

The converter preserves semantic XML content and declaration order, not byte-for-byte formatting.

The script intentionally targets the modular loader architecture represented by `new_data/`. It does not rewrite the C++ loader and does not make the modular output compatible with older legacy-only material loading code.

Known source data issues are repaired only enough to parse and preserve the XML structure:

| Source issue | Repair |
|---|---|
| NUL bytes in XML files | Removed before parsing. |
| Non-UTF-8 text in XML files | Decoded with `cp1252` fallback. |
| Bare `&` in attribute values | Escaped to `&amp;`. |
| Invalid XML control characters | Removed before parsing. |

The report file is the source of truth for what the converter repaired or could not resolve for each version.
