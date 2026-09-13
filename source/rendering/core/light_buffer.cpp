#include "light_buffer.h"

#include <cmath>

#include "app/definitions.h"
#include "rendering/core/render_view.h"

namespace {
	[[nodiscard]] int floorDiv(int value, int divisor) noexcept {
		const int quotient = value / divisor;
		const int remainder = value % divisor;
		return (remainder != 0 && ((remainder < 0) != (divisor < 0))) ? quotient - 1 : quotient;
	}

	[[nodiscard]] int projectedPaddingTiles(const RenderView& view) noexcept {
		const int traversed_floor_span = std::max(0, view.start_z - view.superend_z);
		int max_projected_offset = 0;
		for (int map_z = view.start_z; map_z >= view.superend_z; --map_z) {
			int offset_tiles = 0;
			if (map_z <= GROUND_LAYER) {
				offset_tiles = GROUND_LAYER - map_z;
			} else if (map_z < view.floor) {
				offset_tiles = view.floor - map_z;
			}

			max_projected_offset = std::max(max_projected_offset, offset_tiles);
		}

		return 1 + traversed_floor_span + max_projected_offset;
	}
}

void LightBuffer::Prepare(const RenderView& view) {
	const int buffer_padding_tiles = projectedPaddingTiles(view);
	const int right_edge = static_cast<int>(std::ceil((view.view_scroll_x + view.logical_width) / static_cast<float>(TILE_SIZE))) + buffer_padding_tiles;
	const int bottom_edge = static_cast<int>(std::ceil((view.view_scroll_y + view.logical_height) / static_cast<float>(TILE_SIZE))) + buffer_padding_tiles;

	origin_x = floorDiv(view.view_scroll_x, TILE_SIZE) - buffer_padding_tiles;
	origin_y = floorDiv(view.view_scroll_y, TILE_SIZE) - buffer_padding_tiles;
	width = std::max(1, right_edge - origin_x);
	height = std::max(1, bottom_edge - origin_y);
	const size_t total_tiles = static_cast<size_t>(width * height);
	tiles.resize(total_tiles);
	std::fill(tiles.begin(), tiles.end(), TileLight {});
}

void LightBuffer::AddLight(int pixel_x, int pixel_y, const SpriteLight& light) {
	const uint8_t intensity = light.intensity;

	if (!lights.empty()) {
		Light& previous = lights.back();
		if (previous.pixel_x == pixel_x && previous.pixel_y == pixel_y && previous.color == light.color) {
			previous.intensity = std::max(previous.intensity, intensity);
			return;
		}
	}

	lights.push_back(Light {
		.pixel_x = pixel_x,
		.pixel_y = pixel_y,
		.color = light.color,
		.intensity = intensity
	});
}

void LightBuffer::AddTileLight(int tile_x, int tile_y, const SpriteLight& light) {
	AddLight(tile_x * TILE_SIZE + TILE_SIZE / 2, tile_y * TILE_SIZE + TILE_SIZE / 2, light);
}

void LightBuffer::AddScreenLight(int screen_x, int screen_y, const RenderView& view, const SpriteLight& light) {
	AddLight(screen_x + view.view_scroll_x, screen_y + view.view_scroll_y, light);
}

void LightBuffer::Clear() {
	lights.clear();
	tiles.clear();
	origin_x = 0;
	origin_y = 0;
	width = 0;
	height = 0;
	current_floor_light_start = 0;
}
