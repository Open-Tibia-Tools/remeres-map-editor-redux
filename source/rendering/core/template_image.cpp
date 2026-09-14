#include "rendering/core/template_image.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/outfit_colors.h"
#include "app/settings.h"
#include "rendering/core/graphics.h"
#include <atomic>
#include <spdlog/spdlog.h>

static std::atomic<uint32_t> template_id_generator(0x1000000);

TemplateImage::TemplateImage(GameSprite* parent, int v, const Outfit& outfit) :
	atlas_region(nullptr),
	texture_id(template_id_generator.fetch_add(1)), // Generate unique ID for Atlas
	parent(parent),
	sprite_index(v),
	lookHead(outfit.lookHead),
	lookBody(outfit.lookBody),
	lookLegs(outfit.lookLegs),
	lookFeet(outfit.lookFeet) {
}

TemplateImage::~TemplateImage() {
	if (isGLLoaded) {
		if (g_graphics.hasAtlasManager()) {
			g_graphics.getAtlasManager()->removeSprite(texture_id);
		}
	}
}

void TemplateImage::clean(time_t time, int longevity) {
	// Evict from atlas if expired
	if (longevity == -1) {
		longevity = g_settings.getInteger(Config::TEXTURE_LONGEVITY);
	}
	if (isGLLoaded && time - static_cast<time_t>(lastaccess.load(std::memory_order_relaxed)) > longevity) {
		if (g_graphics.hasAtlasManager()) {
			g_graphics.getAtlasManager()->removeSprite(texture_id);
		}
		isGLLoaded = false;
		atlas_region = nullptr;
		generation_id++;
		g_graphics.collector.NotifyTextureUnloaded();
	}
}

namespace {
	ImageDimensions resolveTemplateDimensions(const TemplateImage* img) {
		if (!img || !img->parent || img->sprite_index < 0 || static_cast<size_t>(img->sprite_index) >= img->parent->spriteList.size()) {
			return {};
		}
		if (const auto* image = img->parent->spriteList[img->sprite_index]) {
			return image->getDimensions();
		}
		return {};
	}

	bool validateTemplateParentAndIndices(const TemplateImage* img, int sprite_index, size_t& mask_index) {
		if (!img->parent) {
			spdlog::warn("TemplateImage (texture_id={}): Invalid parent reference.", img->texture_id);
			return false;
		}

		if (img->parent->width <= 0 || img->parent->height <= 0) {
			spdlog::warn("TemplateImage (texture_id={}): Invalid parent dimensions ({}x{})", img->texture_id, img->parent->width, img->parent->height);
			return false;
		}

		if (sprite_index < 0) {
			spdlog::warn("TemplateImage (texture_id={}): Sprite index is negative (sprite_index={})", img->texture_id, sprite_index);
			return false;
		}

		mask_index = static_cast<size_t>(sprite_index) + static_cast<size_t>(img->parent->height * img->parent->width);
		if (static_cast<size_t>(sprite_index) >= img->parent->spriteList.size() || mask_index >= img->parent->spriteList.size()) {
			spdlog::warn("TemplateImage (texture_id={}): Access index out of bounds (base_index={}, mask_index={}, list_size={})", img->texture_id, sprite_index, mask_index, img->parent->spriteList.size());
			return false;
		}

		return true;
	}

	void clampTemplateLookValues(TemplateImage* img) {
		if (img->lookHead > TemplateOutfitLookupTableSize) {
			img->lookHead = 0;
		}
		if (img->lookBody > TemplateOutfitLookupTableSize) {
			img->lookBody = 0;
		}
		if (img->lookLegs > TemplateOutfitLookupTableSize) {
			img->lookLegs = 0;
		}
		if (img->lookFeet > TemplateOutfitLookupTableSize) {
			img->lookFeet = 0;
		}
	}
} // namespace

ImageDimensions TemplateImage::getDimensions() const {
	return resolveTemplateDimensions(this);
}

