#include <cmath>
#include <cstdlib>
#include <iostream>

#include "rendering/core/map_view_math.h"
#include "rendering/ui/selection_geometry.h"

#define CHECK(expression) \
	do { \
		if (!(expression)) { \
			std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ \
			          << " (" #expression ")" << std::endl; \
			std::exit(1); \
		} \
	} while (false)

namespace {

	MainMapViewport MakeViewport(double zoom, double scale, int floor = GROUND_LAYER, int scroll_x = 0, int scroll_y = 0) {
		MainMapViewport viewport;
		viewport.view_scroll_x = scroll_x;
		viewport.view_scroll_y = scroll_y;
		viewport.zoom = zoom;
		viewport.floor = floor;
		viewport.scale_factor = scale;
		return viewport;
	}

	void TestFloorOffsets() {
		CHECK(MainMapViewMath::GetFloorTileOffset(7) == 0);
		CHECK(MainMapViewMath::GetFloorTileOffset(6) == 1);
		CHECK(MainMapViewMath::GetFloorTileOffset(5) == 2);
		CHECK(MainMapViewMath::GetFloorTileOffset(8) == 0);
		CHECK(MainMapViewMath::GetFloorTileOffset(15) == 0);
	}

	void TestScreenToMapPixelAtZoomOne() {
		const MainMapViewport viewport = MakeViewport(1.0, 1.0);

		const MainMapPixelPosition origin = MainMapViewMath::ScreenToMapPixel(0, 0, viewport);
		CHECK(origin.map_x == 0);
		CHECK(origin.map_y == 0);
		CHECK(origin.pixel_x == 0);
		CHECK(origin.pixel_y == 0);

		const MainMapPixelPosition last_pixel = MainMapViewMath::ScreenToMapPixel(31, 31, viewport);
		CHECK(last_pixel.map_x == 0);
		CHECK(last_pixel.map_y == 0);
		CHECK(last_pixel.pixel_x == 31);
		CHECK(last_pixel.pixel_y == 31);

		const MainMapPixelPosition next_tile = MainMapViewMath::ScreenToMapPixel(32, 64, viewport);
		CHECK(next_tile.map_x == 1);
		CHECK(next_tile.map_y == 2);
		CHECK(next_tile.pixel_x == 0);
		CHECK(next_tile.pixel_y == 0);

		const MainMapPixelPosition offset = MainMapViewMath::ScreenToMapPixel(70, 40, viewport);
		CHECK(offset.map_x == 2);
		CHECK(offset.map_y == 1);
		CHECK(offset.pixel_x == 6);
		CHECK(offset.pixel_y == 8);
	}

	void TestScreenToMapPixelAtOtherZooms() {
		const MainMapViewport zoom_two = MakeViewport(2.0, 1.0);
		const MainMapPixelPosition zoom_two_left = MainMapViewMath::ScreenToMapPixel(8, 8, zoom_two);
		CHECK(zoom_two_left.map_x == 0);
		CHECK(zoom_two_left.pixel_x == 16);
		const MainMapPixelPosition zoom_two_tile = MainMapViewMath::ScreenToMapPixel(16, 16, zoom_two);
		CHECK(zoom_two_tile.map_x == 1);
		CHECK(zoom_two_tile.pixel_x == 0);

		const MainMapViewport zoom_half = MakeViewport(0.5, 1.0);
		const MainMapPixelPosition zoom_half_left = MainMapViewMath::ScreenToMapPixel(32, 32, zoom_half);
		CHECK(zoom_half_left.map_x == 0);
		CHECK(zoom_half_left.pixel_x == 16);
		const MainMapPixelPosition zoom_half_tile = MainMapViewMath::ScreenToMapPixel(64, 64, zoom_half);
		CHECK(zoom_half_tile.map_x == 1);
		CHECK(zoom_half_tile.pixel_x == 0);
	}

