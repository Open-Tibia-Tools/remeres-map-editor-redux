//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "rendering/core/outfit_colorizer.h"
#include "rendering/core/outfit_colors.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/template_image.h"
#include "rendering/core/sprite_decoder.h"
#include "rendering/core/sprite_layout_calculator.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <span>

GameSprite::GameSprite() :
	id(0),
	height(0),
	width(0),
	layers(0),
	pattern_x(0),
	pattern_y(0),
	pattern_z(0),
	frames(0),
	numsprites(0),
	animator(nullptr),
	draw_height(0),
	drawoffset_x(0),
	drawoffset_y(0),
	minimap_color(0),
	is_simple(false) {
}

GameSprite::~GameSprite() = default;

ImageDimensions GameSprite::GetSize() const {
	return getCompositePixelSize();
}

void GameSprite::invalidateCache(const AtlasRegion* region) {
	if (cached_default_region == region) {
		cached_default_region = nullptr;
		cached_generation_id = 0;
		cached_sprite_id = 0;
	}
}

void GameSprite::invalidateMetricCaches() {
	geometry_cache_dirty = true;
	plain_layout_cache_count_ = 0;
	outfit_layout_cache_count_ = 0;
}

void GameSprite::ColorizeTemplatePixels(uint8_t* dest, const uint8_t* mask, size_t pixelCount, int lookHead, int lookBody, int lookLegs, int lookFeet, bool destHasAlpha) {
	OutfitColorizer::ColorizeTemplatePixels(dest, mask, pixelCount, lookHead, lookBody, lookLegs, lookFeet, destHasAlpha);
}

void GameSprite::clean(time_t time, int longevity) {
	for (auto& iter : instanced_templates) {
		iter->clean(time, longevity);
	}
}

int GameSprite::getDrawHeight() const {
	return draw_height;
}

uint32_t GameSprite::getDebugImageId(size_t index) const {
	if (index < sprite_ids.size()) {
		return sprite_ids[index];
	}
	if (index < spriteList.size() && spriteList[index]->isNormalImage()) {
		return static_cast<const NormalImage*>(spriteList[index])->id;
	}
	return 0;
}

ImageDimensions GameSprite::getCompositePixelSize() const {
	if (geometry_cache_dirty) {
		rebuildGeometryCache();
	}
	return cached_composite_size;
}

void GameSprite::rebuildGeometryCache() const {
	int max_width = std::max<int>(width * SPRITE_PIXELS, SPRITE_PIXELS);
	int max_height = std::max<int>(height * SPRITE_PIXELS, SPRITE_PIXELS);
	uint16_t max_part_width = 0;
	uint16_t max_part_height = 0;

	for (int frame = 0; frame < frames; ++frame) {
		for (int pattern_z_index = 0; pattern_z_index < pattern_z; ++pattern_z_index) {
			for (int pattern_y_index = 0; pattern_y_index < pattern_y; ++pattern_y_index) {
				for (int pattern_x_index = 0; pattern_x_index < pattern_x; ++pattern_x_index) {
					for (int layer = 0; layer < layers; ++layer) {
						for (int part_x = 0; part_x < width; ++part_x) {
							for (int part_y = 0; part_y < height; ++part_y) {
								const size_t index = getIndex(part_x, part_y, layer, pattern_x_index, pattern_y_index, pattern_z_index, frame);
								if (index >= spriteList.size() || !spriteList[index]) {
									continue;
								}

								const auto dimensions = spriteList[index]->getDimensions();
								max_part_width = std::max<uint16_t>(max_part_width, dimensions.width);
								max_part_height = std::max<uint16_t>(max_part_height, dimensions.height);
								const int draw_x = (width - part_x - 1) * SPRITE_PIXELS;
								const int draw_y = (height - part_y - 1) * SPRITE_PIXELS;
								max_width = std::max(max_width, draw_x + static_cast<int>(dimensions.width));
								max_height = std::max(max_height, draw_y + static_cast<int>(dimensions.height));
							}
						}
					}
				}
			}
		}
	}

	cached_composite_size = ImageDimensions {
		static_cast<uint16_t>(max_width),
		static_cast<uint16_t>(max_height)
	};
	cached_draw_offset = std::make_pair(
		static_cast<int>(drawoffset_x) + std::max(0, static_cast<int>(max_part_width) - SPRITE_PIXELS),
		static_cast<int>(drawoffset_y) + std::max(0, static_cast<int>(max_part_height) - SPRITE_PIXELS)
	);
	geometry_cache_dirty = false;
}

