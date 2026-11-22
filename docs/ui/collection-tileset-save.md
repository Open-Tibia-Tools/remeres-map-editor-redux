# Collection "Add to Tileset": save plan and tileset ordering

## 1. Pattern & Key Files

The collection studio separates the pure save decision from XML persistence:

- `editor/operations/collection_save_plan.h` — inclusion modes, `BuildCollectionSavePlan`,
  item/brush ordering, save counts.
- `editor/operations/collection_extractor_service.{h,cpp}` — `BuildSaveSelection` resolves staged
  brushes and their `Brush::getRelatedItems()` coverage into a `CollectionSaveSelection`.
- `io/collection_tileset_xml.h` — `AppendMissingTilesetEntries` (dedupe, including `<creature>`) and
  `SortTilesetEntries` (order the final merged file without losing nodes).
- `brushes/brush.{h,cpp}` — `GetBrushSortID`, the numeric key shared with the palette "Sort by ID".
- `ui/dialogs/collection_options_panel.*` — mode checkboxes, counts summary, save callback.

## 2. When to Use It

Do:
- Add new tileset inclusion options by extending `CollectionInclusionMode` and the plan, not by
  filtering spans in the dialog or XML writer.
- Use `GetBrushSortID` for brush ordering: the file groups by category first, then uses the same
  numeric key as the palette ID sort.
- Keep item ids ascending, deduplicated and non-zero in the plan.
- Keep the covered-item source `Brush::getRelatedItems()`; do not hardcode item ids or brush names.
- Keep long dynamic text (covered counts, explanations) in the summary or tooltip. `wxCheckBox`
  cannot wrap; short counters such as `Include Items (107)` are fine.

Don't:
- Don't mutate `CollectionStagingContent` when building a selection; undo/redo and the grids must
  keep showing the full staging.
- Don't sort with staging/grid order or `unordered_set` iteration order.

## 3. Minimal Usage

```cpp
auto selection = CollectionExtractorService::BuildSaveSelection(staging, mode);
std::string error;
if (!CollectionXmlService::SaveTilesetXml(path, name, selection.itemIds, selection.brushes, append, error)) {
	// report error
}
```

## 4. Invariants & Pitfalls

- Order is category (`Ground`, `Wall`, `Doodad`, `Carpet`, `Table`, `Door`, `Creature`, `House`,
  `Other`), then `GetBrushSortID`, then case-insensitive name, then exact name as a tie-breaker.
- Items are always ascending by id; `<brush>`/`<creature>` nodes precede all `<item>` nodes.
- `SortTilesetEntries` also sorts an appended file. Comments and unknown nodes are kept, moved to
  the top in their original relative order; entries are never dropped. A failed top-level copy
  keeps the original order and fails the save instead of writing an unsorted file. pugixml 1.2
  cannot report allocation failures inside deeply copied child nodes.
- Save counts (`ComputeCollectionSaveCounts`) must match the plan: fully covered items with brushes
  still present is a valid save, not an empty one.
- `GetBrushSortID` mirrors the palette: RAW item id, creature outfit, look id, internal brush id.
  Unknown brushes resolve to id 0 and fall back to the name.
