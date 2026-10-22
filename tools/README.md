# tibia_data.py

Self-contained Tibia client data inspector for AI agents and developers.

It reads the three classic client files — `Tibia.dat`, `Tibia.spr` and
`items.otb` — auto-detects the client version and format variant, and prints
(or writes) JSON. It is read-only, uses only the Python standard library and
needs no installation.

```
DAT  thing metadata    items, outfits, effects, missiles
SPR  sprite images     32x32 sprite blobs + PNG export
OTB  server item data  items.otb: server id <-> client id, flags, attributes
```

---

## Requirements

- Python 3.9 or newer (tested with 3.14)
- No third-party packages
- A client data folder (e.g. `Tests1098/` in this repository)

---

## Quick start

```bash
python tools/tibia_data.py \
  --dat Tests1098/Tibia.dat \
  --spr Tests1098/Tibia.spr \
  --otb Tests1098/items.otb \
  info
```

Output (abridged):

```json
{
  "status": "ok",
  "gameVersion": { "label": "10.98", "value": 1098, "datSignature": "0x42A3", "sprSignature": "0x57BBD603" },
  "features": { "extended": true, "transparency": false, "improvedAnimations": true, "frameGroups": true },
  "counts": { "minItemId": 100, "maxItemId": 23725, "items": 23626, "outfits": 903, "effects": 175, "missiles": 54, "sprites": 338944, "otbItems": 26282 },
  "result": { "otb": { "version": "3.57.62", "csdVersion": "OTB 3.57.62-10.98", "itemCount": 26282 } },
  "warnings": []
}
```

> **Option placement:** global options (`--dat`, `--output`, `--compact`,
> `--force-version`, feature overrides) must appear **before** the command.
> Command-specific options (`--category`, `--dump-sprites`, `--sheet`,
> `--out`) go after it.

```bash
# correct
python tools/tibia_data.py --dat X --output out.json client 930

# wrong: argparse will reject --output here
python tools/tibia_data.py --dat X client 930 --output out.json
```

---

## ID numbering (important)

| Category | First id | DAT header meaning |
|---|---|---|
| `item` | **100** | max item id (e.g. 23725) |
| `outfit` | 1 | number of outfits |
| `effect` | 1 | number of effects |
| `missile` | 1 | number of missiles |

So for 10.98: 23,626 item entries with client ids **100 … 23,725**.
`client 99` is invalid. Server ids (OTB) are a separate namespace: the OTB
maps them to client ids through the `SERVER_ID` / `CLIENT_ID` attributes.

---

## Commands

### `info`

Detects and reports the version, feature flags, counts and OTB header.

```bash
python tools/tibia_data.py --dat ... --spr ... --otb ... info
```

### `client <id> [--category item|outfit|effect|missile]`

Dumps a DAT thing by client id: every flag, numeric property, flag sequence,
frame groups (geometry, animation, frame durations, sprite ids) and the unique
sprite id list. For items, it also resolves the OTB server item(s) mapped to
that client id.

```bash
python tools/tibia_data.py --dat ... --spr ... --otb ... client 930
python tools/tibia_data.py --dat ... --spr ... client 12 --category outfit
python tools/tibia_data.py --dat ... --spr ... client 3 --category effect
python tools/tibia_data.py --dat ... --spr ... client 1 --category missile
```

Extra options:

- `--dump-sprites DIR` — write every referenced sprite as a PNG into `DIR`
- `--sheet` — also compose a sprite sheet PNG (requires `--dump-sprites`)

### `server <id>`

Dumps an OTB item by **server id**: group, modeled flags, raw flag bits,
decoded attributes, raw attribute list, sprite hash — plus the mapped DAT
client thing (when `--dat` is given).

```bash
python tools/tibia_data.py --dat ... --spr ... --otb ... server 930
```

`--dump-sprites DIR` / `--sheet` work here too and export the mapped DAT
thing's sprites.

### `sprite <id> [--out DIR]`

Dumps a single SPR sprite: address, compressed size, visible pixels, SHA-1 of
the compressed data, and optionally writes `sprite_<id>.png` into `DIR`.

```bash
python tools/tibia_data.py --spr Tests1098/Tibia.spr sprite 5255 --out dumps/
```

---

## Global options

| Option | Description |
|---|---|
| `--dat FILE` | `Tibia.dat` metadata file |
| `--spr FILE` | `Tibia.spr` sprite file |
| `--otb FILE` | `items.otb` server item file |
| `--output FILE` | write the JSON document to `FILE` (UTF-8) instead of stdout |
| `--compact` | compact JSON instead of pretty printed |
| `--force-version LABEL` | override auto-detection (e.g. `10.98`, `8.60 v2`) |
| `--extended` / `--no-extended` | override uint32 sprite ids (DAT/SPR header) |
| `--transparency` / `--no-transparency` | override RGBA sprite pixels |
| `--improved-animations` / `--no-improved-animations` | override frame duration data |
| `--frame-groups` / `--no-frame-groups` | override outfit frame groups |
| `--debug` | print Python tracebacks instead of a JSON error |

