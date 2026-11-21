# Pattern: Map Version Mismatch Handling & Force Load

## 1. Pattern & Key Files (`.h`/`.cpp`)
- `source/editor/persistence/map_load_decision.h`: Pure stateless domain decision logic isolating version resolution from GUI.
- `source/ui/dialogs/client_mismatch_dialog.h` & `.cpp`: wxWidgets dialog presenting Force Load (active client), Switch Version, or Cancel.
- `source/editor/managers/editor_manager.cpp`: Loading orchestration protecting open editors from premature destruction.
- `tests/test_map_load_decision.cpp`: Isolated Catch2 unit tests.

## 2. When to Use It (Do's & Don'ts)
- **DO** use `DecideMapLoadAction` whenever inspecting an OTBM header before mutating editor state or destroying tabs.
- **DO** verify target client asset paths via `target->hasValidPaths()` before invoking `CloseAllEditors()`.
- **DO** postpone closing empty/unmodified placeholder tabs until loading has officially succeeded.
- **DON'T** mix wxWidgets dialog calls or message boxes into `map_load_decision.h`.
- **DON'T** assume single-map workflows or force client version switches without user consent.

## 3. Minimal C++ Usage Snippet
```cpp
MapLoadDecisionInput input{
    .loaded_client_id = current ? current->getID() : "",
    .resolved_target_id = target ? target->getID() : "",
    .options = load_options,
};
const auto decision = DecideMapLoadAction(input);
if (decision.resolution == MapLoadResolution::PromptMismatch) {
    ClientMismatchDialog dlg(parent, *current, *target, ver.items_major, GetOpenMapCount());
    dlg.ShowModal();
    if (dlg.GetChoice() == ClientMismatchChoice::ForceLoadActive) {
        load_options.force_client_mismatch = true;
    }
}
```

## 4. Invariants & Pitfalls
- **Zero Silent Data Loss**: Never call `CloseAllEditors()` before user confirms switching AND asset paths are verified.
- **Client ID Consistency**: Always match `ClientVersion::getID()` (not user-facing display names) for version identity.
- **Binary Compatibility**: Maps force-loaded with active assets keep their original OTBM header version unless explicitly saved as a different target.
