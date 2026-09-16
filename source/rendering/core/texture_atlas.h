#ifndef RME_RENDERING_CORE_TEXTURE_ATLAS_H_
#define RME_RENDERING_CORE_TEXTURE_ATLAS_H_

#include <glad/glad.h>
#include "rendering/core/pixel_buffer_object.h"
#include "rendering/core/gl_resources.h"
#include <optional>
#include <cstdint>
#include <vector>

/**
 * AtlasRegion represents where a sprite is located in the texture atlas.
 * Contains UV coordinates, layer index, and the sprite's pixel footprint.
 */
struct AtlasRegion {
	uint32_t atlas_index = 0; // Layer in texture array
	float u_min = 0.0f; // UV left
	float v_min = 0.0f; // UV top
	float u_max = 1.0f; // UV right
	float v_max = 1.0f; // UV bottom
	uint32_t debug_sprite_id = 0; // DEBUG: Track which sprite ID owns this region
	int pixel_x = 0; // Pre-calculated pixel X in the atlas layer
	int pixel_y = 0; // Pre-calculated pixel Y in the atlas layer
	int pixel_width = 32; // Sprite width in pixels
	int pixel_height = 32; // Sprite height in pixels
	int slot_width = 1; // Width in 32x32 atlas cells
	int slot_height = 1; // Height in 32x32 atlas cells

	static constexpr uint32_t INVALID_SENTINEL = 0xFFFFFFFE;
};

/**
 * TextureAtlas manages a GL_TEXTURE_2D_ARRAY that packs many sprites per layer.
 *
 * Each layer is ATLAS_SIZE x ATLAS_SIZE (4096x4096).
 * Storage is organized in BASE_SLOT_SIZE x BASE_SLOT_SIZE (32x32) cells.
 * Legacy sprites use one cell and protobuf sprites can span multiple cells.
 */
class TextureAtlas {
public:
	static constexpr int ATLAS_SIZE = 4096;
	static constexpr int BASE_SLOT_SIZE = 32;
	static constexpr int MAX_SPRITE_SIZE = 64;
	static constexpr int SLOTS_PER_ROW = ATLAS_SIZE / BASE_SLOT_SIZE; // 128
	static constexpr int SLOTS_PER_LAYER = SLOTS_PER_ROW * SLOTS_PER_ROW; // 16384
	static constexpr int MAX_LAYERS = 64; // 64 * 16384 = 1M+ cells

	TextureAtlas();
	~TextureAtlas();

	// Non-copyable
	TextureAtlas(const TextureAtlas&) = delete;
	TextureAtlas& operator=(const TextureAtlas&) = delete;

	// Moveable
	TextureAtlas(TextureAtlas&& other) noexcept;
	TextureAtlas& operator=(TextureAtlas&& other) noexcept;

	/**
	 * Initialize the texture array.
	 * @param initial_layers Number of layers to pre-allocate (default: 8)
	 * @return true if successful
	 */
	bool initialize(int initial_layers = 8);

	/**
	 * Add a sprite to the atlas.
	 * @param rgba_data Pointer to pixel_width*pixel_height*4 bytes of RGBA pixel data
	 * @param pixel_width Sprite width in pixels
	 * @param pixel_height Sprite height in pixels
	 * @return AtlasRegion with layer and UV coordinates, or nullopt on failure
	 */
	std::optional<AtlasRegion> addSprite(const uint8_t* rgba_data, int pixel_width, int pixel_height);

	/**
	 * Free a sprite slot for reuse.
	 * @param region The region to free.
	 */
	void freeSlot(const AtlasRegion& region);

	/**
	 * Bind the texture array to a texture slot.
	 */
	void bind(uint32_t slot = 0) const;

	/**
	 * Unbind texture from slot.
	 */
	void unbind(uint32_t slot = 0) const;

	/**
	 * Get layer count.
	 */
	int getLayerCount() const {
		return layer_count_;
	}

	/**
	 * Get OpenGL texture ID.
	 */
	GLuint id() const {
		return texture_id_ ? texture_id_->GetID() : 0;
	}

	/**
	 * Check if atlas is valid.
	 */
	bool isValid() const {
		return texture_id_ != nullptr;
	}

	/**
	 * Release resources.
	 */
	void release();

private:
	struct Placement {
		int layer = 0;
		int slot_x = 0;
		int slot_y = 0;
	};

	struct FreeSlot {
		int slot_x = 0;
		int slot_y = 0;
		int layer = 0;
	};

	bool addLayer();
	std::optional<Placement> findPlacement(int slot_width, int slot_height);
	bool isAreaFree(int layer, int slot_x, int slot_y, int slot_width, int slot_height) const;
	void setAreaOccupied(int layer, int slot_x, int slot_y, int slot_width, int slot_height, bool occupied);
	size_t occupancyIndex(int layer, int slot_x, int slot_y) const;
	static bool normalizePixelDimension(int& pixel_width, int& pixel_height);

	// PBO for async uploads
	std::unique_ptr<PixelBufferObject> pbo_;

	std::unique_ptr<GLTextureResource> texture_id_;
	int layer_count_ = 0;
	int allocated_layers_ = 0;
	int total_sprite_count_ = 0;
	size_t small_search_hint_ = 0;
	size_t large_search_hint_ = 0;
	std::vector<uint8_t> occupancy_;
	std::vector<FreeSlot> free_slots_;
};

#endif
