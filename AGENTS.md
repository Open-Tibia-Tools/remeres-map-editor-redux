# AGENTS.md

**Remere's Map Editor (RME) Redux**: a performance-critical, tile-based map editor for Open Tibia servers built with C++20/C++23, wxWidgets, OpenGL 4.6, and NanoVG. Maps hold millions of tiles, making render and tile-traversal paths performance-critical.

**Your Priority Order (when rules conflict, higher wins):**
1. **Data Integrity & State Consistency** (cause zero silent data loss, guarantee deterministic serialization, enforce valid memory lifetimes, and introduce no Undefined Behavior).
2. **Architectural Purity over Band-Aids** (always refactor flawed foundations blocking your task instead of applying quick workarounds).
3. **Hard Rules: Execution Protocol, Architecture & C++** (follow Sections 1, 2 & 3 unconditionally).
4. **Build, Test & Living Documentation Mandate** (compile cleanly, verify with Catch2/CTest, and document reusable patterns for future sessions).

---

## 1. HARD RULES: Your Execution Protocol, Review Loop & Git Workflow

*Follow this step-by-step instruction manual in every session.*

1. **Follow the Strict Phase Gate (Discuss -> Plan -> Approve -> Code):** Always start by discussing the problem with the user and grounding yourself in the actual source files and `docs/`. Never generate an implementation plan until instructed, and **never write or modify production code until the user explicitly approves your plan** (unless the user explicitly requests a trivial less-than-20-line fast-track fix with zero architectural impact).
2. **Ground Your Code & Write a 4-Part Plan:** Never guess file paths, class layouts, or signatures from memory—search for symbols and read the exact `.h`/`.cpp` files first. Structure every formal Plan into 4 sections: **(1) Code Audit & Root Cause** (what is broken, inefficient, or violates SRP in the inspected files), **(2) Proposed Architecture: Option A (Targeted Refactor) vs. Option B (Deep Subsystem Rewrite)** (so the user chooses the depth), **(3) Unit Testing Strategy** (isolated domain/calculation logic to test in Catch2 vs. manual wxWidgets GUI checks), and **(4) Target & Compatibility Impact** (Windows MSVC build, Linux CI portability, OTBM format, and UI consistency).
3. **Distrust Existing Code & Resolve Root-Cause vs. Incidental Refactors:** Assume previous developers or agents made flawed design choices (raw pointer ownership, manual index loops, `printf`-style formatting, monolithic controllers, scattered mutable state)—existing code is **never** a template. Never apply band-aids or hide behind "out of scope" excuses when a structural flaw **directly blocks** a clean fix: include that root-cause refactor in your Plan (Option A/B). Conversely, if you spot incidental technical debt "by the way" that does not block your task, **only report it in your Code Audit** and do not mix it into your implementation unless the user explicitly selects it.
4. **Run a Relentless Goal-to-Goal Green Loop:** Once your plan is approved, program and iterate autonomously until the build and all tests are green—there is no attempt limit. Inform the user of every significant design decision or obstacle along the way, never silence errors by weakening tests or adding unsafe casts, and only pause to ask the user if a fix would violate a Domain Invariant (Section 2).
5. **Write Complete, Review-Ready Code:** Deliver 100% complete implementations—`// TODO`, `// FIXME`, placeholders, stubbed methods, or truncated code blocks are strictly forbidden. Search for existing utilities to extend before creating duplicates, and never invent speculative abstractions, hooks, or options for hypothetical future use.
6. **Execute the Two-Agent Adversarial Loop (You vs. Read-Only Reviewer Sub-Agent):**
   - **Your Role (Architect & Implementer):** You own all file edits, branch creation, C++/wxWidgets implementation, Catch2 tests, dead-code removal, and `docs/` updates.
   - **Spawn a Read-Only Reviewer Sub-Agent:** Spawn a fresh-context Reviewer Sub-Agent (instructing it never to edit files) at two mandatory gates: **(a) Plan Review Gate** (before showing the Plan to the user, verify it is grounded in real code, respects all Hard Rules, and avoids band-aids) and **(b) Code & DoD Review Gate** (after finishing work, audit `git diff` against the approved Plan and DoD checklist). If the Sub-Agent returns `REJECTED`, fix every finding and re-run the review loop until it returns `APPROVED` (if sub-agent tooling is unavailable, execute this audit as an explicit self-review pass).
