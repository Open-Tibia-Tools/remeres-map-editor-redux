#ifndef RME_MAP_MISSING_GROUND_CHUNK_H_
#define RME_MAP_MISSING_GROUND_CHUNK_H_

#include "map/missing_ground.h"

#include <array>
#include <cstdint>
#include <vector>

class BaseMap;

namespace MissingGround {

    inline constexpr int kChunkSide = 16;
    inline constexpr int kPaddedChunkStride = kChunkSide + 2;

    /**
     * Evaluates all 16x16 tiles of chunk (cx, cy, z) against a reused 18x18 RoleFlags scratch
     * grid (1-tile perimeter halo clamped to [0, map_width) x [0, map_height) when positive),
     * writing 256 tile bits into out_mask_words (4 x 64-bit row-major words).
     * Returns true when any tile in the chunk requires a missing-ground indicator.
     */
    [[nodiscard]] bool EvaluateChunk(
        const BaseMap& map, int32_t cx, int32_t cy, int32_t z, int map_width, int map_height, std::array<uint64_t, 4>& out_mask_words,
        std::vector<RoleFlags>& scratch_roles
    );

} // namespace MissingGround

#endif
