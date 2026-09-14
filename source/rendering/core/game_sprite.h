//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_GAME_SPRITE_H_
#define RME_RENDERING_CORE_GAME_SPRITE_H_

#include "game/outfit.h"
#include "util/common.h"
#include "rendering/core/animator.h"
#include "rendering/core/sprite_light.h"
#include "rendering/core/texture_garbage_collector.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/core/render_timer.h"
#include "rendering/core/normal_image.h"
#include <atomic>
#include <cstdint>
#include <span>

#include <array>
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>

enum SpriteSize {
	SPRITE_SIZE_16x16,
	// SPRITE_SIZE_24x24,
	SPRITE_SIZE_32x32,
	SPRITE_SIZE_64x64,
	SPRITE_SIZE_COUNT
};

class GraphicManager;
class SpritePreloader;

class Sprite {
public:
	Sprite() = default;
	virtual ~Sprite() = default;

	virtual ImageDimensions GetSize() const = 0;

private:
	Sprite(const Sprite&) = delete;
	Sprite& operator=(const Sprite&) = delete;
};

class GameSprite;
class CreatureSprite : public Sprite {
public:
	CreatureSprite(GameSprite* parent, const Outfit& outfit) :
		parent(parent), outfit(outfit) {}
	~CreatureSprite() override = default;

	ImageDimensions GetSize() const override {
		return ImageDimensions { 32, 32 };
	}

	GameSprite* parent;
	Outfit outfit;
};

#include "rendering/core/sprite_layout_calculator.h"

class Image;
class TemplateImage;

class GameSprite : public Sprite {
public:
	static constexpr size_t MAX_SPRITE_PARTS = ::MAX_SPRITE_PARTS;
	using SpriteLayoutMetrics = ::SpriteLayoutMetrics;

	GameSprite();
	~GameSprite() override;

	size_t getIndex(int width, int height, int layer, int pattern_x, int pattern_y, int pattern_z, int frame) const;

	// Phase 2: Get atlas region for texture array rendering
	const AtlasRegion* getAtlasRegion(int _x, int _y, int _layer, int _subtype, int _pattern_x, int _pattern_y, int _pattern_z, int _frame);
	const AtlasRegion* getAtlasRegion(int _x, int _y, int _dir, int _addon, int _pattern_z, const Outfit& _outfit, int _frame);

	ImageDimensions GetSize() const override;

	void clean(time_t time, int longevity = -1);

	int getDrawHeight() const;
	[[nodiscard]] const std::pair<int, int>& getDrawOffset() const {
		if (geometry_cache_dirty) {
			rebuildGeometryCache();
		}
		return cached_draw_offset;
	}
	uint8_t getMiniMapColor() const;
	SpriteLayoutMetrics getPlainLayoutMetrics(int subtype, int pattern_x, int pattern_y, int pattern_z, int frame);
	SpriteLayoutMetrics getOutfitLayoutMetrics(int dir, int addon, int pattern_z, int frame);
	void invalidateMetricCaches();

	bool hasLight() const noexcept {
		return has_light;
	}
	const SpriteLight& getLight() const noexcept {
		return light;
	}

	// Helper for SpritePreloader to decompress data off-thread
	[[nodiscard]] static std::unique_ptr<uint8_t[]> Decompress(std::span<const uint8_t> dump, bool use_alpha, int id = 0);

	static void ColorizeTemplatePixels(uint8_t* dest, const uint8_t* mask, size_t pixelCount, int lookHead, int lookBody, int lookLegs, int lookFeet, bool destHasAlpha);

	// Exposed for NormalImage::clean to invalidate cache
	void invalidateCache(const AtlasRegion* region);

private:
	using PlainLayoutCacheKey = ::PlainLayoutCacheKey;
	using OutfitLayoutCacheKey = ::OutfitLayoutCacheKey;

	void rebuildGeometryCache() const;
	SpriteLayoutMetrics buildPlainLayoutMetrics(const PlainLayoutCacheKey& key) const;
	SpriteLayoutMetrics buildOutfitLayoutMetrics(const OutfitLayoutCacheKey& key) const;

	struct PlainLayoutCacheEntry {
		PlainLayoutCacheKey key;
		SpriteLayoutMetrics metrics;
	};

	struct OutfitLayoutCacheEntry {
		OutfitLayoutCacheKey key;
		SpriteLayoutMetrics metrics;
	};

protected:
	TemplateImage* getTemplateImage(int sprite_index, const Outfit& outfit);

	uint32_t id;

public:
	// GameSprite info
	uint8_t height;
	uint8_t width;
	uint8_t layers;
	uint8_t pattern_x;
	uint8_t pattern_y;
	uint8_t pattern_z;
	uint8_t frames;
	uint32_t numsprites;
	uint32_t getId() const {
		return id;
	}
	uint32_t getDebugImageId(size_t index = 0) const;
	ImageDimensions getCompositePixelSize() const;

	std::unique_ptr<Animator> animator;

	uint16_t draw_height;
	uint16_t drawoffset_x;
	uint16_t drawoffset_y;

	uint16_t minimap_color;

	bool has_light = false;
	SpriteLight light;

	std::vector<uint32_t> sprite_ids;
	std::vector<NormalImage*> spriteList;
	std::vector<std::unique_ptr<TemplateImage>> instanced_templates; // Templates that use this sprite
	bool is_resident = false; // Tracks if this GameSprite is in resident_game_sprites

	friend class GraphicManager;
	friend class GraphicsAssembler;
	friend class SpriteIconService;
	friend class TextureGarbageCollector;
	friend class TooltipDrawer;
	friend class SpritePreloader;

	// Exposed for fast-path rendering (BlitItem)
	const AtlasRegion* getCachedDefaultRegion() const {
		return cached_default_region;
	}

	// DEBUG: Get the actual image ID that would be rendered for these coordinates
	uint32_t getSpriteId(int frameIndex, int pattern_x, int pattern_y) const;

	[[nodiscard]] bool isSimpleAndLoaded() const noexcept {
		return is_simple && !spriteList.empty() && spriteList[0] && spriteList[0]->isGLLoaded;
	}

	bool is_simple = false;
	void updateSimpleStatus() {
		is_simple = (numsprites == 1 && frames == 1 && layers == 1 && width == 1 && height == 1 && !spriteList.empty());
	}

protected:
	// Cache for default state (0,0,0,0) to avoid lookups/virtual calls for simple sprites
	mutable const AtlasRegion* cached_default_region = nullptr;
	uint32_t cached_generation_id = 0;
	uint32_t cached_sprite_id = 0;
	mutable bool geometry_cache_dirty = true;
	mutable ImageDimensions cached_composite_size;
	mutable std::pair<int, int> cached_draw_offset;
	static constexpr size_t LAYOUT_CACHE_CAPACITY = 8;
	mutable std::array<PlainLayoutCacheEntry, LAYOUT_CACHE_CAPACITY> plain_layout_cache_entries_ {};
	mutable uint8_t plain_layout_cache_count_ = 0;
	mutable std::array<OutfitLayoutCacheEntry, LAYOUT_CACHE_CAPACITY> outfit_layout_cache_entries_ {};
	mutable uint8_t outfit_layout_cache_count_ = 0;
};

#endif