7. **Manage Git Branches, Commit Prefixes & Detailed Descriptions:**
   - **Branch First & Wait for Manual Sign-Off:** Always start work by switching to a dedicated branch cut from `master`/`main` (`feature/...`, `fix/...`, `refactor/...`, `perf/...`, `test/...`, `docs/...`, `build/...`). Leave your verified green changes in the working tree for the user's manual testing—**never run `git commit`, merge, or open a PR until the user explicitly commands it**.
   - **Use Prefixes & Extensive Descriptions:** When commanded to commit or create a PR, use Conventional Commit prefixes (`feat(<scope>):`, `fix(<scope>):`, `refactor(<scope>):`, `perf(<scope>):`, `test(<scope>):`, `docs(<scope>):`, `build(<scope>):`) and write a comprehensive multi-paragraph body covering: **(a) Context & Root Cause**, **(b) Architectural Changes (SRP & Decide vs. Apply split)**, and **(c) Automated & Manual Verification**.

---

## 2. HARD RULES: System, Domain & UI Invariants

*Breaking these invariants causes corrupted maps, broken undo history, or rendering regressions.*

1. **Separate Decide vs. Apply (Domain vs. Presentation):** Isolate pure decision logic, algorithms, and calculations ("Decide") into stateless functions or domain classes completely free of wxWidgets dialogs, OpenGL calls, or direct I/O mutations ("Apply"). Functions that calculate *what* should happen must never directly mutate global editor state or trigger GUI updates, making them trivial to unit-test in isolation.
2. **Route All Mutations Through the Command Pattern (Undo/Redo):** Never mutate tiles or map state directly from wxWidgets event/input handlers. Route all drawing through `DrawOperations` (handling auto-bordering, doodad variations, multi-tile footprints) and capture old state via `Action` / `Change` / `ActionQueue` so every user-visible map change is undoable (read `editor/operations/` before writing a new operation).
3. **Preserve OTBM, Public Contracts & Client Versions:** Maps saved here must load in original RME—never change the on-disk OTBM format, attribute meanings, serialization order, or public API contracts without explicit approval. Because item, sprite, and material data depend on the loaded client version, never hardcode item IDs or assume a version is loaded (always check `g_version.IsVersionLoaded()`).
4. **Pass Explicit Context (Multi-Map Safety):** Never assume a single map is open. Obtain the active `Editor` via `g_gui.GetCurrentEditor()` (nullable—always check for `nullptr`) or pass it explicitly via constructor/parameter injection.
5. **Keep Render Hot Paths Zero-Allocation & Use Overlay Tiers:** Never trigger new heap allocations per frame, per tile, or per sprite inside render loops (avoid per-iteration `std::string`, `std::function`, node-based containers, or growing temporaries); **pre-allocated buffers (e.g., `std::vector` with `reserve()` and `clear()`) reused across frames are explicitly encouraged**. Put high-throughput per-tile overlays in GLSL shader passes and other overlays in dedicated overlay drawers—never add ad-hoc drawing to `MapDrawer`.
6. **Maintain Native wxWidgets UX, Theme, Shared Grids & Palette Consistency:** Never hardcode colors, fonts, or spacing in UI (always use `Theme`), reuse shared grid/tileset widgets (`VirtualItemGrid`, `VirtualBrushGrid`, `rme::ui::grid::*`), and match existing keyboard shortcuts, dialog layouts, and UX interaction flows across the entire program. Keep tileset-based palettes data-driven (`materials.xml`), and leave intentionally hardcoded palettes (`houses`, `waypoints`, `zones`, `towns`) hardcoded unless asked.
7. **Never Edit Third-Party Code:** Do not modify vendored files in `ext/` (`pugixml`, `fast_noise_lite`) or generated protobuf outputs; fix integration issues in our own source tree.

---

## 3. HARD RULES: C++ Engineering, SRP, Testing, Dead Code & DoD

*Apply these engineering standards strictly to every file you create, refactor, or touch.*

