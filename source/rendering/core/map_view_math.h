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
	static int GetFloorTileOffset(int floor);
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
};

#endif
