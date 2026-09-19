#include "rendering/drawers/minimap_cache.h"

#include "map/map.h"
#include "map/map_region.h"
#include "map/tile.h"

#include <algorithm>

uint64_t MinimapCache::makePageKey(int page_x, int page_y) {
	return (static_cast<uint64_t>(page_y) << 32) | static_cast<uint32_t>(page_x);
}

MinimapDirtyRect MinimapCache::clampRect(const MinimapDirtyRect& rect, int width, int height) {
	const int x0 = std::clamp(rect.x, 0, std::max(0, width));
	const int y0 = std::clamp(rect.y, 0, std::max(0, height));
	const int x1 = std::clamp(rect.x + rect.width, 0, std::max(0, width));
	const int y1 = std::clamp(rect.y + rect.height, 0, std::max(0, height));

	return {
		.x = x0,
		.y = y0,
		.width = std::max(0, x1 - x0),
		.height = std::max(0, y1 - y0),
	};
}

void MinimapCache::bindMap(uint64_t map_generation, int width, int height) {
	if (map_generation_ == map_generation && width_ == width && height_ == height) {
		return;
	}

	releaseGL();
	map_generation_ = map_generation;
	width_ = width;
	height_ = height;
}

void MinimapCache::invalidateAll() {
	releaseGL();
}

void MinimapCache::markDirty(int floor, const MinimapDirtyRect& rect) {
	if (floor < 0 || floor >= MAP_LAYERS || width_ <= 0 || height_ <= 0) {
		return;
	}

	const MinimapDirtyRect clamped = clampRect(rect, width_, height_);
	if (!IsValidMinimapRect(clamped)) {
		return;
	}

	const int start_page_x = clamped.x / PageSize;
	const int end_page_x = (clamped.x + clamped.width - 1) / PageSize;
	const int start_page_y = clamped.y / PageSize;
	const int end_page_y = (clamped.y + clamped.height - 1) / PageSize;

	for (int page_y = start_page_y; page_y <= end_page_y; ++page_y) {
		for (int page_x = start_page_x; page_x <= end_page_x; ++page_x) {
			auto& pages = floors_[floor].pages;
			auto it = pages.find(makePageKey(page_x, page_y));
			if (it == pages.end()) {
				// Unbuilt pages are initialized as fully dirty once they first become visible,
				// so there is no need to retain off-screen sub-rect dirtiness here.
				continue;
			}

			const int origin_x = page_x * PageSize;
			const int origin_y = page_y * PageSize;
			const MinimapDirtyRect local_rect = {
				.x = std::max(0, clamped.x - origin_x),
				.y = std::max(0, clamped.y - origin_y),
				.width = std::min(PageSize, clamped.x + clamped.width - origin_x) - std::max(0, clamped.x - origin_x),
				.height = std::min(PageSize, clamped.y + clamped.height - origin_y) - std::max(0, clamped.y - origin_y),
			};

			if (IsValidMinimapRect(local_rect)) {
				auto& dirty_rect = it->second.dirty_rect;
				dirty_rect = dirty_rect ? UnionMinimapRects(*dirty_rect, local_rect) : local_rect;
			}
		}
	}
}

FloorCachePage& MinimapCache::getOrCreatePage(int floor, int page_x, int page_y) {
	auto& pages = floors_[floor].pages;
	const uint64_t key = makePageKey(page_x, page_y);
	auto [it, inserted] = pages.try_emplace(key);
	if (inserted) {
		it->second.page_x = page_x;
		it->second.page_y = page_y;
		it->second.dirty_rect = MinimapDirtyRect {
			.x = 0,
			.y = 0,
			.width = PageSize,
			.height = PageSize,
		};
	}

	return it->second;
}

