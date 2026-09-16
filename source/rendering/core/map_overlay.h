#ifndef RME_MAP_OVERLAY_H_
#define RME_MAP_OVERLAY_H_

#include <cstdint>
#include <string>
#include <vector>

struct Color4u {
	uint8_t r = 255;
	uint8_t g = 255;
	uint8_t b = 255;
	uint8_t a = 255;

	bool operator==(const Color4u&) const = default;
};

struct MapViewInfo {
	int start_x = 0;
	int start_y = 0;
	int end_x = 0;
	int end_y = 0;
	int floor = 0;
	float zoom = 1.0f;
	int view_scroll_x = 0;
	int view_scroll_y = 0;
	int tile_size = 32;
	int screen_width = 0;
	int screen_height = 0;
};

struct MapOverlayCommand {
	enum class Type {
		Rect,
		Line,
		Text,
		Sprite,
	};

	Type type = Type::Rect;
	bool screen_space = false;
	bool filled = true;
	bool dashed = false;
	int width = 1;

	int x = 0;
	int y = 0;
	int z = 0;
	int w = 0;
	int h = 0;
	int x2 = 0;
	int y2 = 0;
	int z2 = 0;

	uint32_t sprite_id = 0;

	std::string text;
	Color4u color = { 255, 255, 255, 255 };
};

struct MapOverlayTooltip {
	int x = 0;
	int y = 0;
	int z = 0;
	std::string text;
	Color4u color = { 255, 255, 255, 255 };
};

struct MapOverlayHoverState {
	bool valid = false;
	int x = 0;
	int y = 0;
	int z = 0;
	std::vector<MapOverlayCommand> commands;
	std::vector<MapOverlayTooltip> tooltips;
};

#endif // RME_MAP_OVERLAY_H_