std::unique_ptr<uint8_t[]> TemplateImage::getRGBData() {
	size_t mask_index = 0;
	if (!validateTemplateParentAndIndices(this, sprite_index, mask_index)) {
		return nullptr;
	}

	auto rgbdata = parent->spriteList[sprite_index]->getRGBData();
	auto template_rgbdata = parent->spriteList[mask_index]->getRGBData();
	const ImageDimensions base_dimensions = parent->spriteList[sprite_index]->getDimensions();
	const ImageDimensions mask_dimensions = parent->spriteList[mask_index]->getDimensions();

	if (!rgbdata) {
		return nullptr;
	}
	if (!template_rgbdata) {
		return nullptr;
	}
	if (base_dimensions.width != mask_dimensions.width || base_dimensions.height != mask_dimensions.height || base_dimensions.pixelCount() != mask_dimensions.pixelCount()) {
		spdlog::warn(
			"TemplateImage (texture_id={}): Base/mask dimension mismatch (base={}x{}, mask={}x{})",
			texture_id,
			base_dimensions.width,
			base_dimensions.height,
			mask_dimensions.width,
			mask_dimensions.height
		);
		return nullptr;
	}

	clampTemplateLookValues(this);

	GameSprite::ColorizeTemplatePixels(rgbdata.get(), template_rgbdata.get(), base_dimensions.pixelCount(), lookHead, lookBody, lookLegs, lookFeet, false);

	return rgbdata;
}

std::unique_ptr<uint8_t[]> TemplateImage::getRGBAData() {
	size_t mask_index = 0;
	if (!validateTemplateParentAndIndices(this, sprite_index, mask_index)) {
		return nullptr;
	}

	auto rgbadata = parent->spriteList[sprite_index]->getRGBAData();
	auto template_rgbdata = parent->spriteList[mask_index]->getRGBData();
	const ImageDimensions base_dimensions = parent->spriteList[sprite_index]->getDimensions();
	const ImageDimensions mask_dimensions = parent->spriteList[mask_index]->getDimensions();

	if (!rgbadata) {
		spdlog::warn("TemplateImage: Failed to load BASE sprite data for sprite_index={} (template_id={}). Parent width={}, height={}", sprite_index, texture_id, parent->width, parent->height);
		return nullptr;
	}
	if (!template_rgbdata) {
		spdlog::warn("TemplateImage: Failed to load MASK sprite data for sprite_index={} (template_id={}) (mask_index={})", sprite_index, texture_id, mask_index);
		return nullptr;
	}
	if (base_dimensions.width != mask_dimensions.width || base_dimensions.height != mask_dimensions.height || base_dimensions.pixelCount() != mask_dimensions.pixelCount()) {
		spdlog::warn(
			"TemplateImage (texture_id={}): Base/mask dimension mismatch (base={}x{}, mask={}x{})",
			texture_id,
			base_dimensions.width,
			base_dimensions.height,
			mask_dimensions.width,
			mask_dimensions.height
		);
		return nullptr;
	}

	clampTemplateLookValues(this);

	// Note: the base data is RGBA (4 channels) while the mask data is RGB (3 channels).
	GameSprite::ColorizeTemplatePixels(rgbadata.get(), template_rgbdata.get(), base_dimensions.pixelCount(), lookHead, lookBody, lookLegs, lookFeet, true);

	return rgbadata;
}

const AtlasRegion* TemplateImage::getAtlasRegion() {
	if (isGLLoaded && atlas_region) {
		// Self-Healing: Check for stale atlas region pointer
		if (atlas_region->debug_sprite_id == AtlasRegion::INVALID_SENTINEL || (atlas_region->debug_sprite_id != 0 && atlas_region->debug_sprite_id != texture_id)) {
			spdlog::warn("STALE ATLAS REGION DETECTED: TemplateImage {} held region owned by {}. Force reloading.", texture_id, atlas_region->debug_sprite_id);
			isGLLoaded = false;
			atlas_region = nullptr;
		} else {
			visit();
			return atlas_region;
		}
	}

	if (!isGLLoaded) {
		atlas_region = EnsureAtlasSprite(texture_id);
	}
	visit();
	return atlas_region;
}
