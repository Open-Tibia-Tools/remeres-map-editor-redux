#ifndef RME_MAP_MISSING_GROUND_TILE_H_
#define RME_MAP_MISSING_GROUND_TILE_H_

#include "map/missing_ground.h"

class Tile;

namespace MissingGround {

    /**
     * Fast neighbor-only classification returning the subset of RoleFlags inspectable by
     * EvaluateMissingGroundWithReason on adjacent tiles (HasGround, OpenFloor, HasWall, Barrier).
     */
    [[nodiscard]] RoleFlags ClassifyNeighborRoles(const Tile* tile) noexcept;

    /** Classifies a tile into compact RoleFlags bitmask for zero-allocation overlay and search checks. */
    [[nodiscard]] RoleFlags ClassifyTile(const Tile* tile) noexcept;

} // namespace MissingGround

#endif
