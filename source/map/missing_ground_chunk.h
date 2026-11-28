#ifndef RME_MAP_MISSING_GROUND_CHUNK_H_
#define RME_MAP_MISSING_GROUND_CHUNK_H_

#include "map/basemap.h"
#include "map/missing_ground.h"
#include "map/missing_ground_tile.h"
#include "map/spatial_hash_grid.h"
#include "rendering/core/chunk_tile_mask.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace MissingGround {

    inline constexpr int kChunkSide = ChunkTileMask::kSide; // 16
    inline constexpr int kPaddedChunkStride = kChunkSide + 2; // 18

    /**
     * Evaluates all 16x16 tiles of chunk (cx, cy, z) against a reused 18x18 RoleFlags scratch
     * grid (1-tile perimeter halo clamped to [0, map_width) x [0, map_height) when positive).
     * Returns true when any tile in the chunk requires a missing-ground indicator.
     */
    [[nodiscard]] inline bool EvaluateChunk(
        const BaseMap& map, int32_t cx, int32_t cy, int32_t z, int map_width, int map_height, ChunkTileMask& out_mask,
        std::vector<RoleFlags>& scratch_roles
    )
    {
        out_mask.clear();
        if (z < 0 || z >= MAP_LAYERS) {
            return false;
        }

        const int base_x = cx * kChunkSide;
        const int base_y = cy * kChunkSide;
        const bool has_map_bounds = map_width > 0 && map_height > 0;
        if (has_map_bounds) {
            if (base_x + kChunkSide <= 0 || base_y + kChunkSide <= 0 || base_x >= map_width || base_y >= map_height) {
                return false;
            }
        } else if (base_x + kChunkSide <= 0 || base_y + kChunkSide <= 0) {
            return false;
        }

        scratch_roles.resize(static_cast<size_t>(kPaddedChunkStride * kPaddedChunkStride));

        const int min_x = base_x - 1;
        const int max_x = base_x + kChunkSide;
        const int min_y = base_y - 1;
        const int max_y = base_y + kChunkSide;

        SpatialHashGrid::CellCursor cursor;
        for (int ry = min_y; ry <= max_y; ++ry) {
            RoleFlags* row = scratch_roles.data() + static_cast<size_t>(ry - min_y) * kPaddedChunkStride;
            if (ry < 0 || (has_map_bounds && ry >= map_height)) {
                std::fill(row, row + kPaddedChunkStride, RoleFlags::None);
                continue;
            }

            const bool is_interior_row = (ry >= base_y && ry < base_y + kChunkSide);
            int x = min_x;
            if (x < 0) {
                const int neg_end = std::min(max_x, -1);
                std::fill(row, row + (neg_end - min_x + 1), RoleFlags::None);
                x = neg_end + 1;
            }

            const int scan_max_x = has_map_bounds ? std::min(max_x, map_width - 1) : max_x;
            const int ly = ry & 3;
            while (x <= scan_max_x) {
                const Floor* floor = map.getFloor(x, ry, z, cursor);
                if (!floor) {
                    const int span_end = std::min(scan_max_x, x | cursor.empty_span_mask);
                    std::fill(row + (x - min_x), row + (span_end - min_x + 1), RoleFlags::None);
                    x = span_end + 1;
                    continue;
                }
                const int node_end = std::min(scan_max_x, x | 3);
                for (; x <= node_end; ++x) {
                    const Tile* t = floor->locs[((x & 3) << 2) + ly].get();
                    if (is_interior_row && x >= base_x && x < base_x + kChunkSide && t != nullptr && !t->ground) {
                        row[x - min_x] = ClassifyTile(t);
                    } else {
                        row[x - min_x] = ClassifyNeighborRoles(t);
                    }
                }
            }

            if (x <= max_x) {
                std::fill(row + (x - min_x), row + kPaddedChunkStride, RoleFlags::None);
            }
        }

        const int local_min_x = std::max(0, -base_x);
        const int local_max_x = has_map_bounds ? std::min(kChunkSide - 1, map_width - 1 - base_x) : (kChunkSide - 1);
        const int local_min_y = std::max(0, -base_y);
        const int local_max_y = has_map_bounds ? std::min(kChunkSide - 1, map_height - 1 - base_y) : (kChunkSide - 1);

        bool any_marked = false;
        for (int ty = local_min_y; ty <= local_max_y; ++ty) {
            const RoleFlags* prev = scratch_roles.data() + static_cast<size_t>(ty) * kPaddedChunkStride;
            const RoleFlags* curr = prev + kPaddedChunkStride;
            const RoleFlags* next = curr + kPaddedChunkStride;

            for (int tx = local_min_x; tx <= local_max_x; ++tx) {
                const int idx = tx + 1;
                const RoleFlags cell = curr[idx];
                if (HasRole(cell, RoleFlags::HasGround)) {
                    continue;
                }
                if (EvaluateMissingGround(
                        cell, prev[idx], curr[idx + 1], next[idx], curr[idx - 1], next[idx + 1], prev[idx - 1], prev[idx + 1], next[idx - 1]
                    )) {
                    out_mask.mark(tx, ty);
                    any_marked = true;
                }
            }
        }

        return any_marked;
    }

} // namespace MissingGround

#endif
