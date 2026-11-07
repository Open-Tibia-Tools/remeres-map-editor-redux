# AGENTS.md

**Remere's Map Editor (RME) Redux**: a tile-based map editor for Open Tibia servers. Built with modern C++20/C++23, wxWidgets, OpenGL 4.6, and NanoVG. Maps hold millions of tiles, so the render and tile-traversal paths are performance-critical.

---

## Tooling & Build Workflow

- **Primary Platform & Target**: **Windows x64** is the primary, canonical build and development target.
- **Primary Build Tool**: Use `build_ninja.bat` from the repository root (e.g. `cmd /c build_ninja.bat`). It automatically sets up the MSVC toolchain, locates vcpkg dependencies, and drives Ninja to compile `build-ninja/rme.exe` and generate `compile_commands.json`.
- For incremental builds, run `cmd /c build_ninja.bat`.
- **Client Data Inspection Tool (`tools/tibia_data.py`)**: Zero-dependency Python 3.9+ CLI tool to inspect, dump, and validate Tibia client assets (`Tibia.dat`, `Tibia.spr`, `items.otb`).
  - Print file info & signatures: `python tools/tibia_data.py info --dat <Tibia.dat> --spr <Tibia.spr> --otb <items.otb>`
  - Inspect item by client or server ID: `python tools/tibia_data.py item --client <id> --dat <Tibia.dat>` or `python tools/tibia_data.py item --server <id> --otb <items.otb>`
  - Export sprite to PNG: `python tools/tibia_data.py sprite <sprite_id> --spr <Tibia.spr> -o sprite.png`
  - Run tool unit tests: `python tools/test_tibia_data.py`
  - See `tools/README.md` for full command reference and JSON output options.

---

## Priority Order

When rules conflict, the higher one wins:

1. **Correctness and data safety** (no corrupted maps, no broken undo, no crashes).
2. **Project invariants** (the "Hard rules" below).
3. **Scope discipline** (the change does what was asked, nothing more).
4. **Modernization and performance** (improve code you are already changing).

---

## Scope and Refactoring

- Do what was asked. A bugfix is a bugfix, not a rewrite.
- You may modernize **within the function or class you are already modifying** (ranges, `std::format`, `std::optional`, RAII, removing raw ownership).
- Anything larger (splitting a class, changing a data layout, touching other files) goes in a **separate commit**. If it is big, stop and propose it instead of doing it.
- Never mix refactors across subsystems with a functional change in one commit.
- Do not add abstractions, hooks, or options for hypothetical future use.
- Before adding a utility, search for an existing one. Extend it rather than duplicate it.

---

## Existing Code Is Not a Template
Much of this codebase was written or rewritten by coding agents, and parts still carry legacy idioms (raw pointer ownership, manual index loops, printf-style formatting, large controllers, scattered mutable state). Neither legacy code nor recent agent-written code is proof that a pattern is good.

- Do not copy a pattern just because a neighboring file uses it. Follow the Hard rules and the idioms in this file.
- Do not mass-fix existing code either; apply the scope rules above.
- Never assume existing code is correct. Read it before relying on it.

---

## Hard Rules (Project Invariants)

These are not suggestions. Breaking them causes bugs.

1. **Never mutate tiles directly from input handlers.** All drawing goes through `DrawOperations`. It handles auto-bordering, doodad variations, multi-tile footprints, and undo.
2. **Every user-visible map change must be undoable.** Capture old state before mutating and route it through `Action` / `Change` / `ActionQueue`. Before writing a new operation, read an existing one under `editor/operations/` and follow how it builds its action.
3. **Keep OTBM compatible.** Maps saved by this editor should still load in the original RME. Do not change the on-disk map format, attribute meanings, or serialization order without asking first.
4. **Never assume a single map.** Obtain the `Editor` via `g_gui.GetCurrentEditor()` (nullable, always check) or constructor injection.
5. **Never hardcode colors, fonts, or spacing in UI.** Use `Theme`.
6. **Use the shared grid rendering.** Item and brush grids use `VirtualItemGrid` / `VirtualBrushGrid` and `rme::ui::grid::*`, so cards look identical everywhere.
7. **GL and NanoVG resources must be RAII-owned.** Use the existing wrappers. Never leave a VAO/VBO/texture or NanoVG context with manual cleanup.
8. **Render hot paths do not allocate per frame or per tile.** No `std::string`, `std::function`, node-based containers, or heap allocation inside per-tile or per-sprite loops.
9. **Put overlays in the right tier.** High-throughput per-tile overlays belong in the GLSL shader passes; the rest use the dedicated overlay drawers. Do not add ad-hoc drawing to the main map drawer.
10. **Respect client versions.** Item, sprite, and material data depend on the loaded client version. Do not hardcode item IDs or assume a version is loaded (`g_version.IsVersionLoaded()`).
11. **Keep the Lua API in sync.** `lua/` exposes map, tile, item, brush, and selection APIs to user scripts. When you change the public behavior of those core types, check the matching `lua/lua_api_*` binding and the scripts under `lua_tests/`. Do not break script-visible behavior silently.
12. **Do not edit vendored code.** `ext/` (pugixml, fast_noise_lite) and generated protobuf output are third-party. Fix problems in our code instead.

