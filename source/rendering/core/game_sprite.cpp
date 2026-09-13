//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "rendering/utilities/sprite_icon_generator.h"
#include "rendering/core/outfit_colorizer.h"
#include "rendering/core/outfit_colors.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/template_image.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <span>

constexpr int RGB_COMPONENTS = 3;
constexpr int RGBA_COMPONENTS = 4;

namespace {
	constexpr size_t LAYOUT_CACHE_ENTRY_LIMIT = 8;

	size_t resolvePlainSpriteIndex(const GameSprite& sprite, int x, int y, int layer, int subtype, int pattern_x, int pattern_y, int pattern_z, int frame) {
		if (sprite.numsprites == 0) {
			return 0;
		}

		size_t index = 0;
		if (subtype >= 0 && sprite.height <= 1 && sprite.width <= 1) {
			index = static_cast<size_t>(subtype);
		} else {
			index = sprite.getIndex(x, y, layer, pattern_x, pattern_y, pattern_z, frame);
		}

		if (index >= sprite.numsprites) {
			index = sprite.numsprites == 1 ? 0 : index % sprite.numsprites;
		}

		return index;
	}

	size_t resolveOutfitSpriteIndex(const GameSprite& sprite, int x, int y, int dir, int addon, int pattern_z, int frame) {
		if (sprite.numsprites == 0) {
			return 0;
		}

		size_t index = sprite.getIndex(x, y, 0, dir, addon, pattern_z, frame);
		if (index >= sprite.numsprites) {
			index = sprite.numsprites == 1 ? 0 : index % sprite.numsprites;
		}
		return index;
	}

	void finalizeLayoutMetrics(GameSprite::SpriteLayoutMetrics& metrics) {
		if (metrics.num_columns > 0) {
			metrics.total_width = 0;
			for (size_t i = 0; i < metrics.num_columns; ++i) {
				metrics.total_width += metrics.column_widths[i];
			}
			metrics.left_offset = metrics.total_width - metrics.column_widths[metrics.num_columns - 1];
		}
		if (metrics.num_rows > 0) {
			metrics.total_height = 0;
			for (size_t i = 0; i < metrics.num_rows; ++i) {
				metrics.total_height += metrics.row_heights[i];
			}
			metrics.top_offset = metrics.total_height - metrics.row_heights[metrics.num_rows - 1];
		}
	}
}

CreatureSprite::CreatureSprite(GameSprite* parent, const Outfit& outfit) :
	parent(parent),
	outfit(outfit) {
}

CreatureSprite::~CreatureSprite() {
}

void CreatureSprite::DrawTo(wxDC* dc, SpriteSize sz, int start_x, int start_y, int width, int height) {
	if (parent) {
		parent->DrawTo(dc, sz, outfit, start_x, start_y, width, height);
	}
}

void CreatureSprite::unloadDC() {
	if (parent) {
		GameSprite::RenderKey key;
		key.colorHash = outfit.getColorHash();
		key.mountColorHash = outfit.getMountColorHash();
		key.lookMount = outfit.lookMount;
		key.lookAddon = outfit.lookAddon;
		key.lookMountHead = outfit.lookMountHead;
		key.lookMountBody = outfit.lookMountBody;
		key.lookMountLegs = outfit.lookMountLegs;
		key.lookMountFeet = outfit.lookMountFeet;

		key.size = SPRITE_SIZE_16x16;
		parent->colored_dc.erase(key);

		key.size = SPRITE_SIZE_32x32;
		parent->colored_dc.erase(key);
	}
}

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
	// dc initialized to nullptr by unique_ptr default ctor
}

GameSprite::~GameSprite() {
	unloadDC();
	// instanced_templates and animator cleaned up automatically by unique_ptr
}

wxSize GameSprite::GetSize() const {
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
	const int dest_step = destHasAlpha ? RGBA_COMPONENTS : RGB_COMPONENTS;
	const int mask_step = RGB_COMPONENTS;

	std::span<uint8_t> destSpan(dest, pixelCount * dest_step);
	std::span<const uint8_t> maskSpan(mask, pixelCount * mask_step);

	for (size_t i : std::views::iota(0u, pixelCount)) {
		uint8_t& red = destSpan[i * dest_step + 0];
		uint8_t& green = destSpan[i * dest_step + 1];
		uint8_t& blue = destSpan[i * dest_step + 2];

		const uint8_t& tred = maskSpan[i * mask_step + 0];
		const uint8_t& tgreen = maskSpan[i * mask_step + 1];
		const uint8_t& tblue = maskSpan[i * mask_step + 2];

		if (tred && tgreen && !tblue) { // yellow => head
			OutfitColorizer::ColorizePixel(lookHead, red, green, blue);
		} else if (tred && !tgreen && !tblue) { // red => body
			OutfitColorizer::ColorizePixel(lookBody, red, green, blue);
		} else if (!tred && tgreen && !tblue) { // green => legs
			OutfitColorizer::ColorizePixel(lookLegs, red, green, blue);
		} else if (!tred && !tgreen && tblue) { // blue => feet
			OutfitColorizer::ColorizePixel(lookFeet, red, green, blue);
		}
	}
}

