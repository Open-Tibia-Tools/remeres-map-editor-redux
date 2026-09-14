#include "rendering/core/normal_image.h"
#include "rendering/core/game_sprite.h"
#include "app/settings.h"
#include "rendering/core/sprite_archive.h"
#include "rendering/core/graphics.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <ranges>

constexpr int RGB_COMPONENTS = 3;

namespace {
	bool loadRgbaFromArchive(uint32_t id, std::unique_ptr<uint8_t[]>& rgba, ImageDimensions& dimensions) {
		const auto archive = g_graphics.getSpriteArchive();
		return archive && archive->readRGBA(id, g_graphics.hasTransparency(), rgba, dimensions);
	}

	void updateDimensions(NormalImage& image, const ImageDimensions& dimensions) {
		if (image.pixel_width == dimensions.width && image.pixel_height == dimensions.height) {
			return;
		}

		image.pixel_width = dimensions.width;
		image.pixel_height = dimensions.height;
		for (GameSprite* parent : image.parents) {
			if (parent) {
				parent->invalidateMetricCaches();
			}
		}
	}
}

NormalImage::NormalImage() :
	id(0),
	atlas_region(nullptr),
	size(0),
	dump(nullptr) {
}

NormalImage::~NormalImage() {
	// dump auto-deleted
	if (isGLLoaded) {
		if (g_graphics.hasAtlasManager()) {
			g_graphics.getAtlasManager()->removeSprite(id);
		}
	}
}

void NormalImage::addParent(GameSprite* sprite) {
	if (!sprite) {
		return;
	}
	if (!parent) {
		parent = sprite;
	}
	if (std::find(parents.begin(), parents.end(), sprite) == parents.end()) {
		parents.push_back(sprite);
	}
}

void NormalImage::fulfillPreload(std::unique_ptr<uint8_t[]> data) {
	atlas_region = EnsureAtlasSprite(id, std::move(data), getDimensions());
	is_preloading = false;
}

void NormalImage::clean(time_t time, int longevity) {
	// Evict from atlas if expired
	if (longevity == -1) {
		longevity = g_settings.getInteger(Config::TEXTURE_LONGEVITY);
	}
	if (isGLLoaded && time - static_cast<time_t>(lastaccess.load(std::memory_order_relaxed)) > longevity) {
		if (g_graphics.hasAtlasManager()) {
			g_graphics.getAtlasManager()->removeSprite(id);
		}
		for (GameSprite* sprite : parents) {
			if (sprite) {
				sprite->invalidateCache(atlas_region);
			}
		}

		isGLLoaded = false;
		atlas_region = nullptr;
		is_preloading = false;

		// Invalidate any pending preloads for this sprite ID
		generation_id++;

		g_graphics.collector.NotifyTextureUnloaded();
	}

	if (time - static_cast<time_t>(lastaccess.load(std::memory_order_relaxed)) > 5 && !g_settings.getInteger(Config::USE_MEMCACHED_SPRITES)) { // We keep dumps around for 5 seconds.
		dump.reset();
	}
}
std::unique_ptr<uint8_t[]> NormalImage::getRGBData() {
	if (id == 0) {
		const auto dimensions = getDimensions();
		const auto pixels_data_size = static_cast<int>(dimensions.pixelCount() * RGB_COMPONENTS);
		return std::make_unique<uint8_t[]>(pixels_data_size); // Value-initialized (zeroed)
	}

	std::unique_ptr<uint8_t[]> rgba;
	ImageDimensions dimensions;
	if (!loadRgbaFromArchive(id, rgba, dimensions) || !rgba) {
		return nullptr;
	}
	updateDimensions(*this, dimensions);

	const auto pixels_data_size = static_cast<int>(dimensions.pixelCount() * RGB_COMPONENTS);
	auto data = std::make_unique<uint8_t[]>(pixels_data_size);
	for (size_t index = 0; index < dimensions.pixelCount(); ++index) {
		const size_t rgba_offset = index * 4;
		const size_t rgb_offset = index * RGB_COMPONENTS;
		if (rgba[rgba_offset + 3] == 0) {
			data[rgb_offset + 0] = 0xFF;
			data[rgb_offset + 1] = 0x00;
			data[rgb_offset + 2] = 0xFF;
			continue;
		}
		data[rgb_offset + 0] = rgba[rgba_offset + 0];
		data[rgb_offset + 1] = rgba[rgba_offset + 1];
		data[rgb_offset + 2] = rgba[rgba_offset + 2];
	}

	return data;
}

std::unique_ptr<uint8_t[]> NormalImage::getRGBAData() {
	// Robust ID 0 handling
	if (id == 0) {
		const auto dimensions = getDimensions();
		const auto pixels_data_size = static_cast<int>(dimensions.pixelCount() * 4);
		return std::make_unique<uint8_t[]>(pixels_data_size); // Value-initialized (zeroed)
	}

	std::unique_ptr<uint8_t[]> rgba;
	ImageDimensions dimensions;
	if (!loadRgbaFromArchive(id, rgba, dimensions) || !rgba) {
		return nullptr;
	}

	updateDimensions(*this, dimensions);
	return rgba;
}

const AtlasRegion* NormalImage::getAtlasRegion() {
	if (isGLLoaded && atlas_region) {
		// Self-Healing: Check for stale atlas region pointer (e.g. from memory reuse)
		// Force reload if Owner is INVALID or DOES NOT MATCH
		if (atlas_region->debug_sprite_id == AtlasRegion::INVALID_SENTINEL || (atlas_region->debug_sprite_id != 0 && atlas_region->debug_sprite_id != id)) {
			spdlog::warn("STALE ATLAS REGION DETECTED: NormalImage {} held region owned by {}. Force reloading.", id, atlas_region->debug_sprite_id);
			isGLLoaded = false;
			atlas_region = nullptr;
		} else {
			visit();
			return atlas_region;
		}
	}

	if (!isGLLoaded) {
		atlas_region = EnsureAtlasSprite(id);
	}
	visit();
	return atlas_region;
}
