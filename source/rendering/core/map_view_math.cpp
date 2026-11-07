#include "app/definitions.h"
#include "rendering/core/map_view_math.h"

#include <algorithm>
#include <cmath>

int MainMapViewMath::GetFloorTileOffset(int floor) {
	if (floor <= GROUND_LAYER) {
		return GROUND_LAYER - floor;
	}

	return 0;
}

int MainMapViewMath::WorldTileToScrollPixel(int tile, int floor) {
	return (tile - GetFloorTileOffset(floor)) * TILE_SIZE;
}

MainMapVisibleRect MainMapViewMath::GetVisibleRect(const MainMapViewport& viewport) {
	const double tile_size = static_cast<double>(TILE_SIZE) / std::max(0.0001, viewport.zoom);
	const double start_x = static_cast<double>(viewport.view_scroll_x) / TILE_SIZE + GetFloorTileOffset(viewport.floor);
	const double start_y = static_cast<double>(viewport.view_scroll_y) / TILE_SIZE + GetFloorTileOffset(viewport.floor);
	// Keep one extra tile of padding so partially visible edge tiles remain included
	// and camera-box projection does not clip at exact tile boundaries.
	const double width = viewport.pixel_width / tile_size + 1.0;
	const double height = viewport.pixel_height / tile_size + 1.0;

	return {
		.start_x = start_x,
		.start_y = start_y,
		.width = width,
		.height = height,
	};
}

MainMapPixelPosition MainMapViewMath::ScreenToMapPixel(int screen_x, int screen_y, const MainMapViewport& viewport) {
	const int scaled_x = static_cast<int>(screen_x * viewport.scale_factor);
	const int scaled_y = static_cast<int>(screen_y * viewport.scale_factor);

	// Negative screen coordinates can appear when callers project or test positions just outside the
	// visible viewport. Keep the negative branch paired with MapToScreen() so both conversions stay
	// symmetric across zero instead of applying zoom on one side of the boundary but not the other.
	const int raw_x = (scaled_x < 0)
		? (viewport.view_scroll_x + scaled_x)
		: static_cast<int>(viewport.view_scroll_x + (scaled_x * viewport.zoom));
	const int raw_y = (scaled_y < 0)
		? (viewport.view_scroll_y + scaled_y)
		: static_cast<int>(viewport.view_scroll_y + (scaled_y * viewport.zoom));

	int tile_x = raw_x / TILE_SIZE;
	int tile_y = raw_y / TILE_SIZE;
	int pixel_x = raw_x - tile_x * TILE_SIZE;
	int pixel_y = raw_y - tile_y * TILE_SIZE;

	if (pixel_x < 0) {
		pixel_x += TILE_SIZE;
		--tile_x;
	}
	if (pixel_y < 0) {
		pixel_y += TILE_SIZE;
		--tile_y;
	}

	return {
		.map_x = tile_x + GetFloorTileOffset(viewport.floor),
		.map_y = tile_y + GetFloorTileOffset(viewport.floor),
		.pixel_x = pixel_x,
		.pixel_y = pixel_y,
	};
}

void MainMapViewMath::ScreenToMap(int screen_x, int screen_y, const MainMapViewport& viewport, int* map_x, int* map_y) {
	const MainMapPixelPosition position = ScreenToMapPixel(screen_x, screen_y, viewport);
	*map_x = position.map_x;
	*map_y = position.map_y;
}

void MainMapViewMath::MapToScreen(int map_x, int map_y, int map_z, const MainMapViewport& viewport, int* screen_x, int* screen_y) {
	map_x -= GetFloorTileOffset(map_z);
	map_y -= GetFloorTileOffset(map_z);

	const double raw_x = static_cast<double>(map_x * TILE_SIZE - viewport.view_scroll_x);
	const double raw_y = static_cast<double>(map_y * TILE_SIZE - viewport.view_scroll_y);

	// Mirror ScreenToMap() across the sign boundary. Negative deltas stay in scaled-pixel space,
	// while non-negative deltas divide by zoom * scale_factor, which keeps the round-trip stable
	// when floor offsets push a tile just outside the viewport.
	if (raw_x < 0.0) {
		*screen_x = static_cast<int>(std::ceil(raw_x / viewport.scale_factor));
	} else {
		*screen_x = static_cast<int>(std::ceil(raw_x / (viewport.zoom * viewport.scale_factor)));
	}

	if (raw_y < 0.0) {
		*screen_y = static_cast<int>(std::ceil(raw_y / viewport.scale_factor));
	} else {
		*screen_y = static_cast<int>(std::ceil(raw_y / (viewport.zoom * viewport.scale_factor)));
	}
}
