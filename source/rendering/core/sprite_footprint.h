//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_SPRITE_FOOTPRINT_H_
#define RME_RENDERING_CORE_SPRITE_FOOTPRINT_H_

#include "app/definitions.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rme::rendering {

// Alpha above which a texel counts as visible for the sprite part footprint.
inline constexpr uint8_t kSpriteFootprintAlphaThreshold = 32;

// Largest part the atlas accepts and a client may encode in DAT/protobuf data.
inline constexpr int kMaxSpritePartPixels = 64;

// How many tiles a footprint can reach up/left of its anchor: MAX_SPRITE_PARTS parts
// of up to 64 px plus one tile of draw offset. Callers scanning for anchors pad the
// scanned area right/down by this much; marks beyond it are clipped. Elevation is
// not part of the bound: an elevation larger than one tile can still clip at the
// right/bottom edge of the viewport.
inline constexpr int kSpriteFootprintReachTiles = (static_cast<int>(MAX_SPRITE_PARTS) * kMaxSpritePartPixels + TILE_SIZE) / TILE_SIZE + 1;

static_assert(MAX_SPRITE_PARTS <= 16, "SpritePartFootprint stores one 16-bit column mask per row");

// Union geometry of a composite sprite over all frames, patterns and layers: the size of every
// part column/row and which parts draw at least one visible texel. Part (column, row) is drawn
// up-left of the anchor, at base - sum(previous column widths / row heights), exactly like the
// composite render path (forEachSpriteRegion) lays it out.
struct SpritePartFootprint {
	uint8_t columns = 0;
	uint8_t rows = 0;
	std::array<int, MAX_SPRITE_PARTS> column_widths {};
	std::array<int, MAX_SPRITE_PARTS> row_heights {};
	std::array<uint16_t, MAX_SPRITE_PARTS> opaque_columns_by_row {};

	[[nodiscard]] bool isOpaque(int column, int row) const noexcept {
		if (column < 0 || row < 0 || column >= static_cast<int>(columns) || row >= static_cast<int>(rows)) {
			return false;
		}
		return (opaque_columns_by_row[static_cast<size_t>(row)] >> column) & 1u;
	}

	void markOpaque(int column, int row) noexcept {
		if (column < 0 || row < 0 || column >= static_cast<int>(columns) || row >= static_cast<int>(rows)) {
			return;
		}
		opaque_columns_by_row[static_cast<size_t>(row)] |= static_cast<uint16_t>(1u << column);
	}
};

// Whether a sprite can draw outside its anchor tile and therefore needs a footprint
// for the pathing overlay. Composite dimensions cover sprites whose width/height is
// one but whose image is 64 px (drawn one tile up-left).
[[nodiscard]] constexpr bool NeedsSpriteFootprint(int width, int height, int composite_width, int composite_height) noexcept {
	return width > 1 || height > 1 || composite_width > TILE_SIZE || composite_height > TILE_SIZE;
}

// True when any RGBA texel (4 bytes per pixel) has alpha above `threshold`.
[[nodiscard]] inline bool HasOpaqueTexel(std::span<const uint8_t> rgba, uint8_t threshold) noexcept {
	for (size_t offset = 3; offset < rgba.size(); offset += 4) {
		if (rgba[offset] > threshold) {
			return true;
		}
	}
	return false;
}

// Builds the union footprint of a `width` x `height` composite sprite from its `image_count`
// images. Image index i belongs to part (i % width, (i / width) % height), which is the part
// layout of GameSprite::getIndex(), so iterating every image covers all frames, patterns and
// layers. `source` is the alpha seam:
//   int width(size_t index), int height(size_t index): pixel size of image `index`
//   bool isOpaque(size_t index): image `index` has a visible texel
// isOpaque() is only asked for parts not already known to be opaque.
template <typename ImageSource>
[[nodiscard]] SpritePartFootprint BuildSpritePartFootprint(int width, int height, size_t image_count, ImageSource& source) {
	SpritePartFootprint footprint;
	if (width <= 0 || height <= 0) {
		return footprint;
	}

	footprint.columns = static_cast<uint8_t>(std::min<int>(width, static_cast<int>(MAX_SPRITE_PARTS)));
	footprint.rows = static_cast<uint8_t>(std::min<int>(height, static_cast<int>(MAX_SPRITE_PARTS)));
	std::fill_n(footprint.column_widths.begin(), footprint.columns, TILE_SIZE);
	std::fill_n(footprint.row_heights.begin(), footprint.rows, TILE_SIZE);

	const size_t part_width = static_cast<size_t>(width);
	const size_t part_height = static_cast<size_t>(height);
	for (size_t index = 0; index < image_count; ++index) {
		const int column = static_cast<int>(index % part_width);
		const int row = static_cast<int>((index / part_width) % part_height);
		if (column >= footprint.columns || row >= footprint.rows) {
			continue;
		}

		// Read the pixels first: decoding them can update the image's dimensions.
		const bool newly_opaque = !footprint.isOpaque(column, row) && source.isOpaque(index);
		footprint.column_widths[column] = std::max(footprint.column_widths[column], source.width(index));
		footprint.row_heights[row] = std::max(footprint.row_heights[row], source.height(index));
		if (newly_opaque) {
			footprint.markOpaque(column, row);
		}
	}
	return footprint;
}

// Floor division for negative pixel coordinates (divisor > 0).
[[nodiscard]] constexpr int FloorDivide(int value, int divisor) noexcept {
	const int quotient = value / divisor;
	return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
}

// Visits the tile offsets (relative to the anchor tile) covered by the opaque parts of
// `footprint` when its anchor part is drawn at pixel (`base_x`, `base_y`) of the anchor tile
// (base = -elevation - draw offset). A part covers the tiles whose centre it draws over, so a
// few pixels of draw offset do not spill onto neighbours while a 64 px part covers two tiles per
// axis. A part shifted by exactly half a tile counts toward the anchor (down/right). Overlapping
// parts may visit the same offset more than once.
template <typename Visitor>
void ForEachSpriteFootprintTile(const SpritePartFootprint& footprint, int base_x, int base_y, Visitor&& visit) {
	constexpr int kHalfTile = TILE_SIZE / 2;
	int x_offset = 0;
	for (int column = 0; column < footprint.columns; ++column) {
		const int part_left = base_x - x_offset;
		// Tile t is covered when part_left < t * TILE_SIZE + kHalfTile <= part_left + width.
		const int first_tile_x = FloorDivide(part_left - kHalfTile, TILE_SIZE) + 1;
		const int last_tile_x = FloorDivide(part_left + footprint.column_widths[column] - kHalfTile, TILE_SIZE);
		int y_offset = 0;
		for (int row = 0; row < footprint.rows; ++row) {
			if (footprint.isOpaque(column, row)) {
				const int part_top = base_y - y_offset;
				const int first_tile_y = FloorDivide(part_top - kHalfTile, TILE_SIZE) + 1;
				const int last_tile_y = FloorDivide(part_top + footprint.row_heights[row] - kHalfTile, TILE_SIZE);
				for (int tile_y = first_tile_y; tile_y <= last_tile_y; ++tile_y) {
					for (int tile_x = first_tile_x; tile_x <= last_tile_x; ++tile_x) {
						visit(tile_x, tile_y);
					}
				}
			}
			y_offset += footprint.row_heights[row];
		}
		x_offset += footprint.column_widths[column];
	}
}

} // namespace rme::rendering

#endif
