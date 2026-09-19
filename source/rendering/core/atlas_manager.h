#ifndef RME_RENDERING_CORE_ATLAS_MANAGER_H_
#define RME_RENDERING_CORE_ATLAS_MANAGER_H_

#include "rendering/core/texture_atlas.h"
#include "rendering/core/sprite_atlas_lut.h"
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

/**
 * AtlasManager manages sprite registration and provides O(1) lookup.
 *
 * Uses a single TextureAtlas for all sprites.
 * Sprites are stored by sprite_id for fast lookup during rendering.
 *
 * Based on imgui_renderer_example_readonly reference implementation.
 */
class AtlasManager {
public:
	// Max sprite ID for O(1) direct lookup
	static constexpr uint32_t DIRECT_LOOKUP_SIZE = 2000000; // Support 10.x+ sprite counts
	static constexpr uint32_t WHITE_PIXEL_ID = 0xFFFFFFFF;

	AtlasManager() = default;
	~AtlasManager() = default;

	// Non-copyable
	AtlasManager(const AtlasManager&) = delete;
	AtlasManager& operator=(const AtlasManager&) = delete;

	/**
	 * Add a sprite to the atlas.
	 * @param sprite_id Unique sprite ID for later lookup
	 * @param rgba_data width*height*4 bytes of RGBA pixel data
	 * @param pixel_width Width in pixels
	 * @param pixel_height Height in pixels
	 * @return Pointer to the region info, or nullptr on failure
	 */
	const AtlasRegion* addSprite(uint32_t sprite_id, const uint8_t* rgba_data, int pixel_width, int pixel_height);

	/**
	 * Remove a sprite from the atlas, freeing its slot for reuse.
	 * @param sprite_id Sprite ID to remove
	 */
	void removeSprite(uint32_t sprite_id);

	/**
	 * Clear the mapping for a sprite ID WITHOUT freeing the slot.
	 * Used for recovering from stale/colliding map entries safely.
	 */
	void clearMapping(uint32_t sprite_id);

	/**
	 * Get the atlas region for a sprite. O(1) for common sprites.
	 * @param sprite_id Sprite ID
	 * @return Pointer to region, or nullptr if not found
	 */
	inline const AtlasRegion* getRegion(uint32_t sprite_id) const {
		if (sprite_id < DIRECT_LOOKUP_SIZE) {
			return direct_lookup_[sprite_id];
		}
		auto it = sprite_regions_.find(sprite_id);
		return it != sprite_regions_.end() ? it->second : nullptr;
	}

	/**
	 * Check if a sprite has been added.
	 */
	bool hasSprite(uint32_t sprite_id) const;

	/**
	 * Bind the texture array to a slot.
	 */
	void bind(uint32_t slot = 0) const;

	/**
	 * Get a region for a white pixel.
	 */
	const AtlasRegion* getWhitePixel() const;

	/**
	 * Get texture ID.
	 */
	GLuint getTextureId() const;

	/**
	 * Check if atlas is valid.
	 */
	bool isValid() const {
		return atlas_.isValid();
	}

	/**
	 * Get number of active texture layers.
	 */
	int getLayerCount() const noexcept {
		return atlas_.getLayerCount();
	}

	/**
	 * Get number of allocated GPU texture layers.
	 */
	int getAllocatedLayers() const noexcept {
		return atlas_.getAllocatedLayers();
	}

	/**
	 * Get total count of sprites stored in the texture atlas.
	 */
	int getTotalSpriteCount() const noexcept {
		return atlas_.getTotalSpriteCount();
	}

	/**
	 * Get the GPU SpriteAtlasLUT for SSBO sprite coordinate indirection.
	 */
	SpriteAtlasLUT& getLUT() noexcept {
		return lut_;
	}
	const SpriteAtlasLUT& getLUT() const noexcept {
		return lut_;
	}

	/**
	 * Bind the GPU LUT texture buffer to a texture unit.
	 */
	void bindLUT(GLuint texture_unit = SpriteAtlasLUT::TEXTURE_UNIT_INDEX) {
		lut_.bind(texture_unit);
	}

	/**
	 * Clear atlas and mappings.
	 */
	void clear();

	/**
	 * Ensure atlas is initialized.
	 */
	bool ensureInitialized();

private:
	TextureAtlas atlas_;
	SpriteAtlasLUT lut_;

	// Stable storage for AtlasRegions (deque doesn't invalidate pointers)
	std::deque<AtlasRegion> region_storage_;

	// Map from sprite_id to AtlasRegion pointer
	std::unordered_map<uint32_t, AtlasRegion*> sprite_regions_;

	// O(1) direct lookup for sprite IDs < DIRECT_LOOKUP_SIZE
	std::vector<AtlasRegion*> direct_lookup_ { DIRECT_LOOKUP_SIZE, nullptr };

	// Cache for white pixel region to avoid hash map lookup
	const AtlasRegion* white_pixel_cache_ = nullptr;
};

#endif