uint32_t GameSprite::getSpriteId(int frameIndex, int pattern_x, int pattern_y) const {
	auto idx = getIndex(width, height, 0, pattern_x, pattern_y, 0, frameIndex); // Assuming layer, pattern_z are 0 for this context
	if (idx >= 0 && static_cast<size_t>(idx) < sprite_ids.size()) {
		return sprite_ids[idx];
	}
	if (idx >= 0 && static_cast<size_t>(idx) < spriteList.size() && spriteList[idx]->isNormalImage()) {
		return static_cast<const NormalImage*>(spriteList[idx])->id;
	}
	return 0;
}

uint8_t GameSprite::getMiniMapColor() const {
	return minimap_color;
}

GameSprite::SpriteLayoutMetrics GameSprite::getPlainLayoutMetrics(int subtype, int pattern_x, int pattern_y, int pattern_z, int frame) {
	const PlainLayoutCacheKey key {
		.subtype = subtype,
		.pattern_x = pattern_x,
		.pattern_y = pattern_y,
		.pattern_z = pattern_z,
		.frame = frame,
	};

	for (uint8_t i = 0; i < plain_layout_cache_count_; ++i) {
		if (plain_layout_cache_entries_[i].key == key) {
			if (i > 0) {
				PlainLayoutCacheEntry hit = plain_layout_cache_entries_[i];
				for (uint8_t j = i; j > 0; --j) {
					plain_layout_cache_entries_[j] = plain_layout_cache_entries_[j - 1];
				}
				plain_layout_cache_entries_[0] = hit;
			}
			return plain_layout_cache_entries_[0].metrics;
		}
	}

	SpriteLayoutMetrics metrics = buildPlainLayoutMetrics(key);
	const uint8_t insert_limit = std::min<uint8_t>(plain_layout_cache_count_ + 1, static_cast<uint8_t>(LAYOUT_CACHE_CAPACITY));
	for (uint8_t j = insert_limit - 1; j > 0; --j) {
		plain_layout_cache_entries_[j] = plain_layout_cache_entries_[j - 1];
	}
	plain_layout_cache_entries_[0] = PlainLayoutCacheEntry { .key = key, .metrics = metrics };
	plain_layout_cache_count_ = insert_limit;
	return metrics;
}

GameSprite::SpriteLayoutMetrics GameSprite::buildPlainLayoutMetrics(const PlainLayoutCacheKey& key) const {
	return SpriteLayoutCalculator::BuildPlainLayoutMetrics(*this, key);
}

GameSprite::SpriteLayoutMetrics GameSprite::getOutfitLayoutMetrics(int dir, int addon, int pattern_z, int frame) {
	const OutfitLayoutCacheKey key {
		.dir = dir,
		.addon = addon,
		.pattern_z = pattern_z,
		.frame = frame,
	};

	for (uint8_t i = 0; i < outfit_layout_cache_count_; ++i) {
		if (outfit_layout_cache_entries_[i].key == key) {
			if (i > 0) {
				OutfitLayoutCacheEntry hit = outfit_layout_cache_entries_[i];
				for (uint8_t j = i; j > 0; --j) {
					outfit_layout_cache_entries_[j] = outfit_layout_cache_entries_[j - 1];
				}
				outfit_layout_cache_entries_[0] = hit;
			}
			return outfit_layout_cache_entries_[0].metrics;
		}
	}

	SpriteLayoutMetrics metrics = buildOutfitLayoutMetrics(key);
	const uint8_t insert_limit = std::min<uint8_t>(outfit_layout_cache_count_ + 1, static_cast<uint8_t>(LAYOUT_CACHE_CAPACITY));
	for (uint8_t j = insert_limit - 1; j > 0; --j) {
		outfit_layout_cache_entries_[j] = outfit_layout_cache_entries_[j - 1];
	}
	outfit_layout_cache_entries_[0] = OutfitLayoutCacheEntry { .key = key, .metrics = metrics };
	outfit_layout_cache_count_ = insert_limit;
	return metrics;
}

GameSprite::SpriteLayoutMetrics GameSprite::buildOutfitLayoutMetrics(const OutfitLayoutCacheKey& key) const {
	return SpriteLayoutCalculator::BuildOutfitLayoutMetrics(*this, key);
}

size_t GameSprite::getIndex(int width, int height, int layer, int pattern_x, int pattern_y, int pattern_z, int frame) const {
	if (is_simple) {
		return 0;
	}
	if (this->frames == 0) {
		return 0;
	}
	size_t idx = (this->frames > 1) ? frame % this->frames : 0;
	// Cast operands to size_t to force 64-bit arithmetic and avoid overflow
	idx = idx * static_cast<size_t>(this->pattern_z) + static_cast<size_t>(pattern_z);
	idx = idx * static_cast<size_t>(this->pattern_y) + static_cast<size_t>(pattern_y);
	idx = idx * static_cast<size_t>(this->pattern_x) + static_cast<size_t>(pattern_x);
	idx = idx * static_cast<size_t>(this->layers) + static_cast<size_t>(layer);
	idx = idx * static_cast<size_t>(this->height) + static_cast<size_t>(height);
	idx = idx * static_cast<size_t>(this->width) + static_cast<size_t>(width);
	return idx;
}

