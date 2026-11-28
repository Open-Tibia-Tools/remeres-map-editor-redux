#ifndef RME_MAP_MISSING_GROUND_TILE_H_
#define RME_MAP_MISSING_GROUND_TILE_H_

#include "game/item.h"
#include "map/missing_ground.h"
#include "map/tile.h"

namespace MissingGround {

    /** Classifies a tile into compact RoleFlags bitmask for zero-allocation overlay and search checks. */
    [[nodiscard]] inline uint16_t ClassifyTile(const Tile* tile) noexcept
    {
        if (!tile) {
            return kRoleNone;
        }

        uint16_t mask = kRoleNone;
        if (tile->ground) {
            mask |= kHasGround;
        }

        if (tile->creature != nullptr || tile->spawn != nullptr || tile->isHouseExit() || tile->isTownExit()) {
            mask |= kFunctional;
        }

        const bool has_house_or_zone = tile->isHouseTile() || tile->isPZ() || tile->hasZones();

        bool has_unpassable_trim = false;
        for (const auto& item : tile->items) {
            if (!item) {
                continue;
            }

            const auto def = item->getDefinition();
            const bool is_blocking = item->isBlocking();
            const bool blocks_missiles = def.hasFlag(ItemFlag::BlockMissiles);
            const bool is_wall = item->isWall();
            const bool is_flat_border = IsFlatBorder(
                item->isBorder(), item->isAlwaysOnBottom(), item->getTopOrder(), item->isOptionalBorder(), is_blocking, blocks_missiles
            );

            if (is_wall) {
                mask |= kHasWall | kBarrier;
            } else if (!is_flat_border && blocks_missiles && is_blocking) {
                mask |= kBarrier;
            }

            const bool is_functional_item = def.isFloorChange() || item->isTeleport() || (def.isDoor() && !is_wall);
            if (is_functional_item) {
                mask |= kFunctional;
            }

            const bool is_soft_item
                = def.hasFlag(ItemFlag::IsCarpet) || def.hasFlag(ItemFlag::IsTable) || item->isMoveable() || item->isPickupable();
            if (is_soft_item) {
                mask |= kHasSoftContent;
            }

            if (is_blocking && !is_flat_border && !is_wall && !blocks_missiles && !is_functional_item && !is_soft_item) {
                has_unpassable_trim = true;
            } else if (!is_flat_border && !is_wall && !blocks_missiles && !is_functional_item && !is_soft_item) {
                mask |= kHasStaticContent;
            }
        }

        // Outer walls of houses/zones resting over exterior void shouldn't bypass edge checks.
        if (has_house_or_zone && (mask & (kHasWall | kIsTrim)) == 0u && !has_unpassable_trim) {
            mask |= kFunctional;
        }

        if (tile->ground) {
            const auto ground_def = tile->ground->getDefinition();
            const bool ground_walkable
                = !tile->ground->isBlocking() && !ground_def.isTrashHolder() && !ground_def.hasFlag(ItemFlag::BlockPathfinder);
            if (ground_walkable && (mask & (kHasWall | kBarrier)) == 0u) {
                mask |= kOpenFloor;
            }
        }

        if (has_unpassable_trim && (mask & (kHasWall | kBarrier | kFunctional | kHasSoftContent)) == 0u) {
            mask |= kIsTrim;
        }

        return mask;
    }

} // namespace MissingGround

#endif
