#include "rendering/core/render_chunk_cache.h"

namespace rme::rendering {

RenderChunk* RenderChunkCache::getChunk(int cx, int cy, int floor) {
	const uint64_t key = makeChunkKey(cx, cy, floor);
	const auto it = chunks_.find(key);
	if (it != chunks_.end()) {
		return it->second.get();
	}
	return nullptr;
}

RenderChunk& RenderChunkCache::getOrCreateChunk(int cx, int cy, int floor) {
	const uint64_t key = makeChunkKey(cx, cy, floor);
	auto& ptr = chunks_[key];
	if (!ptr) {
		ptr = std::make_unique<RenderChunk>();
		ptr->key = key;
		ptr->chunk_x = cx * CHUNK_SIZE_TILES;
		ptr->chunk_y = cy * CHUNK_SIZE_TILES;
		ptr->floor = floor;
		ptr->dirty = true;
	}
	return *ptr;
}

void RenderChunkCache::markDirty(int map_x, int map_y, int floor) {
	const int cx = map_x >> 4;
	const int cy = map_y >> 4;

	auto markChunkIfPresent = [this, floor](int target_cx, int target_cy) {
		const uint64_t key = makeChunkKey(target_cx, target_cy, floor);
		if (const auto it = chunks_.find(key); it != chunks_.end()) {
			it->second->dirty = true;
		}
	};

	markChunkIfPresent(cx, cy);

	// Invalidate adjacent chunks when tiles near boundary change (for multi-tile overhangs)
	const int local_x = map_x & 15;
	const int local_y = map_y & 15;
	if (local_x <= 1) {
		markChunkIfPresent(cx - 1, cy);
	}
	if (local_y <= 1) {
		markChunkIfPresent(cx, cy - 1);
	}
	if (local_x <= 1 && local_y <= 1) {
		markChunkIfPresent(cx - 1, cy - 1);
	}
}

void RenderChunkCache::markAllDirty() {
	for (auto& [key, chunk] : chunks_) {
		if (chunk) {
			chunk->dirty = true;
		}
	}
}

void RenderChunkCache::clear() {
	chunks_.clear();
}

} // namespace rme::rendering