void GameSprite::clean(time_t time, int longevity) {
	for (auto& iter : instanced_templates) {
		iter->clean(time, longevity);
	}
}

void GameSprite::unloadDC() {
	dc[SPRITE_SIZE_16x16].reset();
	dc[SPRITE_SIZE_32x32].reset();
	dc[SPRITE_SIZE_64x64].reset();
	bm[SPRITE_SIZE_16x16].reset();
	bm[SPRITE_SIZE_32x32].reset();
	bm[SPRITE_SIZE_64x64].reset();
	colored_dc.clear();
}

int GameSprite::getDrawHeight() const {
	return draw_height;
}

bool GameSprite::isSimpleAndLoaded() const {
	return is_simple && !spriteList.empty() && spriteList[0] && spriteList[0]->isGLLoaded;
}

uint32_t GameSprite::getDebugImageId(size_t index) const {
	if (index < spriteList.size() && spriteList[index]->isNormalImage()) {
		return static_cast<const NormalImage*>(spriteList[index])->id;
	}
	return 0;
}

wxSize GameSprite::getCompositePixelSize() const {
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

	cached_composite_size = wxSize(max_width, max_height);
	cached_draw_offset = std::make_pair(
		static_cast<int>(drawoffset_x) + std::max(0, static_cast<int>(max_part_width) - SPRITE_PIXELS),
		static_cast<int>(drawoffset_y) + std::max(0, static_cast<int>(max_part_height) - SPRITE_PIXELS)
	);
	geometry_cache_dirty = false;
}

uint32_t GameSprite::getSpriteId(int frameIndex, int pattern_x, int pattern_y) const {
	auto idx = getIndex(width, height, 0, pattern_x, pattern_y, 0, frameIndex); // Assuming layer, pattern_z are 0 for this context
	if (idx >= 0 && static_cast<size_t>(idx) < spriteList.size() && spriteList[idx]->isNormalImage()) {
		return static_cast<const NormalImage*>(spriteList[idx])->id;
	}
	return 0;
}

