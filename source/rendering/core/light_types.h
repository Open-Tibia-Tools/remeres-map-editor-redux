//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_LIGHT_TYPES_H_
#define RME_RENDERING_CORE_LIGHT_TYPES_H_

#include <cstdint>
#include <array>
#include <vector>
#include <glm/glm.hpp>
#include "map/spatial_change_tracker.h"

namespace rme::lighting {

inline constexpr int CHUNK_SIZE = 16;
inline constexpr int CHUNK_PIXELS = CHUNK_SIZE * CHUNK_SIZE; // 256
inline constexpr int MAX_LIGHT_RADIUS_TILES = 12;

struct LightSource {
	int32_t x = 0;         // World tile X (already projected if from another floor)
	int32_t y = 0;         // World tile Y (already projected if from another floor)
	int32_t floor = 7;     // Origin floor
	uint8_t color = 215;   // 0-215 Tibia palette index
	uint8_t intensity = 0; // Light radius in tiles
};

struct CachedLightChunk {
	ChunkCoord coord;
	std::array<uint32_t, CHUNK_PIXELS> pixels {}; // Packed RGBA (0xAA'BB'GG'RR)
	uint64_t last_accessed_frame = 0;
	bool is_valid = false;
	bool is_empty = false;
};

struct LightConfig {
	uint8_t ambient_color = 215;
	uint8_t ambient_intensity = 255; // 0-255
	float minimum_ambient_light = 0.0f; // 0.0-1.0
	bool enabled = true;

	constexpr bool operator==(const LightConfig& other) const noexcept = default;
};

} // namespace rme::lighting

#endif // RME_RENDERING_CORE_LIGHT_TYPES_H_
