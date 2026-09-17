//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/light_cache.h"
#include "rendering/core/light_palette.h"
#include "map/basemap.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/core/graphics.h"
#include <algorithm>
#include <cmath>

namespace rme::lighting {

LightCache::LightCache() = default;

LightCache::~LightCache() {
	if (tracked_tracker_ && listener_id_ != 0) {
		tracked_tracker_->removeListener(listener_id_);
		listener_id_ = 0;
		tracked_tracker_ = nullptr;
	}
}

CachedLightChunk& LightCache::getOrBakeChunk(
	int32_t cx, int32_t cy, int32_t z,
	const BaseMap& map,
	int32_t start_floor, int32_t superend_floor,
	const LightConfig& config,
	GraphicManager& gfx,
	uint64_t current_frame
) {
	const ChunkCoord coord{ cx, cy, z };
	auto it = chunks_.find(coord);
	if (it == chunks_.end()) {
		CachedLightChunk new_chunk;
		new_chunk.coord = coord;
		new_chunk.last_accessed_frame = current_frame;
		bakeChunk(new_chunk, map, start_floor, superend_floor, config, gfx);
		auto [inserted_it, _] = chunks_.emplace(coord, std::move(new_chunk));
		return inserted_it->second;
	}

	CachedLightChunk& chunk = it->second;
	chunk.last_accessed_frame = current_frame;
	if (!chunk.is_valid) {
		bakeChunk(chunk, map, start_floor, superend_floor, config, gfx);
	}
	return chunk;
}

bool LightCache::updateDirtyState(SpatialChangeTracker& tracker) {
	if (tracked_tracker_ != &tracker) {
		if (tracked_tracker_ && listener_id_ != 0) {
			tracked_tracker_->removeListener(listener_id_);
		}
		tracked_tracker_ = &tracker;
		listener_id_ = tracker.addListener([this](const SpatialChangeTracker& t) {
			if (t.isAllDirty()) {
				pending_all_dirty_ = true;
			}
			for (const auto& c : t.getDirtyChunks()) {
				pending_dirty_chunks_.insert(c);
			}
		});
	}

	if (tracker.isAllDirty() || pending_all_dirty_) {
		invalidateAll();
		pending_all_dirty_ = false;
		pending_dirty_chunks_.clear();
		return true;
	}

	for (const auto& c : tracker.getDirtyChunks()) {
		pending_dirty_chunks_.insert(c);
	}

	if (pending_dirty_chunks_.empty()) {
		return false;
	}

	bool any_invalidated = false;
	for (const auto& coord : pending_dirty_chunks_) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				for (int z = 0; z <= 15; ++z) {
					auto it = chunks_.find(ChunkCoord{ coord.cx + dx, coord.cy + dy, z });
					if (it != chunks_.end()) {
						it->second.is_valid = false;
						any_invalidated = true;
					}
				}
			}
		}
	}
	pending_dirty_chunks_.clear();
	return any_invalidated;
}

void LightCache::invalidateAll() noexcept {
	for (auto& [coord, chunk] : chunks_) {
		chunk.is_valid = false;
	}
}

void LightCache::invalidateTile(int32_t x, int32_t y, int32_t z) {
	const int32_t cx = x >> 4;
	const int32_t cy = y >> 4;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int f = 0; f <= 15; ++f) {
				auto it = chunks_.find(ChunkCoord{ cx + dx, cy + dy, f });
				if (it != chunks_.end()) {
					it->second.is_valid = false;
				}
			}
		}
	}
}

void LightCache::prune(int current_floor, uint64_t current_frame) {
	for (auto it = chunks_.begin(); it != chunks_.end(); ) {
		const uint64_t age = current_frame - it->second.last_accessed_frame;
		const bool is_far_floor = std::abs(it->first.z - current_floor) > 2;

		if (is_far_floor && age > FAR_FLOOR_FRAME_THRESHOLD) {
			it = chunks_.erase(it);
		} else {
			++it;
		}
	}
}

