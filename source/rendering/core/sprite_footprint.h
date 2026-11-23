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
#include <optional>
#include <span>

namespace rme::rendering {

// Alpha above which a texel counts as visible for the sprite part footprint.
inline constexpr uint8_t kSpriteFootprintAlphaThreshold = 32;

static_assert(MAX_SPRITE_PARTS <= 16, "SpritePartFootprint stores one 16-bit column mask per row");

// Union over all frames, patterns and layers of which parts of a composite sprite draw at least
// one visible texel. Part (column, row) is drawn up-left of the anchor, at base - sum(previous
// column widths / row heights), exactly like the composite render path lays it out: part (0, 0)
// is the anchor tile, part (columns - 1, rows - 1) the farthest up-left.
struct SpritePartFootprint {
	uint8_t columns = 0;
	uint8_t rows = 0;
	std::array<uint16_t, MAX_SPRITE_PARTS> opaque_columns_by_row {};

	[[nodiscard]] constexpr bool isOpaque(int column, int row) const noexcept {
		if (column < 0 || row < 0 || column >= static_cast<int>(columns) || row >= static_cast<int>(rows)) {
			return false;
		}
		return (opaque_columns_by_row[static_cast<size_t>(row)] >> column) & 1u;
	}

	constexpr void markOpaque(int column, int row) noexcept {
		if (column < 0 || row < 0 || column >= static_cast<int>(columns) || row >= static_cast<int>(rows)) {
			return;
		}
		opaque_columns_by_row[static_cast<size_t>(row)] |= static_cast<uint16_t>(1u << column);
	}
};

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

	const size_t part_width = static_cast<size_t>(width);
	const size_t part_height = static_cast<size_t>(height);
	for (size_t index = 0; index < image_count; ++index) {
		const int column = static_cast<int>(index % part_width);
		const int row = static_cast<int>((index / part_width) % part_height);
		if (column >= footprint.columns || row >= footprint.rows) {
			continue;
		}
		if (!footprint.isOpaque(column, row) && source.isOpaque(index)) {
			footprint.markOpaque(column, row);
		}
	}
	return footprint;
}

// One part of a composite sprite, in the column/row layout of SpritePartFootprint.
struct SpritePart {
	int column = 0;
	int row = 0;

	friend constexpr bool operator==(const SpritePart&, const SpritePart&) = default;
};

// "Flatten Multi-Tile Ground" rule: a multi-part ground whose anchor part draws nothing but whose
// farthest (top-left) part does leaves its own tile without any pixel of that ground (mountain
// 919 is the motivating case). Returns that top-left part so the renderer can draw it a second
// time on the anchor tile; nullopt for every other sprite. Geometry only, no item ids.
[[nodiscard]] constexpr std::optional<SpritePart> FlattenGroundPart(bool is_ground, const SpritePartFootprint& footprint) noexcept {
	if (!is_ground || (footprint.columns <= 1 && footprint.rows <= 1)) {
		return std::nullopt;
	}
	const SpritePart top_left { .column = footprint.columns - 1, .row = footprint.rows - 1 };
	if (footprint.isOpaque(0, 0) || !footprint.isOpaque(top_left.column, top_left.row)) {
		return std::nullopt;
	}
	return top_left;
}

} // namespace rme::rendering

#endif
