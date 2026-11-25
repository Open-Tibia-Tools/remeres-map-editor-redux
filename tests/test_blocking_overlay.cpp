#include "rendering/drawers/overlays/blocking_overlay_rules.h"
#include "ui/icons/sprite_icon_bounds.h"
#include "item_definitions/formats/protobuf/protobuf_shift_bounds.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

using namespace rme::rendering;

TEST_CASE("ComputeBlockingBorderFlags emits borders only on exposed edges", "[blocking_overlay]")
{
    REQUIRE(ComputeBlockingBorderFlags({}) == 0u);

    const uint32_t all_edges = ComputeBlockingBorderFlags({
        .north = true,
        .south = true,
        .west = true,
        .east = true,
    });
    REQUIRE(all_edges == (ZONE_FLAG_BLOCK_BORDER_N | ZONE_FLAG_BLOCK_BORDER_S | ZONE_FLAG_BLOCK_BORDER_W | ZONE_FLAG_BLOCK_BORDER_E));

    const uint32_t corner_nw = ComputeBlockingBorderFlags({
        .north = true,
        .south = false,
        .west = true,
        .east = false,
    });
    REQUIRE(corner_nw == (ZONE_FLAG_BLOCK_BORDER_N | ZONE_FLAG_BLOCK_BORDER_W));
}

TEST_CASE("ComputeBlockingTileRenderFlags and MakeBlockingBorderPassFlags preserve blend/border modes", "[blocking_overlay]")
{
    const uint32_t edges = ZONE_FLAG_BLOCK_BORDER_N | ZONE_FLAG_BLOCK_BORDER_E;

    const uint32_t alpha_flags = ComputeBlockingTileRenderFlags(edges, 0);
    REQUIRE(alpha_flags == (ZONE_FLAG_BLOCKING | edges));
    REQUIRE((alpha_flags & ZONE_FLAG_MULTIPLICATIVE) == 0u);

    const uint32_t mult_flags = ComputeBlockingTileRenderFlags(edges, 1);
    REQUIRE(mult_flags == (ZONE_FLAG_BLOCKING | ZONE_FLAG_MULTIPLICATIVE | edges));

    const uint32_t border_pass = MakeBlockingBorderPassFlags(mult_flags);
    REQUIRE((border_pass & ZONE_FLAG_MULTIPLICATIVE) == 0u);
    REQUIRE((border_pass & ZONE_FLAG_BORDER_PASS) != 0u);
    REQUIRE((border_pass & (ZONE_FLAG_BLOCKING | edges)) == (ZONE_FLAG_BLOCKING | edges));
}

TEST_CASE("FillBlockingSpan clears exact subranges without out-of-bounds writes", "[blocking_overlay]")
{
    std::array<uint8_t, 6> row {1, 1, 1, 1, 1, 1};

    FillBlockingSpan(row, 10, 11, 13, 0);
    REQUIRE(row == std::array<uint8_t, 6> {1, 0, 0, 0, 1, 1});

    // Span wider than row bounds is clipped safely.
    FillBlockingSpan(row, 10, 5, 20, 2);
    REQUIRE(row == std::array<uint8_t, 6> {2, 2, 2, 2, 2, 2});

    // Empty / inverted span leaves buffer untouched.
    FillBlockingSpan(row, 10, 14, 12, 9);
    REQUIRE(row == std::array<uint8_t, 6> {2, 2, 2, 2, 2, 2});
}

TEST_CASE("ComputeBoundedIconDimension rejects invalid sizes and caps extreme shift growth", "[sprite_icon_bounds]")
{
    REQUIRE(ComputeBoundedIconDimension(0, 32) == 0);
    REQUIRE(ComputeBoundedIconDimension(32, -4) == 0);
    REQUIRE(ComputeBoundedIconDimension(32, 64) == 64);
    REQUIRE(ComputeBoundedIconDimension(70000, 32) == kMaxSpriteIconDimension);
    REQUIRE(ComputeBoundedIconDimension(64, 99999, 512) == 512);
}

TEST_CASE("ClampProtobufShiftToUint16 preserves valid offsets and clamps overflow", "[protobuf_shift_bounds]")
{
    const auto small = ClampProtobufShiftToUint16(64);
    REQUIRE(small.value == 64);
    REQUIRE_FALSE(small.clamped);

    const auto exact_max = ClampProtobufShiftToUint16(std::numeric_limits<uint16_t>::max());
    REQUIRE(exact_max.value == std::numeric_limits<uint16_t>::max());
    REQUIRE_FALSE(exact_max.clamped);

    const auto overflow = ClampProtobufShiftToUint16(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1);
    REQUIRE(overflow.value == std::numeric_limits<uint16_t>::max());
    REQUIRE(overflow.clamped);
}
