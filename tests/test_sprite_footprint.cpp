#include "rendering/core/sprite_footprint.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

// The pathing overlay washes the anchor tile of a blocking item plus the tiles returned by
// ForEachSpriteFootprintTile(). These cases pin the opaque-part mask (union over frames,
// patterns and layers) and the part -> tile offset mapping on synthetic composite sprites.

using rme::rendering::BuildSpritePartFootprint;
using rme::rendering::FloorDivide;
using rme::rendering::ForEachSpriteFootprintTile;
using rme::rendering::HasOpaqueTexel;
using rme::rendering::kSpriteFootprintAlphaThreshold;
using rme::rendering::kSpriteFootprintReachTiles;
using rme::rendering::NeedsSpriteFootprint;
using rme::rendering::SpritePartFootprint;

namespace {

	using TileSet = std::set<std::pair<int, int>>;

	struct SpriteShape {
		int width = 1;
		int height = 1;
		int layers = 1;
		int pattern_x = 1;
		int pattern_y = 1;
		int pattern_z = 1;
		int frames = 1;

		[[nodiscard]] size_t imageCount() const {
			return static_cast<size_t>(width * height * layers * pattern_x * pattern_y * pattern_z * frames);
		}

		// Same image order as GameSprite::getIndex().
		[[nodiscard]] size_t index(int column, int row, int layer = 0, int px = 0, int py = 0, int pz = 0, int frame = 0) const {
			size_t idx = static_cast<size_t>(frame);
			idx = idx * pattern_z + pz;
			idx = idx * pattern_y + py;
			idx = idx * pattern_x + px;
			idx = idx * layers + layer;
			idx = idx * height + row;
			idx = idx * width + column;
			return idx;
		}
	};

	// Fake alpha seam: per-image opacity and size, counting how often pixels are inspected.
	struct FakeImageSource {
		std::vector<bool> opaque;
		std::vector<int> widths;
		std::vector<int> heights;
		int opacity_queries = 0;

		explicit FakeImageSource(size_t count) :
			opaque(count, false), widths(count, TILE_SIZE), heights(count, TILE_SIZE) { }

		int width(size_t index) const {
			return widths[index];
		}
		int height(size_t index) const {
			return heights[index];
		}
		bool isOpaque(size_t index) {
			++opacity_queries;
			return opaque[index];
		}
	};

	SpritePartFootprint Build(const SpriteShape& shape, FakeImageSource& source) {
		return BuildSpritePartFootprint(shape.width, shape.height, shape.imageCount(), source);
	}

	TileSet FootprintTiles(const SpritePartFootprint& footprint, int base_x = 0, int base_y = 0) {
		TileSet tiles;
		ForEachSpriteFootprintTile(footprint, base_x, base_y, [&](int dx, int dy) {
			tiles.insert({ dx, dy });
		});
		return tiles;
	}

	// Footprint washed by the overlay: the logical anchor tile plus the opaque parts.
	TileSet WashedTiles(const SpritePartFootprint& footprint, int base_x = 0, int base_y = 0) {
		TileSet tiles = FootprintTiles(footprint, base_x, base_y);
		tiles.insert({ 0, 0 });
		return tiles;
	}

	SpritePartFootprint FullyOpaque(int width, int height) {
		const SpriteShape shape { .width = width, .height = height };
		FakeImageSource source(shape.imageCount());
		source.opaque.assign(shape.imageCount(), true);
		return Build(shape, source);
	}

	SpritePartFootprint FullyOpaqueSized(int width, int height, int part_pixels) {
		const SpriteShape shape { .width = width, .height = height };
		FakeImageSource source(shape.imageCount());
		source.opaque.assign(shape.imageCount(), true);
		source.widths.assign(shape.imageCount(), part_pixels);
		source.heights.assign(shape.imageCount(), part_pixels);
		return Build(shape, source);
	}

} // namespace

