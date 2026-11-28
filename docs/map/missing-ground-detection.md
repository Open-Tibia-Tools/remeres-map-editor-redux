# Missing Ground Detection & Search

## 1. Pattern & Key Files
- `source/map/missing_ground.h` — dependency-free `constexpr` classifier (`RoleFlags`, `MissingGroundReason`, `IsFlatBorder`, `EvaluateMissingGroundWithReason`, `EvaluateMissingGround`, `FormatReason`).
- `source/map/missing_ground_tile.{h,cpp}` — single-lookup `ClassifyTile(const Tile*)` and fast `ClassifyNeighborRoles(const Tile*)` for grounded/perimeter neighbors.
- `source/map/missing_ground_chunk.{h,cpp}` — pure `MissingGround::EvaluateChunk` filling 16×16 mask words over a reused 18×18 `RoleFlags` scratch buffer with zero `rendering/` header dependencies.
- `source/rendering/core/missing_ground_overlay_cache.{h,cpp}` — per-chunk `ChunkTileMask` overlay cache observing `SpatialChangeTracker::RegionInvalidationListener` (`LifetimeToken`-guarded, `3×3` halo invalidation, LRU/far-floor prune) without touching `ChunkCacheManager` VBOs.
- `source/rendering/drawers/overlays/zone_overlay_drawer.cpp` & `source/editor/operations/search_query.cpp` — cached overlay rendering and parallel map search sharing the exact same rules.

## 2. When to Use It
Do:
- Route both live overlay rendering (`View -> Show Missing Ground`) and map search (`Search -> Find Missing Grounds`) through `ClassifyTile` / `ClassifyNeighborRoles` + `EvaluateMissingGroundWithReason` so viewport highlights and search results stay 1:1.
- Observe `SpatialChangeTracker` via `addRegionListener` + `LifetimeToken` whenever a neighbor-dependent overlay needs `3×3` cross-chunk invalidation.
Don't:
- Don't inspect item IDs in classifiers or dirty `ChunkCacheManager` VBOs for `SHOW_MISSING_GROUND` toggle changes.
- Don't rescan raw viewport tiles per frame inside `drawFloorMissingGround`; query `MissingGroundOverlayCache` via `visitPopulatedChunks`.

## 3. Minimal Usage
```cpp
zone_overlay_drawer.advanceMissingGroundCacheFrame(view.floor);
zone_overlay_drawer.syncMissingGroundCache(editor.map.getChangeTracker(), editor.map, options.show_missing_ground);

map.visitPopulatedChunks(min_cx, min_cy, max_cx, max_cy, z, [&](int cx, int cy) {
    const auto& chunk = missing_ground_cache_.getOrEvaluateChunk(cx, cy, z, map, w, h);
    if (chunk.has_any) {
        chunk.mask.forEach([&](int tx, int ty) { /* emit quad */ });
    }
});
```

## 4. Invariants & Pitfalls
- Tiles with `ItemFlag::BlockPathfinder` (e.g. `459`/`469`) set `RoleFlags::HasGround` on themselves but never `RoleFlags::OpenFloor` for neighbours.
- Unoccluded XML border-only tiles (`IsBorder` / `IsOptionalBorder` -> `RoleFlags::None`) and void-adjacent trim stay quiet unless occluded in 2.5D behind `E`/`S`/`SE` barriers (`OccludedBehindBarrier`).
- Non-blocking `AlwaysOnBottom` (`topOrder == 1`) items absent from `IsBorder`/`IsCarpet` brushes (such as older carpets in 10.98 `brushes.xml`) receive `RoleFlags::IsBottomClip`: quiet on exterior void edges, flagged (`MaskedByCarpetOrContent`) when enclosed (`not_void_edge`).
- `FindMissingGrounds` must also scan `{W, N, NW}` unallocated tiles next to `RoleFlags::Barrier` tiles so occluded empty voids with zero `Tile*` are caught; chunk evaluation may skip empty 16×16 chunks (`visitPopulatedChunks`) because every coordinate in an empty chunk has `>=2` empty cardinal neighbours (`voids >= 2`).
