#include "rendering/core/atlas_manager.h"
#include "rendering/core/hardware_profile.h"
#include <iostream>
#include <algorithm>
#include <spdlog/spdlog.h>

bool AtlasManager::ensureInitialized() {
	if (atlas_.isValid()) {
		return true;
	}

	const int initial_layers = HardwareProfileManager::get().getActiveBudget().initial_atlas_layers;

	if (!atlas_.initialize(initial_layers)) {
		spdlog::error("AtlasManager: Failed to initialize texture array");
		return false;
	}

	spdlog::info("AtlasManager: Texture array initialized ({}x{}, {} initial layers, active budget)",
		TextureAtlas::ATLAS_SIZE, TextureAtlas::ATLAS_SIZE, initial_layers);

	if (!lut_.isValid()) {
		lut_.initialize(65536);
	}

	// Ensure white pixel exists (ID AtlasRegion::INVALID_SENTINEL)
	std::vector<uint8_t> white_data(TextureAtlas::BASE_SLOT_SIZE * TextureAtlas::BASE_SLOT_SIZE * 4, 255);
	white_pixel_cache_ = addSprite(WHITE_PIXEL_ID, white_data.data(), TextureAtlas::BASE_SLOT_SIZE, TextureAtlas::BASE_SLOT_SIZE);

	if (!white_pixel_cache_) {
		spdlog::error("AtlasManager: Failed to register white pixel sprite");
		return false;
	}

	return true;
}

const AtlasRegion* AtlasManager::addSprite(uint32_t sprite_id, const uint8_t* rgba_data, int pixel_width, int pixel_height) {
	// Fast check via direct lookup for common sprites
	if (sprite_id < DIRECT_LOOKUP_SIZE && direct_lookup_[sprite_id] != nullptr) {
		return direct_lookup_[sprite_id];
	}

	// Check hash map for already-added sprites
	auto it = sprite_regions_.find(sprite_id);
	if (it != sprite_regions_.end()) {
		return it->second;
	}

	if (!rgba_data) {
		spdlog::warn("AtlasManager::addSprite called with null data for sprite {}", sprite_id);
		return nullptr;
	}

	if (!ensureInitialized()) {
		return nullptr;
	}

	// Add to texture array
	auto region = atlas_.addSprite(rgba_data, pixel_width, pixel_height);
	if (!region.has_value()) {
		spdlog::error("AtlasManager: Failed to add sprite {} to texture array", sprite_id);
		return nullptr;
	}

	// Store in stable deque
	region_storage_.push_back(*region);
	AtlasRegion* ptr = &region_storage_.back();

	// Store pointer in hash map
	ptr->debug_sprite_id = sprite_id;
	sprite_regions_.emplace(sprite_id, ptr);

	// Also store in direct lookup for O(1) access
	if (sprite_id < DIRECT_LOOKUP_SIZE) {
		direct_lookup_[sprite_id] = ptr;
	}

	lut_.updateSprite(sprite_id, *ptr);

	return ptr;
}

void AtlasManager::removeSprite(uint32_t sprite_id) {
	if (sprite_id == WHITE_PIXEL_ID) {
		white_pixel_cache_ = nullptr;
	}

	AtlasRegion* region = nullptr;

	if (sprite_id < DIRECT_LOOKUP_SIZE) {
		if (direct_lookup_[sprite_id] != nullptr) {
			region = direct_lookup_[sprite_id];
			direct_lookup_[sprite_id] = nullptr;
			sprite_regions_.erase(sprite_id);
		}
	} else {
		auto it = sprite_regions_.find(sprite_id);
		if (it != sprite_regions_.end()) {
			region = it->second;
			sprite_regions_.erase(it);
		}
	}

	if (region) {
		// 1. Free the slot in the texture array (requires valid UVs)
		atlas_.freeSlot(*region);

		// 2. MARK AS INVALID to trigger Self-Healing in stale GameSprites
		// This prevents "Double Allocation" visual bugs where a stale sprite references
		// this region object after the slot has been reused for a new sprite.
		region->debug_sprite_id = AtlasRegion::INVALID_SENTINEL;
		region->atlas_index = AtlasRegion::INVALID_SENTINEL;

		lut_.invalidateSprite(sprite_id);
	}
}

void AtlasManager::clearMapping(uint32_t sprite_id) {
	if (sprite_id == WHITE_PIXEL_ID) {
		white_pixel_cache_ = nullptr;
	}

	if (sprite_id < DIRECT_LOOKUP_SIZE) {
		direct_lookup_[sprite_id] = nullptr;
	}
	sprite_regions_.erase(sprite_id);
}

const AtlasRegion* AtlasManager::getWhitePixel() const {
	if (white_pixel_cache_) {
		return white_pixel_cache_;
	}
	if (sprite_regions_.count(WHITE_PIXEL_ID)) {
		return sprite_regions_.at(WHITE_PIXEL_ID);
	}
	// Should have been initialized in ensureInitialized()
	return nullptr;
}

bool AtlasManager::hasSprite(uint32_t sprite_id) const {
	if (sprite_id < DIRECT_LOOKUP_SIZE) {
		return direct_lookup_[sprite_id] != nullptr;
	}
	return sprite_regions_.contains(sprite_id);
}

void AtlasManager::bind(uint32_t slot) const {
	atlas_.bind(slot);
}

GLuint AtlasManager::getTextureId() const {
	return atlas_.id();
}

void AtlasManager::clear() {
	atlas_.release();
	lut_.release();
	region_storage_.clear();
	sprite_regions_.clear();
	std::fill(direct_lookup_.begin(), direct_lookup_.end(), nullptr);
	white_pixel_cache_ = nullptr;
	spdlog::info("AtlasManager cleared");
}
