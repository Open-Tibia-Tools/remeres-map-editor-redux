#include "rendering/core/chunk_tile_mask.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

namespace {
    std::vector<std::pair<int, int>> Collect(const ChunkTileMask& mask)
    {
        std::vector<std::pair<int, int>> tiles;
        mask.forEach([&](int tx, int ty) { tiles.emplace_back(tx, ty); });
        return tiles;
    }
} // namespace

TEST_CASE("an empty mask visits no tile", "[chunk_tile_mask]")
{
    const ChunkTileMask mask;
    REQUIRE_FALSE(mask.any());
    REQUIRE(Collect(mask).empty());
}

TEST_CASE("marked tiles are visited exactly once in row-major order", "[chunk_tile_mask]")
{
    ChunkTileMask mask;
    mask.mark(15, 15);
    mask.mark(3, 0);
    mask.mark(0, 4);
    mask.mark(15, 3);
    mask.mark(3, 0);

    REQUIRE(mask.any());
    const std::vector<std::pair<int, int>> expected {{3, 0}, {15, 3}, {0, 4}, {15, 15}};
    REQUIRE(Collect(mask) == expected);
    REQUIRE(mask.test(15, 3));
    REQUIRE(mask.test(0, 4));
    REQUIRE_FALSE(mask.test(0, 3));
    REQUIRE_FALSE(mask.test(14, 15));
}

TEST_CASE("every tile of the chunk maps to its own bit", "[chunk_tile_mask]")
{
    ChunkTileMask mask;
    for (int ty = 0; ty < ChunkTileMask::kSide; ++ty) {
        for (int tx = 0; tx < ChunkTileMask::kSide; ++tx) {
            mask.mark(tx, ty);
        }
    }
    const auto tiles = Collect(mask);
    REQUIRE(tiles.size() == 256);
    for (size_t i = 0; i < tiles.size(); ++i) {
        REQUIRE(tiles[i].first == static_cast<int>(i % 16));
        REQUIRE(tiles[i].second == static_cast<int>(i / 16));
    }
}

TEST_CASE("clear removes every tile", "[chunk_tile_mask]")
{
    ChunkTileMask mask;
    mask.mark(7, 9);
    mask.mark(0, 0);
    mask.clear();
    REQUIRE_FALSE(mask.any());
    REQUIRE(Collect(mask).empty());
}

TEST_CASE("out-of-bounds coordinates are ignored safely without corrupting valid bits", "[chunk_tile_mask]")
{
    CHECK(ChunkTileMask::inBounds(0, 0));
    CHECK(ChunkTileMask::inBounds(15, 15));
    CHECK_FALSE(ChunkTileMask::inBounds(-1, 0));
    CHECK_FALSE(ChunkTileMask::inBounds(0, -1));
    CHECK_FALSE(ChunkTileMask::inBounds(16, 0));
    CHECK_FALSE(ChunkTileMask::inBounds(0, 16));

    ChunkTileMask mask;
    mask.mark(-1, 0);
    mask.mark(0, -1);
    mask.mark(16, 0);
    mask.mark(0, 16);
    mask.mark(-100, 100);

    REQUIRE_FALSE(mask.any());
    REQUIRE(Collect(mask).empty());
    REQUIRE_FALSE(mask.test(-1, 0));
    REQUIRE_FALSE(mask.test(0, -1));
    REQUIRE_FALSE(mask.test(16, 0));
    REQUIRE_FALSE(mask.test(0, 16));
}
