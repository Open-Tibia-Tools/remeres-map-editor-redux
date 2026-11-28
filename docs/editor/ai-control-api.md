# Local AI Control API

## 1. Pattern & Key Files

- `source/ai/ai_control_contract.{h,cpp}`: pure Decide layer. HTTP framing (`ParseHttpRequest`), request policy, route table (`Routes`, `LookupRoute`, `CapabilitiesJson`), busy gate (`CheckBusyGate`) and every request parser (`ParsePaintRequest`, `ParsePatchRequest`, `ParseBatchRequest`, ...). No wx, map or GUI access.
- `source/ai/ai_control_http_server.{h,cpp}`: `HttpServer`, a wxSocket server on 127.0.0.1. Socket events run on the UI thread; `Respond` applies policy, route, gate and dispatch inside one try/catch.
- `source/ai/ai_control_service.{h,cpp}`: `Dispatch` runs a gated route. Handlers take `Editor&` explicitly; mutations use `BrushFillOperations` (DrawOperations) or one `ActionQueue` batch.
- `source/editor/operations/brush_fill_operations.{h,cpp}`: draws an explicit brush on explicit positions like the editor's fills, through `DrawOperations::drawWithBrush` / `drawDoodadsWithBrush`; `borderTargets` is the pure patch + ring set.
- `source/ai/ai_control_controller.{h,cpp}`: owner held by `MainFrame` (`GetAiControl()`); menu item `Editor > Local AI Control API` and `RME_AI_CONTROL_AUTOSTART=1` start it.
- `ScreenshotController::ExportRegion`: tiled 100% zoom capture into one PNG. Tests: `tests/test_ai_control_contract.cpp`.

## 2. When to Use It (Do's & Don'ts)

- Do add a route as one `RouteTable` entry plus one `DispatchForEditor` case; capabilities are generated from the table.
- Do parse and validate the body in the contract (reject unknown fields) and unit-test it there.
- Do wrap every mutation in `HistoryIsolation` so it never merges with a user action.
- Do pass the brush and automagic explicitly (`DrawOperations::drawWithBrush`); never select a brush or change tool settings to make a change.
- Don't touch tiles directly in a handler: stage copies and commit them through `Change`/`ActionQueue`, or draw through `DrawOperations`.
- Don't block or yield in a handler (no progress dialogs, no `wxYield`); the server answers 503 to re-entrant requests.
- Don't add options that are silently ignored: an unknown field is a 400.

## 3. Minimal C++ Usage Snippet

```cpp
const RouteMatch match = LookupRoute(request.method, request.path);
if (auto error = CheckBusyGate(*match.route, activity)) {
	return ErrorResponse(*error); // 503 while loading, modal dialog, request in flight, user gesture
}
const JsonBodyResult body = ParseJsonBody(request.body);
return Dispatch(*match.route, body.body); // handlers get Editor& from g_gui.GetCurrentEditor()
```

## 4. Invariants & Pitfalls

