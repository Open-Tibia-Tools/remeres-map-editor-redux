#include "rendering/core/image.h"
#include "rendering/core/graphics.h"

Image::Image() :
	isGLLoaded(false),
	lastaccess(0) {
}

void Image::visit() const {
	lastaccess.store(static_cast<int64_t>(g_graphics.getCachedTime()), std::memory_order_relaxed);
}

void Image::clean(time_t time, int longevity) {
	// Base implementation does nothing
}

const AtlasRegion* Image::EnsureAtlasSprite(uint32_t sprite_id, std::unique_ptr<uint8_t[]> preloaded_data, std::optional<ImageDimensions> dimensions) {
	if (g_graphics.ensureAtlasManager()) {
		AtlasManager* atlas_mgr = g_graphics.getAtlasManager();

		// 1. Check if already loaded
		const AtlasRegion* region = atlas_mgr->getRegion(sprite_id);
		if (region) {
			// CRITICAL FIX: Check if the region we found is marked INVALID (from double-allocation fix)
			// or belongs to another sprite (mismatch).
			if (region->debug_sprite_id == AtlasRegion::INVALID_SENTINEL || (region->debug_sprite_id != 0 && region->debug_sprite_id != sprite_id)) {
				spdlog::warn("STALE/INVALID MAP ENTRY DETECTED: Sprite {} maps to region owned by {}. Clearing mapping.", sprite_id, region->debug_sprite_id);
				// SAFETY: Only call clearMapping to avoid freeing shared slots owned by others.
				// removeSprite() is only for explicit destruction.
				atlas_mgr->clearMapping(sprite_id);
				region = nullptr; // Force reload
			} else {
				return region;
			}
		}

		// 2. Load data
		std::unique_ptr<uint8_t[]> rgba;
		// Preloaded atlas data already matches the caller-supplied dimensions, while on-demand loading
		// must refresh dimensions from the image source in case the sprite footprint is variable-sized.
		ImageDimensions image_dimensions = dimensions.value_or(getDimensions());
		if (preloaded_data) {
			rgba = std::move(preloaded_data);
		} else {
			rgba = getRGBAData();
			image_dimensions = getDimensions();
		}

		if (!rgba) {
			if (image_dimensions.width == 0 || image_dimensions.height == 0 || image_dimensions.pixelCount() == 0) {
				image_dimensions = ImageDimensions {};
			}

			// Fallback: Create a magenta texture to distinguish failure from garbage
			constexpr int RGBA_COMPONENTS = 4;
			rgba = std::make_unique<uint8_t[]>(image_dimensions.pixelCount() * RGBA_COMPONENTS);
			std::span<uint8_t> buffer(rgba.get(), image_dimensions.pixelCount() * RGBA_COMPONENTS);
			for (size_t i : std::views::iota(size_t { 0 }, image_dimensions.pixelCount())) {
				buffer[i * RGBA_COMPONENTS + 0] = 255;
				buffer[i * RGBA_COMPONENTS + 1] = 0;
				buffer[i * RGBA_COMPONENTS + 2] = 255;
				buffer[i * RGBA_COMPONENTS + 3] = 255;
			}
			spdlog::warn("getRGBAData returned null for sprite_id={} - using fallback", sprite_id);
		}

		// 3. Add to Atlas
		region = atlas_mgr->addSprite(sprite_id, rgba.get(), image_dimensions.width, image_dimensions.height);

		if (region) {
			if (!isGLLoaded) {
				isGLLoaded = true;
				g_graphics.resident_images.push_back(this); // Add to resident set
			}
			g_graphics.collector.NotifyTextureLoaded();
			return region;
		} else {
			spdlog::warn("Atlas addSprite failed for sprite_id={}", sprite_id);
		}
	} else {
		spdlog::error("AtlasManager not available for sprite_id={}", sprite_id);
	}
	return nullptr;
}