1. **Enforce Single Responsibility Principle (SRP):** Every class, struct, and function must have one clear responsibility and one reason to change—never bloat monolithic classes (`MapCanvas`, `GUI`, `BrushManager`) with new inline logic; extract focused helpers instead. Keep each branch/commit focused on executing the approved Plan (which includes any root-cause refactors approved by the user, while excluding unapproved "by the way" cleanups in unrelated modules).
2. **Bootstrap & Write Automated Tests (Catch2 + CTest):** Every new feature, refactored subsystem, or bugfix must include automated C++ unit tests in `tests/` (bootstrap **Catch2 v3 + CTest** in CMake if missing or incomplete). When modifying tightly coupled legacy or wxWidgets UI code, extract the underlying calculation/decision logic ("Decide") into an isolated function/class and unit-test that in Catch2 (if a change is 100% visual wxWidgets/GLSL layout with zero extractable logic, document exact manual test steps in your Plan).
3. **Write Pattern-Oriented Living Documentation (`docs/`):** Whenever you create or modify a reusable pattern (e.g., reusable tileset UI components, `Theme` rules, NanoVG/wxWidgets controls, domain services, or new build/test scripts), document it in a focused topic file under `docs/` (keep `AGENTS.md` read-only unless asked). Use a concise 4-part structure (under 50 lines per pattern): **(1) Pattern & Key Files (`.h`/`.cpp`)**, **(2) When to Use It (Do's & Don'ts)**, **(3) Minimal C++ Usage Snippet (5–10 lines)**, and **(4) Invariants & Pitfalls**.
4. **Enforce Strict Ownership & RAII:** Default to stack values, use `std::unique_ptr<T>` for exclusive heap ownership, and use `T*`, `T&`, `std::span`, or `std::string_view` for non-owning access (`std::shared_ptr` is forbidden unless multiple independent owners have non-hierarchical lifetimes—state why in a comment). Wrap all GL/NanoVG resources (VAO/VBO/textures/contexts) in RAII handles; **exception:** create wxWidgets child windows with raw `new` when owned by their parent window (never wrap them in `unique_ptr` to avoid double-free), and call `Destroy()` on parentless top-level wxWidgets windows.
5. **Apply Data-Oriented Design (DoD) & Measure Performance:** In render paths, chunk caches, and bulk tile traversal, use contiguous storage (flat vectors, Struct-of-Arrays) and avoid pointer chasing; place new per-tile/per-sprite render data in flat buffers, never in new heap objects attached to `Tile`. **Known `Tile` tension:** `Tile` stores items, creatures, and spawns behind `unique_ptr`s and undo deep-copies tiles—never alter `Tile`'s layout as a side effect of another task, and never claim a hot-path speedup without before/after profiling numbers on a large map.
6. **Write Modern C++20/23 & Maintain Header Hygiene:** Keep `.h`/`.hpp` files minimal using forward declarations instead of heavy `#include`s, and write cross-platform C++ (exact case-sensitive `#include` paths, `<filesystem>`) so Windows MSVC code never breaks Linux GitHub Actions CI. Prefer `std::format` (converting to `wxString` only at the UI boundary via `wxstr(...)`; avoid `wxString::Format`), `std::optional` over sentinel values (`-1`, `0xFFFF`), `enum class`, `constexpr`/`consteval`, `std::scoped_lock`, structured bindings, and `std::span<const T>` / `std::string_view` for read-only parameters (prefer plain loops over ranges/views in hot paths if profiling shows views are slower, and mark all getters/predicates `const` with zero silent side effects).
7. **Hunt & Eliminate Dead Code (Orphan Hunting Protocol):** Never comment code out—delete unused code, parameters, private fields, redundant `#include`s, and debug logs (`printf`, `std::cout`, `wxLogDebug`) outright. Whenever you refactor or replace a helper, **run a repo-wide symbol search (`grep`) for every old function, method, or type you touched**—if it now has zero callers, delete its declaration and definition immediately, and run `git clang-format` to format strictly the lines you modified.
8. **Complete Your Definition of Done (Mandatory Pre-Sign-Off Checklist):**
   Before reporting to the user that a task is finished, verify every item below and obtain an explicit `APPROVED` verdict from your Reviewer Sub-Agent:
   - [ ] **100% Plan Fulfilled, SRP & Root Cause Resolved:** All approved Plan items implemented with single-responsibility classes/functions, zero band-aids, zero `TODO`/`FIXME`s, and full compliance with all Hard Rules.
   - [ ] **Build Green:** Application (`build-ninja/rme.exe`) and test targets compile cleanly with zero errors.
   - [ ] **Automated Tests Green:** New and existing Catch2 unit tests in `tests/` compile and pass via CTest.
   - [ ] **Undo & Binary Format Safety:** Map mutations are reversible via `ActionQueue`, and OTBM/OTB compatibility is intact.
   - [ ] **Dead Code & Diff Hygiene (`git diff`):** Repo-wide symbol search confirmed zero orphaned symbols; diff has zero commented-out code, zero debug logs, and formatting limited to modified lines (`git clang-format`).
   - [ ] **Living Documentation Updated (`docs/`):** Any new reusable UI/tileset pattern, architectural convention, or build/test script is documented in `docs/` using the 4-part format.
   - [ ] **Reviewer Sub-Agent Approved:** Read-only Reviewer Sub-Agent audited the final `git diff` against the Plan and Hard Rules and returned `APPROVED`.
   - [ ] **Honest Verification Report:** Plainly state what you verified automatically, what you measured (with numbers), and **what you could not verify** (listing what needs manual GUI testing).