	void TestContentScale() {
		const MainMapViewport scaled = MakeViewport(1.0, 1.25);
		const MainMapPixelPosition last_pixel = MainMapViewMath::ScreenToMapPixel(25, 25, scaled);
		CHECK(last_pixel.map_x == 0);
		CHECK(last_pixel.pixel_x == 31);
		const MainMapPixelPosition next_tile = MainMapViewMath::ScreenToMapPixel(26, 26, scaled);
		CHECK(next_tile.map_x == 1);
		CHECK(next_tile.pixel_x == 0);
	}

	void TestScrollAndFloorOffset() {
		const MainMapViewport scrolled = MakeViewport(1.0, 1.0, GROUND_LAYER, 100, 36);
		const MainMapPixelPosition position = MainMapViewMath::ScreenToMapPixel(0, 0, scrolled);
		CHECK(position.map_x == 3);
		CHECK(position.pixel_x == 4);
		CHECK(position.map_y == 1);
		CHECK(position.pixel_y == 4);

		const MainMapViewport upper_floor = MakeViewport(1.0, 1.0, 6);
		const MainMapPixelPosition shifted = MainMapViewMath::ScreenToMapPixel(0, 0, upper_floor);
		CHECK(shifted.map_x == 1);
		CHECK(shifted.map_y == 1);
		CHECK(shifted.pixel_x == 0);
		CHECK(shifted.pixel_y == 0);
	}

	void TestNegativeCoordinatesFloorDivision() {
		const MainMapViewport viewport = MakeViewport(1.0, 1.0, GROUND_LAYER, 0, 0);

		const MainMapPixelPosition neg_one = MainMapViewMath::ScreenToMapPixel(-1, -1, viewport);
		CHECK(neg_one.map_x == -1);
		CHECK(neg_one.map_y == -1);
		CHECK(neg_one.pixel_x == 31);
		CHECK(neg_one.pixel_y == 31);

		const MainMapPixelPosition neg_thirty_two = MainMapViewMath::ScreenToMapPixel(-32, -32, viewport);
		CHECK(neg_thirty_two.map_x == -1);
		CHECK(neg_thirty_two.map_y == -1);
		CHECK(neg_thirty_two.pixel_x == 0);
		CHECK(neg_thirty_two.pixel_y == 0);

		const MainMapPixelPosition neg_thirty_three = MainMapViewMath::ScreenToMapPixel(-33, -33, viewport);
		CHECK(neg_thirty_three.map_x == -2);
		CHECK(neg_thirty_three.map_y == -2);
		CHECK(neg_thirty_three.pixel_x == 31);
		CHECK(neg_thirty_three.pixel_y == 31);
	}

	void TestScreenToMapUsesSameTile() {
		const MainMapViewport viewport = MakeViewport(1.5, 1.25, 6, 120, 44);
		for (int screen_x = -50; screen_x < 200; screen_x += 7) {
			for (int screen_y = -50; screen_y < 200; screen_y += 11) {
				int map_x = 0;
				int map_y = 0;
				MainMapViewMath::ScreenToMap(screen_x, screen_y, viewport, &map_x, &map_y);
				const MainMapPixelPosition pixel = MainMapViewMath::ScreenToMapPixel(screen_x, screen_y, viewport);
				CHECK(map_x == pixel.map_x);
				CHECK(map_y == pixel.map_y);
				CHECK(pixel.pixel_x >= 0 && pixel.pixel_x < TILE_SIZE);
				CHECK(pixel.pixel_y >= 0 && pixel.pixel_y < TILE_SIZE);
			}
		}
	}

	SpriteLayoutMetrics MakeTwoByTwoMetrics() {
		SpriteLayoutMetrics metrics;
		metrics.num_columns = 2;
		metrics.num_rows = 2;
		metrics.column_widths[0] = 32;
		metrics.column_widths[1] = 32;
		metrics.row_heights[0] = 32;
		metrics.row_heights[1] = 32;
		metrics.total_width = 64;
		metrics.total_height = 64;
		return metrics;
	}

