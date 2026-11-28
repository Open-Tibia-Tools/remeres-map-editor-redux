//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAWERS_OVERLAYS_BLOCKING_OVERLAY_RULES_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_BLOCKING_OVERLAY_RULES_H_

#include "rendering/indicators/zone_flags.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rme::rendering {

    struct BlockingNeighbors {
        bool north = false;
        bool south = false;
        bool west = false;
        bool east = false;
    };

    [[nodiscard]] constexpr uint32_t ComputeBlockingBorderFlags(const BlockingNeighbors& neighbors) noexcept
    {
        uint32_t flags = 0;
        if (neighbors.north) {
            flags |= ZONE_FLAG_BLOCK_BORDER_N;
        }
        if (neighbors.south) {
            flags |= ZONE_FLAG_BLOCK_BORDER_S;
        }
        if (neighbors.west) {
            flags |= ZONE_FLAG_BLOCK_BORDER_W;
        }
        if (neighbors.east) {
            flags |= ZONE_FLAG_BLOCK_BORDER_E;
        }
        return flags;
    }

    [[nodiscard]] constexpr uint32_t ComputeBlockingTileRenderFlags(uint32_t border_flags, int blend_mode) noexcept
    {
        uint32_t flags = ZONE_FLAG_BLOCKING | border_flags;
        if (blend_mode == 1) {
            flags |= ZONE_FLAG_MULTIPLICATIVE;
        }
        return flags;
    }

    [[nodiscard]] constexpr uint32_t MakeBlockingBorderPassFlags(uint32_t tile_zone_flags) noexcept
    {
        return (tile_zone_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
    }

    inline void FillBlockingSpan(std::span<uint8_t> row, int row_start_x, int span_start_x, int span_end_x, uint8_t value) noexcept
    {
        if (row.empty() || span_end_x < span_start_x) {
            return;
        }
        const ptrdiff_t row_size = static_cast<ptrdiff_t>(row.size());
        const ptrdiff_t offset_start = std::max<ptrdiff_t>(0, static_cast<ptrdiff_t>(span_start_x) - static_cast<ptrdiff_t>(row_start_x));
        const ptrdiff_t offset_end
            = std::min<ptrdiff_t>(row_size, static_cast<ptrdiff_t>(span_end_x) - static_cast<ptrdiff_t>(row_start_x) + 1);
        if (offset_start >= offset_end) {
            return;
        }
        std::ranges::fill(row.subspan(static_cast<size_t>(offset_start), static_cast<size_t>(offset_end - offset_start)), value);
    }

} // namespace rme::rendering

#endif