Only provide the files a command needs: `client` needs `--dat` (and `--otb`
for the server mapping, `--spr` for sprite export); `server` needs `--otb`;
`sprite` needs `--spr`.

---

## Output format

Every successful invocation prints one JSON document:

```json
{
  "status": "ok",
  "tool": "tibia_data.py",
  "files": { "dat": "...", "spr": "...", "otb": "..." },
  "gameVersion": { "label": "10.98", "value": 1098, "matches": ["10.98"] },
  "features": {
    "extended": true,
    "transparency": false,
    "improvedAnimations": true,
    "frameGroups": true,
    "detection": { "datValidated": true, "reader": "MetadataReader6", "attempts": [...], "transparency": {...} }
  },
  "counts": { "items": 23626, "maxItemId": 23725, "sprites": 338944, ... },
  "result": { "...command specific..." },
  "warnings": []
}
```

- `client` → `result.clientThing`, `result.otbMapping.serverIds` / `.serverItems`
- `server` → `result.serverItem`, `result.clientThing`
- `sprite` → `result.sprite`, `result.pngFile` (with `--out`)
- `info` → `result.otb`

Errors are JSON too, with exit code `1`:

```json
{ "status": "error", "tool": "tibia_data.py", "errorType": "ParseError", "error": "item id 99 is out of range 100..23725" }
```

---

## Examples

Query a client item and find its server ids:

```bash
python tools/tibia_data.py \
  --dat Tests1098/Tibia.dat --spr Tests1098/Tibia.spr --otb Tests1098/items.otb \
  client 930
# result.otbMapping.serverIds == [784]
```

Query a server item and find its client data:

```bash
python tools/tibia_data.py \
  --dat Tests1098/Tibia.dat --spr Tests1098/Tibia.spr --otb Tests1098/items.otb \
  server 930
# result.serverItem.clientId == 1162, result.clientThing.id == 1162
```

Dump item data to a file and export its sprites:

```bash
python tools/tibia_data.py \
  --dat Tests1098/Tibia.dat --spr Tests1098/Tibia.spr --otb Tests1098/items.otb \
  --output dumps/client_item_930.json \
  client 930 --dump-sprites dumps/sprites/client930 --sheet
```

---

## Auto-detection

1. **Version** — the DAT and SPR signatures are matched against the built-in
   table from `src/config/versions.xml` (7.10 … 13.10+, including variants
   such as `8.50 v2`, `8.54 v3`, `10.93 test`). DAT-only or SPR-only matching
   is used when one file is missing; ambiguous matches are listed in
   `gameVersion.matches`.
2. **Reader** — `MetadataReader1` … `MetadataReader6` selected by version,
   mirroring `MetadataControllerStorage`.
3. **Features** — candidate combinations of `extended`, `improvedAnimations`
   and `frameGroups` are tried until the entire DAT parses exactly to EOF
   (and all sprite ids fit into the SPR file). `transparency` is probed from
   the SPR chunk streams (3- vs 4-byte pixels). The chosen values are shown
   under `features`, with the validation details in `features.detection`.

If a custom client uses an unknown signature, pass `--force-version`; if its
layout differs, use the feature overrides.

---

## Sprite export

- `DIR/sprite_<spriteId>.png` — one 32×32 RGBA PNG per referenced sprite
- `DIR/<category><id>_sheet.png` — grid of all referenced sprites (8 columns,
  row-major in DAT order); `result.spriteExport.sheetOrder` lists the order
- Empty sprites are exported as fully transparent PNGs
- Duplicate sprite references are exported once

---

## Tests

```bash
python -m unittest discover -s tools -p "test_*.py" -v
```

The suite (40 tests) builds synthetic DAT/SPR/OTB files to test the parsers,
CLI behavior, error handling, PNG output and feature detection, and includes
integration tests against `Tests1098/` (version/count checks plus a
sprite-hash oracle: the tool recomputes the MD5 sprite hashes stored in
`items.otb` from the DAT + SPR and compares them).

---

## Limitations

- Standard 32×32 sprites only; custom OTFI `sprite-size` / `sprite-data-size`
  clients are not auto-detected.
- Uncompressed `items.otb` only (no gzip/LZMA variants).
- Item client ids are assumed to start at 100 for every version (matching
  ObjectBuilder's `ThingTypeStorage.MIN_ITEM_ID`).
- Version signatures are limited to the embedded table; use
  `--force-version` for custom clients.
- If multiple OTB items share a server id, the first is used and the rest are
  returned in `result.additionalMatches`.