---

## Ownership Rules

- `std::unique_ptr<T>` for exclusive ownership. `T*`, `T&`, `std::span`, `std::string_view` for non-owning access.
- **`std::shared_ptr` is not allowed** unless there are multiple independent owners with non-hierarchical lifetimes. State the reason in a comment if you use it.
- No raw `new`/`delete` for owned data.
- **Exception: wxWidgets windows.** Child windows are created with `new` and owned by their parent window. Do **not** wrap them in `unique_ptr` (double free). Top-level windows without a parent follow wx's own lifetime rules (`Destroy()`).
- Use `std::scoped_lock` for locking.

---

## Modern C++ Idioms to Prefer in New or Touched Code

- `std::format` for strings and logs. Convert to `wxString` only at the UI boundary with `wxstr(...)`. Avoid `wxString::Format`.
- `std::optional` instead of sentinel values (`-1`, `0xFFFF`).
- `enum class` instead of plain enums or integer flags.
- `constexpr`/`consteval` for constants, masks, and geometry metrics.
- `std::span<const T>` / `std::string_view` for read-only parameters instead of `const std::vector<T>&` / `const std::string&`.
- Ranges/views where they read more clearly than a loop. In hot paths, prefer a plain loop if profiling shows the view is slower.
- Structured bindings for positions and pairs.

---

## Data-Oriented Design: Where It Applies

Cache locality matters in the render path, the chunk cache, and any code that visits many tiles per frame.

- In those paths, prefer contiguous storage (flat vectors, Struct-of-Arrays) and avoid pointer chasing.
- **Known tension:** `Tile` stores its items, creature, and spawn behind `unique_ptr`s, and undo deep-copies tiles. Do not change `Tile`'s layout as a side effect of another task. If you believe a layout change is justified, propose it with measurements first.
- New per-tile or per-sprite data used for rendering goes in a flat buffer, not in new heap objects hanging off `Tile`.

---

## Performance Claims

- Do not claim something is faster without measurement. Profile before and after on a large map and report the numbers.
- Do not make hot-path code more complicated for a speedup you have not measured.

---

## Git Workflow

- Work on a dedicated branch cut from `master` (`feature/...`, `fix/...`, `refactor/...`, `docs/...`). Creating the branch is the first step of any plan. Never commit to `master`.
- Small, atomic, logically coherent commits.

---

## Dead Code

- Delete unused code, parameters, includes, and debug logging in code you touched. Never comment code out.
- Before finishing, check your own diff for commented-out code, scaffolding, debug output, and symbols that no longer have callers (search for them).
- Do not go hunting for dead code in files you did not touch.

---

## Definition of Done

- The change solves the stated task and nothing else.
- No Hard rule is violated.
- Any map mutation is undoable, and map files remain loadable.
- The diff contains no dead code, debug output, or unrelated edits.
- Performance claims, if any, are backed by numbers.
- You have said plainly what you did **not** verify.

If you could not confirm something (a name, an API signature, runtime behavior), say so rather than guessing.

---

## Orientation

Production C++ source files live exclusively in `source/` (with test sources under `tests/`). Repo-root folders hold game data, assets, and runtime content:
- `brushes/` — XML brush configurations and bordering definitions
- `data/` — materials, item mappings, menubar configs, and version manifests
- `extensions/` — editor extension packages
- `lua_tests/` — integration test scripts for the Lua engine
- `tests/` — C++ GoogleTest/standalone unit test suites
- `tools/` — standalone Python client data inspection tools (`tibia_data.py`)

Do not trust file names from memory. **Find things by searching for the symbol**, then read the code. If a folder has its own `agents.md`, read it too; the nearest one wins for that folder.

### Map of `source/`