TEST_CASE("2x2 sprite with only the up-left part opaque washes anchor and that part (919 shape)", "[footprint]") {
	// Mountain 919: 2x2 with 4x4 patterns; every pattern has an image only for part (1,1).
	const SpriteShape shape { .width = 2, .height = 2, .pattern_x = 4, .pattern_y = 4 };
	FakeImageSource source(shape.imageCount());
	for (int py = 0; py < 4; ++py) {
		for (int px = 0; px < 4; ++px) {
			source.opaque[shape.index(1, 1, 0, px, py)] = true;
		}
	}

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(footprint.isOpaque(1, 1));
	REQUIRE_FALSE(footprint.isOpaque(0, 0));
	REQUIRE_FALSE(footprint.isOpaque(1, 0));
	REQUIRE_FALSE(footprint.isOpaque(0, 1));

	REQUIRE(FootprintTiles(footprint) == TileSet { { -1, -1 } });
	REQUIRE(WashedTiles(footprint) == TileSet { { 0, 0 }, { -1, -1 } });
}

TEST_CASE("fully opaque 2x2 sprite washes all four tiles", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaque(2, 2);
	REQUIRE(WashedTiles(footprint) == TileSet { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } });
}

TEST_CASE("2x2 sprite opaque only on the anchor part washes the anchor alone", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 2 };
	FakeImageSource source(shape.imageCount());
	source.opaque[shape.index(0, 0)] = true;

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(FootprintTiles(footprint) == TileSet { { 0, 0 } });
	REQUIRE(WashedTiles(footprint) == TileSet { { 0, 0 } });
}

TEST_CASE("fully transparent 2x2 sprite adds nothing beyond the anchor", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 2 };
	FakeImageSource source(shape.imageCount());

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(FootprintTiles(footprint).empty());
	REQUIRE(WashedTiles(footprint) == TileSet { { 0, 0 } });
}

TEST_CASE("1x1 sprite footprint is the anchor tile", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaque(1, 1);
	REQUIRE(footprint.columns == 1);
	REQUIRE(footprint.rows == 1);
	REQUIRE(FootprintTiles(footprint) == TileSet { { 0, 0 } });
	REQUIRE(FootprintTiles(footprint, -8, -8) == TileSet { { 0, 0 } });
}

TEST_CASE("draw offset moves the footprint with the drawn parts", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaque(2, 2);
	const TileSet unshifted { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } };

	// A few pixels of offset keep every part on its tile.
	REQUIRE(FootprintTiles(footprint, -8, -8) == unshifted);
	// Exactly half a tile counts toward the anchor.
	REQUIRE(FootprintTiles(footprint, -16, -16) == unshifted);
	// More than half a tile moves the parts onto the next tiles up-left.
	REQUIRE(FootprintTiles(footprint, -24, -24) == TileSet { { -1, -1 }, { -2, -1 }, { -1, -2 }, { -2, -2 } });
	// A full tile of offset on one axis only.
	REQUIRE(FootprintTiles(footprint, -32, 0) == TileSet { { -1, 0 }, { -2, 0 }, { -1, -1 }, { -2, -1 } });
}

TEST_CASE("919 shape with draw offset follows the opaque part", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 2 };
	FakeImageSource source(shape.imageCount());
	source.opaque[shape.index(1, 1)] = true;

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(FootprintTiles(footprint, -8, -8) == TileSet { { -1, -1 } });
	REQUIRE(FootprintTiles(footprint, -32, -32) == TileSet { { -2, -2 } });
}

TEST_CASE("3x3 sprite maps every opaque part to its own tile", "[footprint]") {
	const SpriteShape shape { .width = 3, .height = 3 };
	FakeImageSource source(shape.imageCount());
	source.opaque[shape.index(2, 0)] = true;
	source.opaque[shape.index(1, 1)] = true;
	source.opaque[shape.index(0, 2)] = true;

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(WashedTiles(footprint) == TileSet { { 0, 0 }, { -2, 0 }, { -1, -1 }, { 0, -2 } });
}

