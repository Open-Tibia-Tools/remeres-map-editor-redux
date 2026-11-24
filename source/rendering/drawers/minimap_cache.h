#ifndef RME_RENDERING_DRAWERS_MINIMAP_CACHE_H_
#define RME_RENDERING_DRAWERS_MINIMAP_CACHE_H_

#include "app/definitions.h"
#include "rendering/core/gl_resources.h"
#include "rendering/drawers/minimap_rect.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

class Map;

struct FloorCachePage {
	int page_x = 0;
	int page_y = 0;
	std::unique_ptr<GLTextureResource> texture;
	std::optional<MinimapDirtyRect> dirty_rect;
};

class MinimapCache {
public:
	static constexpr int PageSize = kMinimapPageSize;

	struct VisiblePage {
		GLuint texture_id = 0;
		int page_x = 0;
		int page_y = 0;
	};

	void bindMap(uint64_t map_generation, int width, int height);
	void invalidateAll();
	void markDirty(int floor, const MinimapDirtyRect& rect);
	void flushVisible(const Map& map, int floor, const MinimapDirtyRect& visible_rect);
	std::vector<VisiblePage> collectVisiblePages(int floor, const MinimapDirtyRect& visible_rect);
	void releaseGL();

	int getWidth() const {
		return width_;
	}

	int getHeight() const {
		return height_;
	}

private:
	struct FloorCache {
		std::unordered_map<uint64_t, FloorCachePage> pages;
	};

	static uint64_t makePageKey(int page_x, int page_y);
	static MinimapDirtyRect clampRect(const MinimapDirtyRect& rect, int width, int height);

	FloorCachePage& getOrCreatePage(int floor, int page_x, int page_y);
	void ensurePageTexture(FloorCachePage& page);
	void uploadRect(const Map& map, int floor, FloorCachePage& page, const MinimapDirtyRect& rect);

	std::array<FloorCache, MAP_LAYERS> floors_;
	std::vector<uint8_t> upload_buffer_;
	uint64_t map_generation_ = 0;
	int width_ = 0;
	int height_ = 0;
};

#endif