```
source/
  app/                  lifecycle, settings, extensions, updater, client asset detection
    managers/             client version loading (g_version)
    preferences/          preferences pages (general, editor, graphics, interface, client version, map compatibility)
  assets/               embedded application UI icons, PNGs, and FontAwesome SVGs
  brushes/              Brush base class, enums, footprint, shared utilities
    border/               optional border brush
    carpet/ table/ wall/  brush + border calculator + item arrays + XML loader each
    doodad/               doodad brush, items, loader
    ground/               ground brush, autoborder, border calculation, terrain placement
    creature/             creature, monster, NPC brushes
    door/ eraser/ flag/ raw/ spawn/ waypoint/ zone/    one brush each
    house/                house brush + house exit brush
    town/                 town temple brush
    managers/             brush manager, autoborder and doodad preview managers
  editor/               Editor, selection (+ selection thread), undo/redo (Action, ActionQueue), copy buffer, dirty list, tabs, hotkeys, MapSession
    managers/             editor manager
    operations/           draw, copy, selection, spawn validation, unreachable cleaning, map version change, collection extraction
    persistence/          save/load coordination, map backup, minimap export
  ext/                  VENDORED (pugixml, fast_noise_lite). Do not edit
  game/                 domain objects (Item, Creature, House, Spawn, Town, Waypoints, Outfit), item attributes, material database/manifest/includes, animation timer
  ingame_preview/       in-game preview: window, canvas, renderer, floor visibility
  io/                   map I/O base, OTBM coordinator, XML I/O, file handles, collection XML
    compression/          gzip streams and node file writing
    otbm/                 header, tile, item, town, waypoint serializers; fast reader
  item_definitions/
    core/                 ItemDefinitionStore, builder, loader, resolver, asset bundle, missing-item report
    formats/              item parser per source format (dat/, otb/, protobuf/, xml/)
  live/                 live collaboration: server, client, peers, sockets, packets, live actions
  lua/                  scripting engine, script manager, dialogs, scripts window
                        lua_api_*: bindings per domain (algo, app, brush, color, creature, geo, http, image, item, json, map, noise, position, selection, tile)
  map/                  Map, BaseMap, Tile, tile operations, regions, spatial hash grid, change tracker, zone index, spawn manager, statistics, search, converter
    operations/           whole-map batch processing (cleaning, version migration)
  net/                  connection and process communication primitives
  palette/              palette window, brush lists, hardcoded page registry, waypoint and zone pages
    all_items/            searchable flat catalog of all client items
    controls/             virtual brush grid, search controls, tileset toolbar
    house/                house palette and tile assignment panel
    managers/             palette manager and collection synchronization
    panels/               brush palette and individual tileset panels
    town/                 town temple assignment palette
  protobuf/             client appearances proto
  rendering/            MapDrawer (render orchestration)
    core/                 GL resources, sprite batch, texture atlas/array, chunk cache, ring buffer, multi-draw-indirect, shader programs,
                          graphics manager, sprite loading/decoding, light buffer/gatherer/cache, drawing options, render view/frame context
    drawers/              draw passes
      tiles/                floors, shading, tile renderer, tile colors
      entities/             creatures, items, sprites, creature names
      overlays/             brush, selection, zone, grid, markers, doors, hooks, preview, Lua overlays, overlay collector
      cursors/              drag shadow, live cursors
                            (plus minimap drawing at this level)
    shaders/              GLSL sources as headers (sprite batch, chunk, house, zone, indicator, pathing, highlight)
    indicators/           technical item registry, zone flags/colors/clusters
    ui/                   MapCanvas (map_display) and its controllers: drawing, selection, navigation, zoom, keyboard, clipboard, popup actions, tooltips, minimap window
    utilities/            LightDrawer, tile describer, icon renderer, FPS counter
    io/                   screenshots and screen capture
  ui/                   GUI controller (gui), main frame, menubar, toolbar, map tab/window, theme, tool options, find item window, welcome/about
    controls/             reusable controls (buttons, sortable lists, collection grids, outfit palette)
    dialogs/              standalone dialogs (collection studio, find, goto position, outfit chooser, spawn validator, missing items)
    icons/                editor icon registry, sprite icon service
    managers/             GL context, layout, loading, minimap, recent files, search, status, vsync, welcome
    map/                  map properties, import, minimap export windows
    menubar/              menu action handlers (file, map, navigation, palette, script, search, view)
    properties/           item/container/creature/depot/podium/spawn/splash property windows, attribute and property services
    replace_tool/         tile replacement: rules, engine, similarity, UI panels (see ALGORITHM_DOCS.md there)
    tile_properties/      browse-tile property panels per entity type
    toolbar/              toolbar factory, registry, layout, persistence, individual toolbars
    welcome/              startup screen cards, lists, formatters
  util/                 common helpers (wxstr), file system, image manager, JSON, NanoVG canvas/listbox/table, virtual item grid, grid cell renderer, profiler, system specs
```

Symbols worth knowing by name (search for them; read before editing): `g_gui`, `g_version`, `g_items`, `g_brushes`, `g_materials`, `g_graphics`, `DrawOperations`, `ActionQueue`, `Action`, `Change`, `Theme`, `VirtualItemGrid`, `VirtualBrushGrid`, `DrawingOptions`, `wxstr`.

---

## Palettes

Tileset-based palette pages are data-driven (built from the palette catalog and `materials.xml`). Some pages are intentionally hardcoded, such as houses, waypoints, zones, and towns. Do not convert one kind into the other unless asked, and do not treat the hardcoded pages as debt to remove.