---

## 4. Build Workflow, Self-Healing Scripts & Helper Tools

- **Target Platforms:** **Windows x64** (MSVC + Ninja + vcpkg) is the primary local build environment; **Linux** builds run in GitHub Actions CI (always maintain cross-platform C++ portability).
- **Build Execution & Self-Healing Script Mandate:** Run `cmd /c build_ninja.bat` from the repo root to configure MSVC/vcpkg, compile `build-ninja/rme.exe`, and generate `compile_commands.json` (or run `ninja -C build-ninja <target>` for specific targets). **If the existing build script fails, lacks a target (such as Catch2 unit tests), or is insufficient, create a new working build/test script (e.g., `run_tests.bat`) and document it in `docs/` and right here in Section 4 for future sessions.**
- **Run Automated Tests (CTest):**
  - Run all tests: `ctest --test-dir build-ninja --output-on-failure`
  - Run a specific test: `ctest --test-dir build-ninja -R <test_name_regex> --output-on-failure`
- **Format Modified Lines Only:** Run `git clang-format` to format strictly the lines modified in your diff.
- **Optional Diagnostic Scripts (`tools/`):** Standalone Python utilities in `tools/` are **optional helpers** (not required build steps—use them whenever they help your investigation):
  - **Client Data Inspection (`tools/tibia_data.py`):** Zero-dependency Python 3.9+ CLI tool to inspect, dump, and validate `Tibia.dat`, `Tibia.spr`, and `items.otb` (see `tools/README.md` for full JSON/CLI reference):
    - Info & signatures: `python tools/tibia_data.py info --dat <Tibia.dat> --spr <Tibia.spr> --otb <items.otb>`
    - Inspect item: `python tools/tibia_data.py item --client <id> --dat <Tibia.dat>` (or `--server <id> --otb <items.otb>`)
    - Export sprite: `python tools/tibia_data.py sprite <sprite_id> --spr <Tibia.spr> -o sprite.png`
    - Run tool tests: `python tools/test_tibia_data.py`

---

## 5. Codebase Orientation & Living Documentation

Never trust file names from memory—**find things by searching for the symbol first**, check existing guides in `docs/` (run `ls docs/` to see current guides), and read the code. If a subfolder has its own `AGENTS.md` (or `agents.md`), read it too; the nearest one wins for that folder.

- **Key Symbols to Search First:** `g_gui`, `g_version`, `g_items`, `g_brushes`, `g_materials`, `g_graphics`, `DrawOperations`, `ActionQueue`, `Action`, `Change`, `Theme`, `VirtualItemGrid`, `VirtualBrushGrid`, `DrawingOptions`, `wxstr`.
- **Repo-Root Folders:**
  - `source/` — Production C++ source files (core editor, domain, OpenGL/NanoVG rendering, and wxWidgets UI)
  - `tests/` — C++ Catch2 automated unit test suites
  - `docs/` — Living architectural guides and reusable UI/tileset patterns
  - `tools/` — Optional standalone Python client data inspection tools (`tibia_data.py`)
  - `brushes/` — XML brush configurations and bordering definitions
  - `data/` — Materials, item mappings, menubar configs, and version manifests
  - `extensions/` — Editor extension packages

### Map of `source/`
```text
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
      overlays/             brush, selection, zone, grid, markers, doors, hooks, preview, overlay collector
      cursors/              drag shadow, live cursors (plus minimap drawing at this level)
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
    menubar/              menu action handlers (file, map, navigation, palette, search, view)
    properties/           item/container/creature/depot/podium/spawn/splash property windows, attribute and property services
    replace_tool/         tile replacement: rules, engine, similarity, UI panels (see ALGORITHM_DOCS.md there)
    tile_properties/      browse-tile property panels per entity type
    toolbar/              toolbar factory, registry, layout, persistence, individual toolbars
    welcome/              startup screen cards, lists, formatters
  util/                 common helpers (wxstr), file system, image manager, JSON, NanoVG canvas/listbox/table, virtual item grid, grid cell renderer, profiler, system specs
```