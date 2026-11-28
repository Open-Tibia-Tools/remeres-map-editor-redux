# Missing Ground Detection & Search

## 1. Pattern & Key Files
- `source/map/missing_ground.h` — dependency-free `constexpr` classifier (`RoleFlags`, `MissingGroundReason`, `IsFlatBorder`, `EvaluateMissingGroundWithReason`, `EvaluateMissingGround`, `FormatReason`).
- `source/map/missing_ground_tile.h` — `ClassifyTile(const Tile*)` translating `Tile` + `ItemDefinitionView` flags into compact `RoleFlags` bitmasks.
- `source/rendering/drawers/overlays/zone_overlay_drawer.cpp` — `ZoneOverlayDrawer::drawFloorMissingGround` viewport pass reusing 3 row buffers (`missing_ground_row_prev_/curr_/next_`) and `SpatialHashGrid::CellCursor`.
- `source/editor/operations/search_query.cpp` — parallel `SearchEngine::FindMissingGrounds` + `BuildMissingGroundGroups` per-item/creature/spawn/empty grouping.

## 2. When to Use It
Do:
- Route both live overlay rendering (`View -> Show Missing Ground`) and map search (`Search -> Find Missing Grounds`) through `ClassifyTile` + `EvaluateMissingGroundWithReason` so viewport highlights and search results stay 1:1.
- Keep `missing_ground.h` free of map/editor/UI headers so every rule change stays fast and `constexpr`-testable in Catch2 (`tests/test_missing_ground.cpp`).
Don't:
- Don't inspect `TechnicalItemRegistry` or item IDs in `ClassifyTile`; use `ItemDefinitionView` domain flags (`BlockPathfinder`, `Unpassable`, `BlockMissiles`, `IsWall`, `IsCarpet`, `IsTable`, `Moveable`, `Pickupable`, `FloorChange`).
- Don't allocate per-tile temporaries inside `drawFloorMissingGround`; keep 3 sliding `uint16_t` row buffers.

## 3. Minimal Usage
```cpp
const uint16_t self = MissingGround::ClassifyTile(tile);
if ((self & MissingGround::kHasGround) == 0u) {
    const auto reason = MissingGround::EvaluateMissingGroundWithReason(
        self, n, e, s, w, se, nw, ne, sw);
    if (reason != MissingGround::MissingGroundReason::None) {
        const std::string_view label = MissingGround::FormatReason(reason);
    }
}
```

## 4. Invariants & Pitfalls
- Tiles with `ItemFlag::BlockPathfinder` (such as drop-off tile `459`/`469` or pitfalls/holes) still receive `kHasGround` on themselves but never set `kOpenFloor` for neighbours.
- Unoccluded border-only tiles (`kBorderOnly`) and void-adjacent trim (`kIsTrim` with `voids > 0`) stay quiet unless occluded in 2.5D behind `E`/`S`/`SE` barriers (`OccludedBehindBarrier`).
- `SearchEngine::FindMissingGrounds` must also scan upstream `{W, N, NW}` unallocated tiles next to any `kBarrier` tile so occluded empty voids with zero `Tile*` allocation are still caught.
