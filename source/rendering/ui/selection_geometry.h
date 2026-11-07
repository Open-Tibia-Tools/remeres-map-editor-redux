#ifndef RME_RENDERING_UI_SELECTION_GEOMETRY_H_
#define RME_RENDERING_UI_SELECTION_GEOMETRY_H_

#include "rendering/core/sprite_layout_calculator.h"

#include <optional>

struct SelectionSpritePart {
	int column = 0;
	int row = 0;
	int local_x = 0;
	int local_y = 0;
};

inline std::optional<SelectionSpritePart> FindSelectionSpritePart(int pixel_x, int pixel_y, int base_x, int base_y, const SpriteLayoutMetrics& metrics) {
	int x_offset = 0;
	for (int column = 0; column < metrics.num_columns; ++column) {
		int y_offset = 0;
		for (int row = 0; row < metrics.num_rows; ++row) {
			const int part_x = base_x - x_offset;
			const int part_y = base_y - y_offset;
			if (pixel_x >= part_x && pixel_x < part_x + metrics.column_widths[column] &&
				pixel_y >= part_y && pixel_y < part_y + metrics.row_heights[row]) {
				return SelectionSpritePart {
					.column = column,
					.row = row,
					.local_x = pixel_x - part_x,
					.local_y = pixel_y - part_y,
				};
			}
			y_offset += metrics.row_heights[row];
		}
		x_offset += metrics.column_widths[column];
	}
	return std::nullopt;
}

#endif