TEST_CASE("16x16 sprite covers the maximum footprint within the reach", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaque(16, 16);
	const TileSet tiles = FootprintTiles(footprint);
	REQUIRE(tiles.size() == 256);
	REQUIRE(tiles.begin()->first == -15);
	REQUIRE(tiles.rbegin()->first == 0);

	// Draw offset and elevation of up to one tile still stay within the scan padding.
	const TileSet shifted = FootprintTiles(footprint, -32, -32);
	int min_x = 0;
	int min_y = 0;
	for (const auto& [x, y] : shifted) {
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
	}
	REQUIRE(min_x >= -kSpriteFootprintReachTiles);
	REQUIRE(min_y >= -kSpriteFootprintReachTiles);
}

TEST_CASE("a 1x1 sprite with a 64 px image covers two by two tiles", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaqueSized(1, 1, 64);
	REQUIRE(NeedsSpriteFootprint(1, 1, 64, 64));
	REQUIRE(FootprintTiles(footprint, -32, -32) == TileSet { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } });
}

TEST_CASE("a normal 1x1 sprite still only needs its anchor tile", "[footprint]") {
	REQUIRE_FALSE(NeedsSpriteFootprint(1, 1, TILE_SIZE, TILE_SIZE));
	const SpritePartFootprint footprint = FullyOpaqueSized(1, 1, TILE_SIZE);
	REQUIRE(FootprintTiles(footprint) == TileSet { { 0, 0 } });
}

TEST_CASE("sprite footprint eligibility follows width, height and composite size", "[footprint]") {
	REQUIRE(NeedsSpriteFootprint(2, 1, TILE_SIZE, TILE_SIZE));
	REQUIRE(NeedsSpriteFootprint(1, 2, TILE_SIZE, TILE_SIZE));
	REQUIRE(NeedsSpriteFootprint(1, 1, TILE_SIZE, TILE_SIZE) == false);
	REQUIRE(NeedsSpriteFootprint(1, 1, TILE_SIZE, TILE_SIZE * 2));
}

TEST_CASE("a 16x16 sprite with 64 px parts stays within the scan reach", "[footprint]") {
	const SpritePartFootprint footprint = FullyOpaqueSized(16, 16, 64);
	const TileSet tiles = FootprintTiles(footprint, -32, -32);
	REQUIRE(tiles.size() == 1024);

	int min_x = 0;
	int min_y = 0;
	for (const auto& [x, y] : tiles) {
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
	}
	REQUIRE(min_x == -31);
	REQUIRE(min_y == -31);
	REQUIRE(min_x >= -kSpriteFootprintReachTiles);
	REQUIRE(min_y >= -kSpriteFootprintReachTiles);
}

TEST_CASE("footprint opacity accessors reject out-of-range columns and rows", "[footprint]") {
	SpritePartFootprint footprint;
	footprint.columns = 2;
	footprint.rows = 2;
	footprint.markOpaque(1, 1);
	REQUIRE(footprint.isOpaque(1, 1));
	REQUIRE_FALSE(footprint.isOpaque(-1, 0));
	REQUIRE_FALSE(footprint.isOpaque(0, -1));
	REQUIRE_FALSE(footprint.isOpaque(2, 0));
	REQUIRE_FALSE(footprint.isOpaque(0, 2));
	footprint.markOpaque(-1, 0);
	footprint.markOpaque(0, -1);
	footprint.markOpaque(2, 0);
	footprint.markOpaque(0, 2);
	REQUIRE_FALSE(footprint.isOpaque(-1, 0));
	REQUIRE_FALSE(footprint.isOpaque(2, 0));
}

TEST_CASE("sprites wider than the part limit are clipped like the layout", "[footprint]") {
	const SpriteShape shape { .width = 20, .height = 1 };
	FakeImageSource source(shape.imageCount());
	source.opaque.assign(shape.imageCount(), true);

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(footprint.columns == MAX_SPRITE_PARTS);
	REQUIRE(FootprintTiles(footprint).size() == MAX_SPRITE_PARTS);
}