void MinimapCache::ensurePageTexture(FloorCachePage& page) {
	if (page.texture) {
		return;
	}

	page.texture = std::make_unique<GLTextureResource>(GL_TEXTURE_2D);
	glTextureStorage2D(page.texture->GetID(), 1, GL_R8UI, PageSize, PageSize);
	glTextureParameteri(page.texture->GetID(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(page.texture->GetID(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(page.texture->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(page.texture->GetID(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void MinimapCache::uploadRect(const Map& map, int floor, FloorCachePage& page, const MinimapDirtyRect& rect) {
	ensurePageTexture(page);

	const MinimapDirtyRect clamped_rect = clampRect(rect, PageSize, PageSize);
	if (!IsValidMinimapRect(clamped_rect)) {
		return;
	}

	const size_t buffer_size = static_cast<size_t>(clamped_rect.width) * clamped_rect.height;
	upload_buffer_.resize(buffer_size);
	std::fill(upload_buffer_.begin(), upload_buffer_.end(), 0);

	const int origin_x = page.page_x * PageSize + clamped_rect.x;
	const int origin_y = page.page_y * PageSize + clamped_rect.y;
	const int rect_end_x = origin_x + clamped_rect.width - 1;
	const int rect_end_y = origin_y + clamped_rect.height - 1;
	map.visitLeaves(origin_x, origin_y, rect_end_x, rect_end_y, [&](const MapNode* node, int node_map_x, int node_map_y) {
		const Floor* node_floor = node->getFloor(floor);
		if (!node_floor) {
			return;
		}

		for (int local_node_x = 0; local_node_x < 4; ++local_node_x) {
			const int map_x = node_map_x + local_node_x;
			if (map_x < origin_x || map_x > rect_end_x) {
				continue;
			}

			for (int local_node_y = 0; local_node_y < 4; ++local_node_y) {
				const int map_y = node_map_y + local_node_y;
				if (map_y < origin_y || map_y > rect_end_y) {
					continue;
				}

				const int floor_index = local_node_x * 4 + local_node_y;
				const TileLocation& location = node_floor->locs[static_cast<size_t>(floor_index)];
				const Tile* tile = location.get();
				if (!tile) {
					continue;
				}

				const int buffer_x = map_x - origin_x;
				const int buffer_y = map_y - origin_y;
				upload_buffer_[static_cast<size_t>(buffer_y) * clamped_rect.width + buffer_x] = tile->getMiniMapColor();
			}
		}
	});

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(
		page.texture->GetID(),
		0,
		clamped_rect.x,
		clamped_rect.y,
		clamped_rect.width,
		clamped_rect.height,
		GL_RED_INTEGER,
		GL_UNSIGNED_BYTE,
		upload_buffer_.data());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void MinimapCache::flushVisible(const Map& map, int floor, const MinimapDirtyRect& visible_rect) {
	if (floor < 0 || floor >= MAP_LAYERS || width_ <= 0 || height_ <= 0) {
		return;
	}

	const MinimapDirtyRect clamped_visible = clampRect(visible_rect, width_, height_);
	if (!IsValidMinimapRect(clamped_visible)) {
		return;
	}

	const int start_page_x = clamped_visible.x / PageSize;
	const int end_page_x = (clamped_visible.x + clamped_visible.width - 1) / PageSize;
	const int start_page_y = clamped_visible.y / PageSize;
	const int end_page_y = (clamped_visible.y + clamped_visible.height - 1) / PageSize;

	for (int page_y = start_page_y; page_y <= end_page_y; ++page_y) {
		for (int page_x = start_page_x; page_x <= end_page_x; ++page_x) {
			auto& page = getOrCreatePage(floor, page_x, page_y);
			if (!page.dirty_rect.has_value()) {
				continue;
			}

			uploadRect(map, floor, page, *page.dirty_rect);
			page.dirty_rect.reset();
		}
	}
}

std::vector<MinimapCache::VisiblePage> MinimapCache::collectVisiblePages(int floor, const MinimapDirtyRect& visible_rect) {
	std::vector<VisiblePage> visible_pages;
	if (floor < 0 || floor >= MAP_LAYERS || width_ <= 0 || height_ <= 0) {
		return visible_pages;
	}

	const MinimapDirtyRect clamped_visible = clampRect(visible_rect, width_, height_);
	if (!IsValidMinimapRect(clamped_visible)) {
		return visible_pages;
	}

	const int start_page_x = clamped_visible.x / PageSize;
	const int end_page_x = (clamped_visible.x + clamped_visible.width - 1) / PageSize;
	const int start_page_y = clamped_visible.y / PageSize;
	const int end_page_y = (clamped_visible.y + clamped_visible.height - 1) / PageSize;

	for (int page_y = start_page_y; page_y <= end_page_y; ++page_y) {
		for (int page_x = start_page_x; page_x <= end_page_x; ++page_x) {
			auto& page = getOrCreatePage(floor, page_x, page_y);
			ensurePageTexture(page);
			visible_pages.push_back({
				.texture_id = page.texture->GetID(),
				.page_x = page_x,
				.page_y = page_y,
			});
		}
	}

	return visible_pages;
}

void MinimapCache::releaseGL() {
	for (auto& floor_cache : floors_) {
		floor_cache.pages.clear();
	}
	upload_buffer_.clear();
}
