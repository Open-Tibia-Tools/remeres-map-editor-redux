//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_TILE_INSTANCE_H_
#define RME_RENDERING_CORE_TILE_INSTANCE_H_

#include <cstdint>

/**
 * TileInstance represents a single sprite instance in a chunk buffer.
 * Decoupled from atlas texture coordinates via sprite_id indirection (SpriteAtlasLUT).
 *
 * 48 bytes per instance (aligned to 16 bytes for GPU std430/std140 alignment).
 */
struct alignas(16) TileInstance {
	float x = 0.0f;          // Byte 0-3: Screen X
	float y = 0.0f;          // Byte 4-7: Screen Y
	float w = 32.0f;         // Byte 8-11: Width
	float h = 32.0f;         // Byte 12-15: Height
	float sprite_id = 0.0f;  // Byte 16-19: float avoids glVertexArrayAttribIFormat driver bugs
	float flags = 0.0f;      // Byte 20-23: float avoids glVertexArrayAttribIFormat driver bugs
	float r = 1.0f;          // Byte 24-27: Red tint [0.0, 1.0]
	float g = 1.0f;          // Byte 28-31: Green tint [0.0, 1.0]
	float b = 1.0f;          // Byte 32-35: Blue tint [0.0, 1.0]
	float a = 1.0f;          // Byte 36-39: Alpha [0.0, 1.0]
	float _pad[2] = {0.0f, 0.0f}; // Byte 40-47: Align to 48 bytes (multiple of 16)
};

static_assert(sizeof(TileInstance) == 48, "TileInstance must be exactly 48 bytes");

#endif
