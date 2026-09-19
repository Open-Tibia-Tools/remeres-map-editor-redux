//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_SPRITE_ATLAS_LUT_H_
#define RME_RENDERING_CORE_SPRITE_ATLAS_LUT_H_

#include <glad/glad.h>
#include <cstdint>
#include <vector>

struct AtlasRegion;

/**
 * 32-byte GPU-aligned lookup entry.
 * When bound as a GL_RGBA32F texture buffer (16 bytes per texel),
 * each entry corresponds to 2 texels:
 *   Texel 2*i + 0: (u_min, v_min, u_max, v_max)
 *   Texel 2*i + 1: (layer, valid, _pad[0], _pad[1])
 */
struct alignas(16) SpriteLUTEntry {
	float u_min = 0.0f;
	float v_min = 0.0f;
	float u_max = 1.0f;
	float v_max = 1.0f;
	float layer = 0.0f;
	float valid = 0.0f;
	float _pad[2] = {0.0f, 0.0f};
};
static_assert(sizeof(SpriteLUTEntry) == 32, "SpriteLUTEntry must be exactly 32 bytes");

/**
 * SpriteAtlasLUT manages a GPU Texture Buffer Object (TBO)
 * providing O(1) sprite UV/layer resolution by sprite_id.
 *
 * Decouples chunk geometry buffers from texture atlas placement,
 * ensuring zero chunk re-baking when background sprites finish loading
 * or when the atlas repacks.
 *
 * Uses GL_TEXTURE_BUFFER / samplerBuffer for universal cross-vendor
 * compatibility (including Intel integrated graphics).
 */
class SpriteAtlasLUT {
public:
	static constexpr GLuint TEXTURE_UNIT_INDEX = 1;
	static constexpr size_t DEFAULT_INITIAL_CAPACITY = 65536;
	static constexpr uint32_t WHITE_PIXEL_LUT_INDEX = 0;
	static constexpr uint32_t MAX_SUPPORTED_SPRITES = 2000000;

	SpriteAtlasLUT();
	~SpriteAtlasLUT();

	// Non-copyable, movable
	SpriteAtlasLUT(const SpriteAtlasLUT&) = delete;
	SpriteAtlasLUT& operator=(const SpriteAtlasLUT&) = delete;
	SpriteAtlasLUT(SpriteAtlasLUT&& other) noexcept;
	SpriteAtlasLUT& operator=(SpriteAtlasLUT&& other) noexcept;

	/**
	 * Initialize the GPU Texture Buffer.
	 * @param initial_capacity Initial number of sprite entries to allocate
	 * @return true if successful
	 */
	bool initialize(size_t initial_capacity = DEFAULT_INITIAL_CAPACITY);

	/**
	 * Register or update the LUT entry for a sprite.
	 */
	void updateSprite(uint32_t sprite_id, const AtlasRegion& region);

	/**
	 * Mark a sprite as invalid/unloaded.
	 */
	void invalidateSprite(uint32_t sprite_id);

	/**
	 * Upload pending changes to GPU buffer.
	 */
	void flush();

	/**
	 * Bind the texture buffer to the designated texture unit (default 1).
	 */
	void bind(GLuint texture_unit = TEXTURE_UNIT_INDEX);

	/**
	 * Unbind the texture buffer from the designated texture unit.
	 */
	void unbind(GLuint texture_unit = TEXTURE_UNIT_INDEX) const;

	/**
	 * Get direct entry on CPU for inspection/tests.
	 */
	const SpriteLUTEntry* getEntry(uint32_t sprite_id) const;

	size_t getCapacity() const noexcept {
		return cpu_entries_.size();
	}

	GLuint getBufferID() const noexcept {
		return buffer_;
	}

	GLuint getTextureID() const noexcept {
		return texture_;
	}

	bool isValid() const noexcept {
		return texture_ != 0 && buffer_ != 0;
	}

	void release();

private:
	void ensureCapacity(size_t required_capacity);

	GLuint buffer_ = 0;
	GLuint texture_ = 0;
	std::vector<SpriteLUTEntry> cpu_entries_;
	size_t gpu_capacity_ = 0;
	uint32_t dirty_min_id_ = UINT32_MAX;
	uint32_t dirty_max_id_ = 0;
	bool has_dirty_entries_ = false;
};

#endif