TEST_CASE("opaque-part mask is the union over frames, patterns and layers", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 2, .layers = 2, .pattern_x = 2, .frames = 3 };
	FakeImageSource source(shape.imageCount());
	source.opaque[shape.index(0, 1, 1, 0, 0, 0, 2)] = true; // part (0,1): second layer, last frame only
	source.opaque[shape.index(1, 0, 0, 1, 0, 0, 0)] = true; // part (1,0): second pattern only

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(footprint.isOpaque(0, 1));
	REQUIRE(footprint.isOpaque(1, 0));
	REQUIRE_FALSE(footprint.isOpaque(0, 0));
	REQUIRE_FALSE(footprint.isOpaque(1, 1));
}

TEST_CASE("opaque parts stop further pixel inspection of that part", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 1, .frames = 4 };
	FakeImageSource source(shape.imageCount());
	source.opaque.assign(shape.imageCount(), true);

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(footprint.isOpaque(0, 0));
	REQUIRE(footprint.isOpaque(1, 0));
	REQUIRE(source.opacity_queries == 2);
}

TEST_CASE("part sizes are the maximum over all images", "[footprint]") {
	const SpriteShape shape { .width = 2, .height = 1, .frames = 2 };
	FakeImageSource source(shape.imageCount());
	source.opaque.assign(shape.imageCount(), true);
	source.widths[shape.index(1, 0, 0, 0, 0, 0, 1)] = 64;

	const SpritePartFootprint footprint = Build(shape, source);
	REQUIRE(footprint.column_widths[0] == TILE_SIZE);
	REQUIRE(footprint.column_widths[1] == 64);
	REQUIRE(footprint.row_heights[0] == TILE_SIZE);

	// Base compensates the 64 px part like GameSprite::getDrawOffset(); the wide part spans two tiles.
	REQUIRE(FootprintTiles(footprint, -32, 0) == TileSet { { -1, 0 }, { -2, 0 } });
}

TEST_CASE("empty sprite dimensions produce an empty footprint", "[footprint]") {
	FakeImageSource source(0);
	const SpritePartFootprint footprint = BuildSpritePartFootprint(0, 0, 0, source);
	REQUIRE(footprint.columns == 0);
	REQUIRE(footprint.rows == 0);
	REQUIRE(FootprintTiles(footprint).empty());
}

TEST_CASE("opaque texel test uses the alpha channel above the threshold", "[footprint]") {
	std::array<uint8_t, 8> rgba {};
	REQUIRE_FALSE(HasOpaqueTexel(rgba, kSpriteFootprintAlphaThreshold));

	rgba[0] = 255;
	rgba[1] = 255;
	rgba[2] = 255;
	REQUIRE_FALSE(HasOpaqueTexel(rgba, kSpriteFootprintAlphaThreshold));

	rgba[7] = kSpriteFootprintAlphaThreshold;
	REQUIRE_FALSE(HasOpaqueTexel(rgba, kSpriteFootprintAlphaThreshold));

	rgba[7] = kSpriteFootprintAlphaThreshold + 1;
	REQUIRE(HasOpaqueTexel(rgba, kSpriteFootprintAlphaThreshold));
}

TEST_CASE("floor division rounds toward negative infinity", "[footprint]") {
	REQUIRE(FloorDivide(0, TILE_SIZE) == 0);
	REQUIRE(FloorDivide(31, TILE_SIZE) == 0);
	REQUIRE(FloorDivide(32, TILE_SIZE) == 1);
	REQUIRE(FloorDivide(-1, TILE_SIZE) == -1);
	REQUIRE(FloorDivide(-32, TILE_SIZE) == -1);
	REQUIRE(FloorDivide(-33, TILE_SIZE) == -2);
}
