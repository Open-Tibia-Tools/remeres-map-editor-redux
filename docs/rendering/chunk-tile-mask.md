# Per-Chunk 16x16 Tile Bitmask (`ChunkTileMask`)

## 1. Pattern & Key Files
- `rendering/core/chunk_tile_mask.h` — `ChunkTileMask` (4×`uint64_t` bitset, `any()`, `mark()`, `test()`, `clear()`, `forEach()`).
- `rendering/core/chunk_cache_manager.{h,cpp}` — `CachedChunk::dynamic_tile_mask` and `CachedChunk::light_tile_mask` populated during `bakeChunk` and consumed in `renderDynamicOverlays`.

## 2. When to Use It
Do:
- Use `ChunkTileMask` in `CachedChunk` whenever a per-frame overlay needs to visit only a sparse subset of a 16×16 chunk's tiles without allocating or iterating all 256 tiles.
- Populate masks unconditionally during `bakeChunk` when toggling the overlay does not re-bake chunks.
Don't:
- Don't duplicate raw `std::array<uint64_t, 4>` bit-twiddling loops in `ChunkCacheManager`; call `ChunkTileMask::forEach`.
- Don't change `ChunkTileMask::kSide` without keeping `ChunkCacheManager::CHUNK_SIZE` matching (`static_assert`).

## 3. Minimal Usage
```cpp
if (chunk.light_tile_mask.any()) {
	chunk.light_tile_mask.forEach([&](int tx, int ty) {
		// Visit marked tile in row-major order with zero heap allocations
	});
}
```

## 4. Invariants & Pitfalls
- `mark`/`test` ignore out-of-range coordinates (`!inBounds(tx, ty)`) safely instead of corrupting neighboring bits.
- Always clear all `ChunkTileMask` fields at the start of `bakeChunk` and in `CachedChunk` move constructor/assignment.
- Keep `forEach` lambdas allocation-free on the render hot path.
