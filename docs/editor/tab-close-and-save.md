# Safe Notebook Tab Closing & Background Save

## 1. Pattern & Key Files
- `editor/tab_close_decision.h`: Stateless `DecideTabCloseAction` bounds-check and close-routing logic (`Ignore`, `AllowNativeClose`, `VetoAndLeaveOpen`, `CloseThroughManager`).
- `editor/editor_tabs.{h,cpp}`: `MapTabbook` resolves tabs dynamically via `dynamic_cast<EditorTab*>(notebook->GetPage(idx))` (no raw pointer side-map) and reads `evt.GetSelection()`.
- `editor/managers/editor_manager.{h,cpp}` & `ui/main_frame.{h,cpp}`: `CloseTab(int idx)`, `ShouldSave(const Editor&)`, `SaveMap(Editor&, MapTab*, ...)`, and `DoQuerySave(Editor&, MapTab*, bool)`.
- `tests/test_tab_close_decision.cpp`: Catch2 unit test coverage.

## 2. When to Use It
Do:
- Read `evt.GetSelection()` on `wxAuiNotebookEvent` (note `evt.GetInt()` stays `0`).
- Pass explicit `Editor&` and `MapTab*` into `DoQuerySave` / `SaveMap` / `CloseTab(idx)` so background tabs can save and close without switching active focus.
Don't:
- Don't read `evt.GetInt()` on `wxEVT_AUINOTEBOOK_*` events or rely on `GetCurrentEditor()` when operating on a specific tab index.

## 3. Minimal Usage
```cpp
const int tab_idx = evt.GetSelection();
const auto decision = DecideTabCloseAction({ tab_idx, GetTabCount(), kind, has_changed, live_connected });
if (decision.action == TabCloseAction::CloseThroughManager) {
	evt.Veto();
	if (!decision.prompt_save || g_gui.root->DoQuerySave(*mapTab->GetEditor(), mapTab, false)) {
		g_editors.CloseTab(GetTabIndex(mapTab->GetWindow()));
	}
}
```

## 4. Invariants & Pitfalls
- **`OnNotebookPageChanged` out-of-bounds `oldSelection`**: Deleting Tab 0 triggers `OnNotebookPageChanged` after page removal; always bounds-check `idx < 0 || idx >= GetTabCount()`.
- **Active-tab-only palette reset**: Only call `g_palettes.RefreshPalettes(nullptr, false)` in `CloseTab` when `idx == GetSelection()`.