	void TestSelectionSpriteParts() {
		const SpriteLayoutMetrics metrics = MakeTwoByTwoMetrics();

		const auto top_left = FindSelectionSpritePart(0, 0, 0, 0, metrics);
		CHECK(top_left.has_value());
		CHECK(top_left->column == 0);
		CHECK(top_left->row == 0);
		CHECK(top_left->local_x == 0);
		CHECK(top_left->local_y == 0);

		const auto bottom_right = FindSelectionSpritePart(-1, -1, 0, 0, metrics);
		CHECK(bottom_right.has_value());
		CHECK(bottom_right->column == 1);
		CHECK(bottom_right->row == 1);
		CHECK(bottom_right->local_x == 31);
		CHECK(bottom_right->local_y == 31);

		const auto second_column = FindSelectionSpritePart(-32, 0, 0, 0, metrics);
		CHECK(second_column.has_value());
		CHECK(second_column->column == 1);
		CHECK(second_column->row == 0);
		CHECK(second_column->local_x == 0);

		CHECK(!FindSelectionSpritePart(-64, 0, 0, 0, metrics).has_value());
		CHECK(!FindSelectionSpritePart(32, 0, 0, 0, metrics).has_value());
	}

	void TestSelectionSpritePartsWithNegativeOrigin() {
		const SpriteLayoutMetrics metrics = MakeTwoByTwoMetrics();

		// A multi-tile item is drawn extending left/up from its storage tile.
		CHECK(!FindSelectionSpritePart(0, 0, -32, 0, metrics).has_value());

		const auto first_part = FindSelectionSpritePart(-1, 0, -32, 0, metrics);
		CHECK(first_part.has_value());
		CHECK(first_part->column == 0);
		CHECK(first_part->local_x == 31);

		const auto second_part = FindSelectionSpritePart(-32, 0, -32, 0, metrics);
		CHECK(second_part.has_value());
		CHECK(second_part->column == 0);
		CHECK(second_part->local_x == 0);

		const auto second_part_last = FindSelectionSpritePart(-33, 0, -32, 0, metrics);
		CHECK(second_part_last.has_value());
		CHECK(second_part_last->column == 1);
		CHECK(second_part_last->local_x == 31);

		const auto second_part_first = FindSelectionSpritePart(-64, 0, -32, 0, metrics);
		CHECK(second_part_first.has_value());
		CHECK(second_part_first->column == 1);
		CHECK(second_part_first->local_x == 0);

		CHECK(!FindSelectionSpritePart(-65, 0, -32, 0, metrics).has_value());
	}

	void TestSelectionSpritePartsWithVariableWidths() {
		SpriteLayoutMetrics metrics;
		metrics.num_columns = 2;
		metrics.num_rows = 1;
		metrics.column_widths[0] = 64;
		metrics.column_widths[1] = 32;
		metrics.row_heights[0] = 64;
		metrics.total_width = 96;
		metrics.total_height = 64;

		const auto wide_part = FindSelectionSpritePart(63, 0, 0, 0, metrics);
		CHECK(wide_part.has_value());
		CHECK(wide_part->column == 0);
		CHECK(wide_part->local_x == 63);

		const auto narrow_part = FindSelectionSpritePart(-33, 0, 0, 0, metrics);
		CHECK(narrow_part.has_value());
		CHECK(narrow_part->column == 1);
		CHECK(narrow_part->local_x == 31);

		const auto narrow_part_first = FindSelectionSpritePart(-64, 0, 0, 0, metrics);
		CHECK(narrow_part_first.has_value());
		CHECK(narrow_part_first->column == 1);
		CHECK(narrow_part_first->local_x == 0);
	}

} // namespace

int main() {
	TestFloorOffsets();
	TestScreenToMapPixelAtZoomOne();
	TestScreenToMapPixelAtOtherZooms();
	TestContentScale();
	TestScrollAndFloorOffset();
	TestNegativeCoordinatesFloorDivision();
	TestScreenToMapUsesSameTile();
	TestSelectionSpriteParts();
	TestSelectionSpritePartsWithNegativeOrigin();
	TestSelectionSpritePartsWithVariableWidths();

	std::cout << "[PASS] pixel-perfect selection geometry tests" << std::endl;
	return 0;
}