std::pair<int, int> GameSprite::getDrawOffset() const {
	if (geometry_cache_dirty) {
		rebuildGeometryCache();
	}
	return cached_draw_offset;
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
	SpriteLayoutMetrics metrics;
	const uint8_t cols = std::min<uint8_t>(width, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	const uint8_t rows = std::min<uint8_t>(height, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	metrics.num_columns = cols;
	metrics.num_rows = rows;
	for (size_t i = 0; i < cols; ++i) {
		metrics.column_widths[i] = TILE_SIZE;
	}
	for (size_t i = 0; i < rows; ++i) {
		metrics.row_heights[i] = TILE_SIZE;
	}

	for (int cx = 0; cx < cols; ++cx) {
		for (int cy = 0; cy < rows; ++cy) {
			for (int layer = 0; layer < layers; ++layer) {
				const size_t index = resolvePlainSpriteIndex(*this, cx, cy, layer, key.subtype, key.pattern_x, key.pattern_y, key.pattern_z, key.frame);
				if (index >= spriteList.size() || !spriteList[index]) {
					continue;
				}

				const auto dimensions = spriteList[index]->getDimensions();
				metrics.column_widths[cx] = std::max<int>(metrics.column_widths[cx], static_cast<int>(dimensions.width));
				metrics.row_heights[cy] = std::max<int>(metrics.row_heights[cy], static_cast<int>(dimensions.height));
			}
		}
	}

	finalizeLayoutMetrics(metrics);
	return metrics;
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
	SpriteLayoutMetrics metrics;
	const uint8_t cols = std::min<uint8_t>(width, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	const uint8_t rows = std::min<uint8_t>(height, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	metrics.num_columns = cols;
	metrics.num_rows = rows;
	for (size_t i = 0; i < cols; ++i) {
		metrics.column_widths[i] = TILE_SIZE;
	}
	for (size_t i = 0; i < rows; ++i) {
		metrics.row_heights[i] = TILE_SIZE;
	}

	for (int cx = 0; cx < cols; ++cx) {
		for (int cy = 0; cy < rows; ++cy) {
			const size_t index = resolveOutfitSpriteIndex(*this, cx, cy, key.dir, key.addon, key.pattern_z, key.frame);
			if (index >= spriteList.size() || !spriteList[index]) {
				continue;
			}

			const auto dimensions = spriteList[index]->getDimensions();
			metrics.column_widths[cx] = std::max<int>(metrics.column_widths[cx], static_cast<int>(dimensions.width));
			metrics.row_heights[cy] = std::max<int>(metrics.row_heights[cy], static_cast<int>(dimensions.height));
		}
	}

	finalizeLayoutMetrics(metrics);
	return metrics;
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

			// Lazy set parent for cache invalidation (legacy path, kept for safety)
			spriteList[0]->parent = this;
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

	// Ensure parent is set for invalidation (even in slow path)
	if (spriteList[v]) {
		spriteList[v]->parent = this;
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
		spriteList[v]->parent = this;
		return spriteList[v]->getAtlasRegion();
	}
	return nullptr;
}

wxMemoryDC* GameSprite::getDC(SpriteSize size) {
	ASSERT(size == SPRITE_SIZE_16x16 || size == SPRITE_SIZE_32x32);

	if (!dc[size]) {
		wxBitmap bmp = SpriteIconGenerator::Generate(this, size);
		if (bmp.IsOk()) {
			bm[size] = std::make_unique<wxBitmap>(bmp);
			dc[size] = std::make_unique<wxMemoryDC>(*bm[size]);
		}
		g_gui.gfx.addSpriteToCleanup(this);
	}
	return dc[size].get();
}

wxMemoryDC* GameSprite::getDC(SpriteSize size, const Outfit& outfit) {
	ASSERT(size == SPRITE_SIZE_16x16 || size == SPRITE_SIZE_32x32);

	RenderKey key;
	key.size = size;
	key.colorHash = outfit.getColorHash();
	key.mountColorHash = outfit.getMountColorHash();
	key.lookMount = outfit.lookMount;
	key.lookAddon = outfit.lookAddon;
	key.lookMountHead = outfit.lookMountHead;
	key.lookMountBody = outfit.lookMountBody;
	key.lookMountLegs = outfit.lookMountLegs;
	key.lookMountFeet = outfit.lookMountFeet;

	auto it = colored_dc.find(key);
	if (it == colored_dc.end()) {
		wxBitmap bmp = SpriteIconGenerator::Generate(this, size, outfit);
		if (bmp.IsOk()) {
			auto cache = std::make_unique<CachedDC>();
			cache->bm = std::make_unique<wxBitmap>(bmp);
			cache->dc = std::make_unique<wxMemoryDC>(*cache->bm);

			auto res = colored_dc.insert(std::make_pair(key, std::move(cache)));
			g_gui.gfx.addSpriteToCleanup(this);
			return res.first->second->dc.get();
		}
		return nullptr;
	}
	return it->second->dc.get();
}

void GameSprite::DrawTo(wxDC* dc, SpriteSize sz, int start_x, int start_y, int width, int height) {
	const int sprite_dim = (sz == SPRITE_SIZE_64x64) ? 64 : (sz == SPRITE_SIZE_32x32 ? 32 : 16);
	int src_width = sprite_dim;
	int src_height = sprite_dim;

	if (width == -1) {
		width = src_width;
	}
	if (height == -1) {
		height = src_height;
	}
	wxDC* sdc = getDC(sz);
	if (sdc) {
		dc->StretchBlit(start_x, start_y, width, height, sdc, 0, 0, src_width, src_height, wxCOPY, true);
	} else {
		const wxBrush& b = dc->GetBrush();
		dc->SetBrush(*wxRED_BRUSH);
		dc->DrawRectangle(start_x, start_y, width, height);
		dc->SetBrush(b);
	}
}

void GameSprite::DrawTo(wxDC* dc, SpriteSize sz, const Outfit& outfit, int start_x, int start_y, int width, int height) {
	const int sprite_dim = (sz == SPRITE_SIZE_64x64) ? 64 : (sz == SPRITE_SIZE_32x32 ? 32 : 16);
	int src_width = sprite_dim;
	int src_height = sprite_dim;

	if (width == -1) {
		width = src_width;
	}
	if (height == -1) {
		height = src_height;
	}
	wxDC* sdc = getDC(sz, outfit);
	if (sdc) {
		dc->StretchBlit(start_x, start_y, width, height, sdc, 0, 0, src_width, src_height, wxCOPY, true);
	} else {
		const wxBrush& b = dc->GetBrush();
		dc->SetBrush(*wxRED_BRUSH);
		dc->DrawRectangle(start_x, start_y, width, height);
		dc->SetBrush(b);
	}
}

namespace {

	struct DecompressionContext {
		int id;
		uint8_t bpp;
		bool use_alpha;
		bool& non_zero_alpha_found;
		bool& non_black_pixel_found;
	};

	bool ProcessTransparencyRun(std::span<const uint8_t> dump, size_t& read, std::span<uint8_t> data, size_t& write, DecompressionContext ctx) {
		if (read + 1 >= dump.size()) {
			return false;
		}
		int transparent = dump[read] | dump[read + 1] << 8;

		// Integrity check for transparency run
		if (write + (transparent * RGBA_COMPONENTS) > data.size()) {
			spdlog::warn("Sprite {}: Transparency run overrun (transparent={}, write={}, max={})", ctx.id, transparent, write, data.size());
			transparent = (data.size() - write) / RGBA_COMPONENTS;
		}

		read += 2;
		std::ranges::fill(data.subspan(write, transparent * RGBA_COMPONENTS), 0);
		write += transparent * RGBA_COMPONENTS;
		return true;
	}

	bool ProcessColoredRun(std::span<const uint8_t> dump, size_t& read, std::span<uint8_t> data, size_t& write, DecompressionContext ctx) {
		if (read + 1 >= dump.size()) {
			return false;
		}
		int colored = dump[read] | dump[read + 1] << 8;
		read += 2;

		// Integrity check for colored run
		if (write + (colored * RGBA_COMPONENTS) > data.size()) {
			spdlog::warn("Sprite {}: Colored run overrun (colored={}, write={}, max={})", ctx.id, colored, write, data.size());
			colored = (data.size() - write) / RGBA_COMPONENTS;
		}

		// Integrity check for read buffer
		if (read + (colored * ctx.bpp) > dump.size()) {
			spdlog::warn("Sprite {}: Read buffer overrun (colored={}, bpp={}, read={}, size={})", ctx.id, colored, ctx.bpp, read, dump.size());
			// We can't easily recover here without risking reading garbage, so stop
			return false;
		}

		for (int cnt = 0; cnt < colored; ++cnt) {
			uint8_t r = dump[read + 0];
			uint8_t g = dump[read + 1];
			uint8_t b = dump[read + 2];
			uint8_t a = ctx.use_alpha ? dump[read + 3] : 0xFF;

			data[write + 0] = r;
			data[write + 1] = g;
			data[write + 2] = b;
			data[write + 3] = a;

			if (a > 0) {
				ctx.non_zero_alpha_found = true;
			}
			if (r > 0 || g > 0 || b > 0) {
				ctx.non_black_pixel_found = true;
			}

			write += RGBA_COMPONENTS;
			read += ctx.bpp;
		}
		return true;
	}

} // namespace

std::unique_ptr<uint8_t[]> GameSprite::Decompress(std::span<const uint8_t> dump, bool use_alpha, int id) {
	const int pixels_data_size = SPRITE_PIXELS_SIZE * RGBA_COMPONENTS;
	auto data_buffer = std::make_unique<uint8_t[]>(pixels_data_size);

	std::span<uint8_t> data(data_buffer.get(), pixels_data_size);

	uint8_t bpp = use_alpha ? 4 : 3;
	size_t write = 0;
	size_t read = 0;
	bool non_zero_alpha_found = false;
	bool non_black_pixel_found = false;

	DecompressionContext ctx {
		.id = id,
		.bpp = bpp,
		.use_alpha = use_alpha,
		.non_zero_alpha_found = non_zero_alpha_found,
		.non_black_pixel_found = non_black_pixel_found
	};

	// decompress pixels
	while (read < dump.size() && write < data.size()) {
		if (!ProcessTransparencyRun(dump, read, data, write, ctx)) {
			break;
		}

		if (read >= dump.size() || write >= data.size()) {
			break;
		}

		if (!ProcessColoredRun(dump, read, data, write, ctx)) {
			break;
		}
	}

	// fill remaining pixels
	while (write < data.size()) {
		data[write + 0] = 0x00; // red
		data[write + 1] = 0x00; // green
		data[write + 2] = 0x00; // blue
		data[write + 3] = 0x00; // alpha
		write += RGBA_COMPONENTS;
	}

	// Debug logging for diagnostic - verify if we are decoding pure transparency or pure blackness
	if (!non_zero_alpha_found && id > 100) {
		static int empty_log_count = 0;
		if (empty_log_count++ < 10) {
			spdlog::info("Sprite {}: Decoded fully transparent sprite. bpp used: {}, dump size: {}", id, bpp, dump.size());
		}
	} else if (!non_black_pixel_found && non_zero_alpha_found && id > 100) {
		static int black_log_count = 0;
		if (black_log_count++ < 10) {
			spdlog::warn("Sprite {}: Decoded PURE BLACK sprite (Alpha > 0, RGB = 0). bpp used: {}, dump size: {}. Check hasTransparency() config!", id, bpp, dump.size());
		}
	}

	return data_buffer;
}
