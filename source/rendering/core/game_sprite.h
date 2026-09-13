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
#include <atomic>
#include <cstdint>
#include <span>

#include <array>
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>
#include <wx/dc.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>

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
	Sprite() { }
	virtual ~Sprite() = default;

	virtual void DrawTo(wxDC* dc, SpriteSize sz, int start_x, int start_y, int width = -1, int height = -1) = 0;
	virtual void unloadDC() = 0;
	virtual wxSize GetSize() const = 0;

private:
	Sprite(const Sprite&);
	Sprite& operator=(const Sprite&);
};

class GameSprite;
class CreatureSprite : public Sprite {
public:
	CreatureSprite(GameSprite* parent, const Outfit& outfit);
	~CreatureSprite() override;

	void DrawTo(wxDC* dc, SpriteSize sz, int start_x, int start_y, int width = -1, int height = -1) override;
	virtual void unloadDC() override;
	wxSize GetSize() const override {
		return wxSize(32, 32);
	}

	GameSprite* parent;
	Outfit outfit;
};

class Image;
class NormalImage;
class TemplateImage;

class GameSprite : public Sprite {
public:
	static constexpr size_t MAX_SPRITE_PARTS = 16;

	struct SpriteLayoutMetrics {
		std::array<int, MAX_SPRITE_PARTS> column_widths {};
		std::array<int, MAX_SPRITE_PARTS> row_heights {};
		uint8_t num_columns = 1;
		uint8_t num_rows = 1;
		int total_width = TILE_SIZE;
		int total_height = TILE_SIZE;
		int left_offset = 0;
		int top_offset = 0;
	};

	GameSprite();
	~GameSprite() override;

	size_t getIndex(int width, int height, int layer, int pattern_x, int pattern_y, int pattern_z, int frame) const;

	// Phase 2: Get atlas region for texture array rendering
	const AtlasRegion* getAtlasRegion(int _x, int _y, int _layer, int _subtype, int _pattern_x, int _pattern_y, int _pattern_z, int _frame);
	const AtlasRegion* getAtlasRegion(int _x, int _y, int _dir, int _addon, int _pattern_z, const Outfit& _outfit, int _frame);

	void DrawTo(wxDC* dc, SpriteSize sz, int start_x, int start_y, int width = -1, int height = -1) override;
	virtual void DrawTo(wxDC* dc, SpriteSize sz, const Outfit& outfit, int start_x, int start_y, int width = -1, int height = -1);

	void unloadDC() override;

	wxSize GetSize() const override;

	void clean(time_t time, int longevity = -1);

	int getDrawHeight() const;
	std::pair<int, int> getDrawOffset() const;
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
	struct PlainLayoutCacheKey {
		int subtype = 0;
		int pattern_x = 0;
		int pattern_y = 0;
		int pattern_z = 0;
		int frame = 0;

		bool operator==(const PlainLayoutCacheKey& other) const = default;
	};

	struct OutfitLayoutCacheKey {
		int dir = 0;
		int addon = 0;
		int pattern_z = 0;
		int frame = 0;

		bool operator==(const OutfitLayoutCacheKey& other) const = default;
	};

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
	wxMemoryDC* getDC(SpriteSize size);
	wxMemoryDC* getDC(SpriteSize size, const Outfit& outfit);
	TemplateImage* getTemplateImage(int sprite_index, const Outfit& outfit);

	uint32_t id;
	std::unique_ptr<wxMemoryDC> dc[SPRITE_SIZE_COUNT];
	std::unique_ptr<wxBitmap> bm[SPRITE_SIZE_COUNT];

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
	wxSize getCompositePixelSize() const;

	std::unique_ptr<Animator> animator;

	uint16_t draw_height;
	uint16_t drawoffset_x;
	uint16_t drawoffset_y;

	uint16_t minimap_color;

	bool has_light = false;
	SpriteLight light;

	std::vector<NormalImage*> spriteList;
	std::vector<std::unique_ptr<TemplateImage>> instanced_templates; // Templates that use this sprite
	struct CachedDC {
		std::unique_ptr<wxMemoryDC> dc;
		std::unique_ptr<wxBitmap> bm;
	};

	struct RenderKey {
		SpriteSize size;
		uint32_t colorHash;
		uint32_t mountColorHash;
		int lookMount, lookAddon, lookMountHead, lookMountBody, lookMountLegs, lookMountFeet;

		bool operator==(const RenderKey& rk) const {
			return size == rk.size && colorHash == rk.colorHash && mountColorHash == rk.mountColorHash && lookMount == rk.lookMount && lookAddon == rk.lookAddon && lookMountHead == rk.lookMountHead && lookMountBody == rk.lookMountBody && lookMountLegs == rk.lookMountLegs && lookMountFeet == rk.lookMountFeet;
		}
	};

	struct RenderKeyHash {
		size_t operator()(const RenderKey& k) const noexcept {
			// Combine hashes of the most significant fields
			size_t h = std::hash<uint64_t> {}((uint64_t(k.colorHash) << 32) | k.mountColorHash);
			h ^= std::hash<uint64_t> {}((uint64_t(k.lookMount) << 32) | k.lookAddon) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<uint64_t> {}((uint64_t(k.lookMountHead) << 32) | k.lookMountBody) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
	std::unordered_map<RenderKey, std::unique_ptr<CachedDC>, RenderKeyHash> colored_dc;

	bool is_resident = false; // Tracks if this GameSprite is in resident_game_sprites

	friend class GraphicManager;
	friend class GraphicsAssembler;
	friend class SpriteIconGenerator;
	friend class TextureGarbageCollector;
	friend class TooltipDrawer;
	friend class SpritePreloader;

	// Exposed for fast-path rendering (BlitItem)
	const AtlasRegion* getCachedDefaultRegion() const {
		return cached_default_region;
	}

	// DEBUG: Get the actual image ID that would be rendered for these coordinates
	uint32_t getSpriteId(int frameIndex, int pattern_x, int pattern_y) const;

	bool isSimpleAndLoaded() const;

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
	mutable wxSize cached_composite_size;
	mutable std::pair<int, int> cached_draw_offset;
	static constexpr size_t LAYOUT_CACHE_CAPACITY = 8;
	mutable std::array<PlainLayoutCacheEntry, LAYOUT_CACHE_CAPACITY> plain_layout_cache_entries_ {};
	mutable uint8_t plain_layout_cache_count_ = 0;
	mutable std::array<OutfitLayoutCacheEntry, LAYOUT_CACHE_CAPACITY> outfit_layout_cache_entries_ {};
	mutable uint8_t outfit_layout_cache_count_ = 0;
};

#endif
