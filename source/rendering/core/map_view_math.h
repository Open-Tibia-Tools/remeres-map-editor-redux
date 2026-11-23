#ifndef RME_RENDERING_CORE_MAP_VIEW_MATH_H_
#define RME_RENDERING_CORE_MAP_VIEW_MATH_H_

#include "app/definitions.h"

struct MainMapViewport {
	int view_scroll_x = 0;
	int view_scroll_y = 0;
	int logical_width = 0;
	int logical_height = 0;
	int pixel_width = 0;
	int pixel_height = 0;
	double zoom = 1.0;
	int floor = GROUND_LAYER;
	double scale_factor = 1.0;
};

struct MainMapVisibleRect {
	double start_x = 0.0;
	double start_y = 0.0;
	double width = 0.0;
	double height = 0.0;
};

struct MainMapPixelPosition {
	int map_x = 0;
	int map_y = 0;
	int pixel_x = 0;
	int pixel_y = 0;
};

class MainMapViewMath {
public:
	[[nodiscard]] static int GetFloorTileOffset(int floor) {
		if (floor <= GROUND_LAYER) {
			return GROUND_LAYER - floor;
		}
		return 0;
	}
	static int WorldTileToScrollPixel(int tile, int floor);
	static MainMapVisibleRect GetVisibleRect(const MainMapViewport& viewport);
	static MainMapPixelPosition ScreenToMapPixel(
		int screen_x,
		int screen_y,
		const MainMapViewport& viewport);
	static void ScreenToMap(
		int screen_x,
		int screen_y,
		const MainMapViewport& viewport,
		int* map_x,
		int* map_y);
	static void MapToScreen(
		int map_x,
		int map_y,
		int map_z,
		const MainMapViewport& viewport,
		int* screen_x,
		int* screen_y);
	// Float variant without ceil quantization, for overlay drawers that need sub-DIP grid lines.
	// Uses the uniform zoom * scale_factor divisor; the negative-delta asymmetry of MapToScreen
	// is a round-trip pairing artifact and does not apply to overlays drawn inside the viewport.
	[[nodiscard]] static void MapTileToScreenDIP(
		int tile_x,
		int tile_y,
		int floor,
		const MainMapViewport& viewport,
		double* screen_x,
		double* screen_y) {
		const int floor_offset = GetFloorTileOffset(floor);
		const double raw_x = (static_cast<double>(tile_x - floor_offset) * TILE_SIZE - viewport.view_scroll_x);
		const double raw_y = (static_cast<double>(tile_y - floor_offset) * TILE_SIZE - viewport.view_scroll_y);
		*screen_x = raw_x / (viewport.zoom * viewport.scale_factor);
		*screen_y = raw_y / (viewport.zoom * viewport.scale_factor);
	}
};

#endif
