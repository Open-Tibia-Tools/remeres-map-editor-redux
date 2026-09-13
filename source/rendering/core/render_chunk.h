#ifndef RME_RENDERING_CORE_RENDER_CHUNK_H_
#define RME_RENDERING_CORE_RENDER_CHUNK_H_

#include "rendering/core/sprite_instance.h"
#include "rendering/core/sprite_light.h"
#include <vector>
#include <cstdint>

namespace rme::rendering {

constexpr int CHUNK_SIZE_TILES = 16; // 16x16 tiles per chunk (4x4 MapNodes)

struct ChunkLightEmitter {
	int tile_x = 0;
	int tile_y = 0;
	SpriteLight light;
};

struct RenderChunk {
	uint64_t key = 0;
	int chunk_x = 0; // World tile coordinate (multiple of 16)
	int chunk_y = 0;
	int floor = 0;
	bool dirty = true;

	// Sequential Sprite Batches (Data-Oriented Design):
	// Separate arrays for ground and items to maximize GPU cache locality
	std::vector<SpriteInstance> ground_sprites;
	std::vector<SpriteInstance> item_sprites;

	// Cached light emitters for fast light collection
	std::vector<ChunkLightEmitter> lights;

	[[nodiscard]] bool empty() const noexcept {
		return ground_sprites.empty() && item_sprites.empty();
	}

	void clear() {
		ground_sprites.clear();
		item_sprites.clear();
		lights.clear();
	}
};

[[nodiscard]] inline uint64_t makeChunkKey(int cx, int cy, int floor) noexcept {
	return (static_cast<uint64_t>(floor & 0x0F) << 48) |
	       (static_cast<uint64_t>(static_cast<uint32_t>(cx) & 0xFFFFFF) << 24) |
	       (static_cast<uint64_t>(static_cast<uint32_t>(cy) & 0xFFFFFF));
}

} // namespace rme::rendering

#endif