const AtlasRegion* GameSprite::getAtlasRegion(int _x, int _y, int _layer, int _count, int _pattern_x, int _pattern_y, int _pattern_z, int _frame) {
	if (numsprites == 0) {
		return nullptr;
	}

	// Optimization for simple static sprites (1x1, 1 frame, etc.)
	// Most ground tiles fall into this category.
	if (_count == -1 && numsprites == 1 && frames == 1 && layers == 1 && width == 1 && height == 1) {
		// Also check default params
		if (_x == 0 && _y == 0 && _layer == 0 && _frame == 0 && _pattern_x == 0 && _pattern_y == 0 && _pattern_z == 0) {
			// Check cache
			// We rely on spriteList[0] being valid for simple sprites
			// shared Sprite Fix: Verify generation ID matches what we cached.
			// Wrong Sprite Fix: Verify sprite ID matches what we cached.
			// Optimization: Check lightweight fields BEFORE calling heavy getAtlasRegion()
			if (cached_default_region && spriteList[0]->isGLLoaded && cached_generation_id == spriteList[0]->generation_id && cached_sprite_id == spriteList[0]->id) {
				return cached_default_region;
			}

			// Cache miss or staleness suspected: Use the getter to ensure self-healing check runs
			const AtlasRegion* valid_region = spriteList[0]->getAtlasRegion();
			if (valid_region && spriteList[0]->isGLLoaded) {
				cached_default_region = valid_region;
				cached_generation_id = spriteList[0]->generation_id;
				cached_sprite_id = spriteList[0]->id;
			} else {
				cached_default_region = nullptr;
				cached_generation_id = 0;
				cached_sprite_id = 0;
			}

			return valid_region;
		}
	}

	uint32_t v;
	if (_count >= 0 && height <= 1 && width <= 1) {
		v = _count;
	} else {
		v = ((((((_frame)*pattern_y + _pattern_y) * pattern_x + _pattern_x) * layers + _layer) * height + _y) * width + _x);
	}
	if (v >= numsprites) {
		if (numsprites == 1) {
			v = 0;
		} else {
			v %= numsprites;
		}
	}

	if (spriteList[v]) {
		return spriteList[v]->getAtlasRegion();
	}
	return nullptr;
}

TemplateImage* GameSprite::getTemplateImage(int sprite_index, const Outfit& outfit) {
	// While this is linear lookup, it is very rare for the list to contain more than 4-8 entries,
	// so it's faster than a hashmap anyways.
	auto it = std::ranges::find_if(instanced_templates, [sprite_index, &outfit](const auto& img) {
		if (img->sprite_index != sprite_index) {
			return false;
		}
		uint32_t lookHash = img->lookHead << 24 | img->lookBody << 16 | img->lookLegs << 8 | img->lookFeet;
		return outfit.getColorHash() == lookHash;
	});

	if (it != instanced_templates.end()) {
		// Move-to-front optimization
		if (it != instanced_templates.begin()) {
			std::iter_swap(it, instanced_templates.begin());
			return instanced_templates.front().get();
		}
		return it->get();
	}

	auto img = std::make_unique<TemplateImage>(this, sprite_index, outfit);
	TemplateImage* ptr = img.get();
	instanced_templates.push_back(std::move(img));
	return ptr;
}

const AtlasRegion* GameSprite::getAtlasRegion(int _x, int _y, int _dir, int _addon, int _pattern_z, const Outfit& _outfit, int _frame) {
	if (numsprites == 0) {
		return nullptr;
	}

	uint32_t v = getIndex(_x, _y, 0, _dir, _addon, _pattern_z, _frame);
	if (v >= numsprites) {
		if (numsprites == 1) {
			v = 0;
		} else {
			v %= numsprites;
		}
	}
	if (layers > 1) { // Template
		TemplateImage* img = getTemplateImage(v, _outfit);
		return img->getAtlasRegion();
	}
	if (spriteList[v]) {
		return spriteList[v]->getAtlasRegion();
	}
	return nullptr;
}

std::unique_ptr<uint8_t[]> GameSprite::Decompress(std::span<const uint8_t> dump, bool use_alpha, int id) {
	return SpriteDecoder::DecodeRle(dump, use_alpha, id);
}