- Binds 127.0.0.1 only. Rejects any `Origin` header, a `Host` other than `127.0.0.1:<port>`/`localhost:<port>`, POST without `Content-Type: application/json`, headers over 64 KiB, bodies over 1 MiB, JSON nested deeper than 64, methods other than GET/POST. Always `Connection: close`.
- Sockets use `wxSOCKET_NOWAIT`: reads and writes never block or yield. A response that does not fit the socket buffer continues on `wxSOCKET_OUTPUT`; after the last byte the server half-closes with a plain `shutdown(SD_SEND/SHUT_WR)` (wx's `ShutdownOutput()` closes the whole socket) and drains the client's leftover input until it closes, so the response is not lost to a reset. Deadlines (`PhaseTimeoutSeconds`): 10 s of silence while reading, 5 s in total to write, 2 s to drain; draining connections do not count toward the 16-connection limit.
- Mutations and `export_region` answer 503 while the user draws, drags, box-selects, pans, holds a paste or any window has mouse capture (`IsUserInteracting`).
- `paint` never touches the user's brush, brush size, automagic, editor mode or selection: the brush and automagic go straight to DrawOperations, and one paint is one undo step (doodads included, one composite per tile, using the doodad brush's own variation unless doodads are randomized). Doors still follow the editor's locked-door toggle, reported as `tools.doorsLocked` in `/snapshot`. The response's `changed` is false when the brush placed nothing (a doodad needs ground under it, as in the editor).
- `/undo` and `/redo` behave like the editor's Undo/Redo, including switching to selection mode when they leave a selection.
- A Live client is read-only (403); undo/redo require the batch's editor to still be the current one.
- `batch` runs `PrevalidateBatch` first: the fields of every paint, clear_area, apply_patch, undo, redo and export_region op are checked before anything runs. Brush names, preview tokens and read ops are only checked when the op runs.
- `batch` rollback undoes exactly the entries the batch pushed, counted with `ActionQueue::getPushedCount()` (indexes shift when full history drops old entries). `rolledBack` is false if an entry merged or was dropped, or if an `undo`/`redo` op ran in the batch, in which case nothing is undone.
- `export_region` needs a visible view; it changes floor, zoom and scroll during the capture and restores them together with the minimap viewport. It blocks the UI thread for one redraw per viewport-sized tile, so a 16 Mpx region takes seconds. Only local absolute paths (no UNC or `\\?\` device paths); an existing file is replaced only with `overwrite: true` (else 409 `file_exists`).
- `RME_AI_CONTROL_AUTOSTART=1` never opens a dialog: a bad port or a busy port is logged and shown in the status bar.

## Endpoint Reference

All routes also answer under `/ai/<route>`. Errors are `{"ok": false, "error": "<code>", "message": "..."}`.

| Method | Path | Body | Notes |
|---|---|---|---|
| GET | `/health` | none | Always answers; `documentOpen`, `openMaps`. |
| GET | `/capabilities` | none | Always answers; route table, limits, rules. |
| GET | `/snapshot` | none | Map info, selection (first 4096), tool state, brush catalog. |
| POST | `/brushes` | `{q?, type?, limit?}` | Brush search; `type` is a brush type or `other`. |
| POST | `/inspect_tile` | `{x, y, z}` | All items of one tile, flags, house and zone ids. |
| POST | `/inspect_area` | `{x, y, z, width, height, sampleLimit?}` | Up to 20000 tiles; histogram and samples. |
| POST | `/terrain_grid` | `{x, y, z, width, height}` | Up to 50000 tiles; ground rows and legend. |
| POST | `/paint` | `{brush, type?, z, area?, tiles?, automagic?}` | Ground, wall, wall_decoration, door, doodad, carpet, table, raw; up to 20000 tiles. |
| POST | `/clear_area` | `{x, y, z, width, height, keepGround?, keepFlags?, label?}` | Items, creatures, spawns, house ids, zones and flags; one undo step. |
| POST | `/preview_patch` | `{label?, tiles}` | Before/after and `previewToken`; changes nothing. |
| POST | `/apply_patch` | `{label?, previewToken, tiles}` | Same tiles as the preview; stale tokens fail. No auto-borders. |
| POST | `/undo`, `/redo` | `{steps?}` | 1 to 1000 steps. |
| GET | `/history` | none | Undo index, size and the last 100 labels. |
| POST | `/batch` | `{ops, rollbackOnError?}` | Up to 200 ops `{op: "<route>", ...body}`; no nested batch; map-changing ops are validated up front. |
| POST | `/export_region` | `{minX, minY, maxX, maxY, z, path, overwrite?}` | Absolute local `.png` path; at most 16384 px per side and 16 Mpx. |

The port is 17310 unless `RME_AI_CONTROL_PORT` (1024 to 65535) is set.