void LightCache::bakeChunk(
	CachedLightChunk& chunk,
	const BaseMap& map,
	int32_t start_floor, int32_t superend_floor,
	const LightConfig& config,
	GraphicManager& gfx
) {
	const int32_t cx = chunk.coord.cx;
	const int32_t cy = chunk.coord.cy;
	const int32_t z = chunk.coord.z;
	const int32_t chunk_origin_x = cx * CHUNK_SIZE;
	const int32_t chunk_origin_y = cy * CHUNK_SIZE;

	// 1. Fill ground occlusion mask for this chunk on floor z
	// true = solid ground present, blocks light from floors > z
	ground_occlusion_mask_.fill(false);
	for (int ty = 0; ty < CHUNK_SIZE; ++ty) {
		for (int tx = 0; tx < CHUNK_SIZE; ++tx) {
			const Tile* tile = map.getTile(chunk_origin_x + tx, chunk_origin_y + ty, z);
			if (tile && tile->ground && tile->ground->blocksLightFromBelow()) {
				ground_occlusion_mask_[ty * CHUNK_SIZE + tx] = true;
			}
		}
	}

	// 2. Fill initial ambient light
	const glm::vec3 ambient = getAmbientRGB(z, config);
	const uint8_t amb_r = static_cast<uint8_t>(std::clamp(std::lround(ambient.r * 255.0f), 0l, 255l));
	const uint8_t amb_g = static_cast<uint8_t>(std::clamp(std::lround(ambient.g * 255.0f), 0l, 255l));
	const uint8_t amb_b = static_cast<uint8_t>(std::clamp(std::lround(ambient.b * 255.0f), 0l, 255l));
	const uint32_t ambient_packed = packRGBA(amb_r, amb_g, amb_b, 255);
	chunk.pixels.fill(ambient_packed);

	// 3. Gather lights affecting this chunk
	gatherer_.gatherForChunk(map, cx, cy, z, start_floor, superend_floor, gfx);
	const auto& lights = gatherer_.getLights();
	if (lights.empty()) {
		chunk.is_valid = true;
		return;
	}

	// 4. Bake lights with LUT distance and ground occlusion
	for (const auto& light : lights) {
		const int radius = light.intensity;
		const int min_tx = std::max(0, light.x - radius - chunk_origin_x);
		const int max_tx = std::min(CHUNK_SIZE - 1, light.x + radius - chunk_origin_x);
		const int min_ty = std::max(0, light.y - radius - chunk_origin_y);
		const int max_ty = std::min(CHUNK_SIZE - 1, light.y + radius - chunk_origin_y);

		if (min_tx > max_tx || min_ty > max_ty) {
			continue;
		}

		const auto& light_rgb = s_palette_table[light.color];
		const int lr = light_rgb.r;
		const int lg = light_rgb.g;
		const int lb = light_rgb.b;

		const int radius_sq = radius * radius;
		const float intensity_f = static_cast<float>(light.intensity);
		const bool is_light_from_below = (light.floor > z);

		for (int ty = min_ty; ty <= max_ty; ++ty) {
			const int dy = (chunk_origin_y + ty) - light.y;
			const int dy2 = dy * dy;
			if (dy2 > radius_sq) continue;

			const int row_idx = ty * CHUNK_SIZE;

			for (int tx = min_tx; tx <= max_tx; ++tx) {
				const int pixel_idx = row_idx + tx;

				// Ground occlusion check: lights from below cannot penetrate solid ground
				if (is_light_from_below && ground_occlusion_mask_[pixel_idx]) {
					continue;
				}

				const int dx = (chunk_origin_x + tx) - light.x;
				const int dist_sq = dx * dx + dy2;
				if (dist_sq > radius_sq) continue;

				// Fast distance via constexpr LUT
				const float dist = (dist_sq <= 512) ? s_distance_table[dist_sq] : std::sqrt(static_cast<float>(dist_sq));
				float factor = (-dist + intensity_f) * 0.2f;
				if (factor < 0.01f) continue;
				factor = std::min(factor, 1.0f);

				const int factor_256 = static_cast<int>(factor * 256.0f + 0.5f);
				const uint8_t light_r = static_cast<uint8_t>((lr * factor_256) >> 8);
				const uint8_t light_g = static_cast<uint8_t>((lg * factor_256) >> 8);
				const uint8_t light_b = static_cast<uint8_t>((lb * factor_256) >> 8);

				uint32_t& px = chunk.pixels[pixel_idx];
				const uint8_t cur_r = static_cast<uint8_t>(px & 0xFF);
				const uint8_t cur_g = static_cast<uint8_t>((px >> 8) & 0xFF);
				const uint8_t cur_b = static_cast<uint8_t>((px >> 16) & 0xFF);

				px = packRGBA(
					std::max(cur_r, light_r),
					std::max(cur_g, light_g),
					std::max(cur_b, light_b),
					255
				);
			}
		}
	}

	chunk.is_valid = true;
}

} // namespace rme::lighting
