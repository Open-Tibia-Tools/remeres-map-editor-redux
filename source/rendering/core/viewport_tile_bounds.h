//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_VIEWPORT_TILE_BOUNDS_H_
#define RME_RENDERING_CORE_VIEWPORT_TILE_BOUNDS_H_

#include <cmath>

struct ViewportTileBounds {
    int start_x = 0;
    int start_y = 0;
    int end_x = -1;
    int end_y = -1;
};

/**
 * @brief Computes inclusive tile bounds visible in a floor viewport using its shifted screen origin.
 *
 * Unlike scroll-only bounds, upper floors are drawn offset by (GROUND_LAYER - z) * tile_size, so
 * callers that emit vertices in floor-local screen coordinates must invert base_screen_x/y directly.
 */
[[nodiscard]] inline ViewportTileBounds ComputeViewportTileBounds(
    int base_screen_x, int base_screen_y, float logical_width, float logical_height, int tile_size = 32, int extra_margin_tiles = 1
) noexcept
{
    if (logical_width <= 0.0f || logical_height <= 0.0f || tile_size <= 0) {
        return ViewportTileBounds {};
    }

    const int margin = extra_margin_tiles > 0 ? extra_margin_tiles : 0;
    const double inv_tile_size = 1.0 / static_cast<double>(tile_size);

    ViewportTileBounds bounds;
    bounds.start_x = static_cast<int>(std::floor(-static_cast<double>(base_screen_x) * inv_tile_size)) - margin;
    bounds.start_y = static_cast<int>(std::floor(-static_cast<double>(base_screen_y) * inv_tile_size)) - margin;
    bounds.end_x
        = static_cast<int>(std::floor((static_cast<double>(logical_width) - static_cast<double>(base_screen_x)) * inv_tile_size)) + margin;
    bounds.end_y
        = static_cast<int>(std::floor((static_cast<double>(logical_height) - static_cast<double>(base_screen_y)) * inv_tile_size)) + margin;
    return bounds;
}

/**
 * @brief Computes inclusive visible tile bounds for map_z from viewport scroll and camera floor.
 */
[[nodiscard]] inline ViewportTileBounds ComputeFloorViewportTileBounds(
    int map_z, int view_scroll_x, int view_scroll_y, int camera_floor, float logical_width, float logical_height, int ground_layer = 7,
    int tile_size = 32, int extra_margin_tiles = 1
) noexcept
{
    const int offset = (map_z <= ground_layer) ? (ground_layer - map_z) * tile_size : (camera_floor - map_z) * tile_size;
    return ComputeViewportTileBounds(
        -view_scroll_x - offset, -view_scroll_y - offset, logical_width, logical_height, tile_size, extra_margin_tiles
    );
}

#endif // RME_RENDERING_CORE_VIEWPORT_TILE_BOUNDS_H_
