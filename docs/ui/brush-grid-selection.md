# Brush grid multi-selection

## 1. Pattern & Key Files

- `palette/controls/brush_grid_selection.h` — `BrushGridSelection`, pure (no wx): members, anchor, primary.
- `palette/controls/virtual_brush_grid.{h,cpp}` — maps mouse input to it, draws members, sends events.
- `tests/test_brush_grid_selection.cpp` — Catch2 cases.

## 2. When to Use It

- Use it for any brush grid that needs Ctrl (toggle) / Shift (range) selection.
- Only a single pick (`PressResult::picked`, or `Release()` collapsing a group) may change the current brush. Ctrl/Shift never do.
- Read the group with `GetSelectedBrushes()`; `GetSelectedBrush()` stays the current (primary) brush because `g_gui.SelectBrush()` re-reads it from palettes.
- Don't keep indices: members are brushes, so sort/filter/`ReloadBrushes()` keep them (`Remap`).

## 3. Minimal Usage

```cpp
// CellAt() returns std::nullopt for empty space.
const auto result = m_selection.Press(m_display_brushes, CellAt(x, y), ModifierOf(event));
SyncSelection();
if (result.picked) {
	CommitPick(const_cast<Brush*>(result.picked)); // focuses the map if it already is current
}
// on left-up:
if (const Brush* collapsed = m_selection.Release(drag_started)) { /* single pick */ }
```

## 4. Invariants & Pitfalls

- A plain click on a member of a group keeps the group (for a drag) and collapses on release without a drag.
- Search results (override brushes) jump to their page on release, never on press, so Shift/Ctrl and dragging work there. Their grid passes `Commit::OnRelease` and commits (calling `Reset`) only when the button is released on the pressed cell, so a drag or a release elsewhere keeps the current brush.
- A pick is compared with `g_gui.GetCurrentBrush()`, not with the grid's own selection: another page or window may have changed the brush since. Only a click on the current brush just focuses the map.
- Right-click selects the hit cell only when it is outside the group, and must `Skip()` (wxGTK sends `wxEVT_CONTEXT_MENU` only for unhandled right presses).
- `BEGIN_DRAG` is sent synchronously from `OnMotion`; the drop source loop consumes the release.
- The primary brush stays a member while hidden by a filter, so it is highlighted again when shown.
- Call `SyncSelection()` after every selection or display-list change. It rebuilds `selected_index` and the per-cell flags that the paint loop reads, so painting never searches the member set.
