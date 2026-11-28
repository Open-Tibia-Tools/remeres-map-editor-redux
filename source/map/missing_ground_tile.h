#ifndef RME_MAP_MISSING_GROUND_TILE_H_
#define RME_MAP_MISSING_GROUND_TILE_H_

#include "game/item.h"
#include "map/missing_ground.h"
#include "map/tile.h"

namespace MissingGround {

    namespace detail {

        [[nodiscard]] constexpr bool HasFlagMask(uint64_t mask, ItemFlag flag) noexcept
        {
            return (mask & flagMask(flag)) != 0;
        }

    } // namespace detail

    /**
     * Fast neighbor-only classification returning the subset of RoleFlags inspectable by
     * EvaluateMissingGroundWithReason on adjacent tiles (HasGround, OpenFloor, HasWall, Barrier).
     * Reads each item definition once and short-circuits once HasWall and Barrier are both known.
     */
    [[nodiscard]] inline RoleFlags ClassifyNeighborRoles(const Tile* tile) noexcept
    {
        if (!tile) {
            return RoleFlags::None;
        }

        RoleFlags mask = tile->ground ? RoleFlags::HasGround : RoleFlags::None;
        for (const auto& item : tile->items) {
            if (!item) {
                continue;
            }
            const auto def = item->getDefinition();
            const uint64_t flags = def.flagMask();
            if (detail::HasFlagMask(flags, ItemFlag::IsWall)) {
                mask |= RoleFlags::HasWall | RoleFlags::Barrier;
                break;
            }
            if (detail::HasFlagMask(flags, ItemFlag::BlockMissiles) && detail::HasFlagMask(flags, ItemFlag::Unpassable)) {
                mask |= RoleFlags::Barrier;
            }
        }

        if (tile->ground) {
            const auto ground_def = tile->ground->getDefinition();
            const uint64_t ground_flags = ground_def.flagMask();
            const bool ground_walkable = !detail::HasFlagMask(ground_flags, ItemFlag::Unpassable)
                && !detail::HasFlagMask(ground_flags, ItemFlag::BlockPathfinder) && !ground_def.isTrashHolder();
            if (ground_walkable && !HasRole(mask, RoleFlags::HasWall | RoleFlags::Barrier)) {
                mask |= RoleFlags::OpenFloor;
            }
        }

        return mask;
    }

    /** Classifies a tile into compact RoleFlags bitmask for zero-allocation overlay and search checks. */
    [[nodiscard]] inline RoleFlags ClassifyTile(const Tile* tile) noexcept
    {
        if (!tile) {
            return RoleFlags::None;
        }

        RoleFlags mask = RoleFlags::None;
        if (tile->ground) {
            mask |= RoleFlags::HasGround;
        }

        if (tile->creature != nullptr || tile->spawn != nullptr || tile->isHouseExit() || tile->isTownExit()) {
            mask |= RoleFlags::Functional;
        }

        const bool has_house_or_zone = tile->isHouseTile() || tile->isPZ() || tile->hasZones();

        bool has_unpassable_trim = false;
        for (const auto& item : tile->items) {
            if (!item) {
                continue;
            }

            const auto def = item->getDefinition();
            const uint64_t flags = def.flagMask();
            const bool is_blocking = detail::HasFlagMask(flags, ItemFlag::Unpassable);
            const bool blocks_missiles = detail::HasFlagMask(flags, ItemFlag::BlockMissiles);
            const bool is_wall = detail::HasFlagMask(flags, ItemFlag::IsWall);
            const bool is_border = detail::HasFlagMask(flags, ItemFlag::IsBorder);
            const bool is_always_on_bottom = detail::HasFlagMask(flags, ItemFlag::AlwaysOnBottom);
            const bool is_optional_border = detail::HasFlagMask(flags, ItemFlag::IsOptionalBorder);
            // Lazy attribute lookup only for non-blocking non-border always-on-bottom candidates.
            const int top_order = (!is_blocking && !blocks_missiles && !is_border && is_always_on_bottom && !is_optional_border)
                ? static_cast<int>(def.attribute(ItemAttributeKey::AlwaysOnTopOrder))
                : 0;
            const bool is_flat_border
                = IsFlatBorder(is_border, is_always_on_bottom, top_order, is_optional_border, is_blocking, blocks_missiles);

            if (is_wall) {
                mask |= RoleFlags::HasWall | RoleFlags::Barrier;
            } else if (blocks_missiles && is_blocking) {
                mask |= RoleFlags::Barrier;
            }

            const bool is_functional_item
                = detail::HasFlagMask(flags, ItemFlag::FloorChange) || def.isTeleport() || (def.isDoor() && !is_wall);
            if (is_functional_item) {
                mask |= RoleFlags::Functional;
            }

            const bool is_soft_item = detail::HasFlagMask(flags, ItemFlag::IsCarpet) || detail::HasFlagMask(flags, ItemFlag::IsTable)
                || detail::HasFlagMask(flags, ItemFlag::Moveable) || detail::HasFlagMask(flags, ItemFlag::Pickupable);
            if (is_soft_item) {
                mask |= RoleFlags::HasSoftContent;
            }

            if (!is_flat_border && !is_wall && !blocks_missiles && !is_functional_item && !is_soft_item) {
                if (is_blocking) {
                    has_unpassable_trim = true;
                } else {
                    mask |= RoleFlags::HasStaticContent;
                }
            }
        }

        // Outer walls of houses/zones resting over exterior void shouldn't bypass edge checks.
        if (has_house_or_zone && !HasRole(mask, RoleFlags::HasWall | RoleFlags::IsTrim) && !has_unpassable_trim) {
            mask |= RoleFlags::Functional;
        }

        if (tile->ground) {
            const auto ground_def = tile->ground->getDefinition();
            const uint64_t ground_flags = ground_def.flagMask();
            const bool ground_walkable = !detail::HasFlagMask(ground_flags, ItemFlag::Unpassable)
                && !detail::HasFlagMask(ground_flags, ItemFlag::BlockPathfinder) && !ground_def.isTrashHolder();
            if (ground_walkable && !HasRole(mask, RoleFlags::HasWall | RoleFlags::Barrier)) {
                mask |= RoleFlags::OpenFloor;
            }
        }

        if (has_unpassable_trim
            && !HasRole(mask, RoleFlags::HasWall | RoleFlags::Barrier | RoleFlags::Functional | RoleFlags::HasSoftContent)) {
            mask |= RoleFlags::IsTrim;
        }

        return mask;
    }

} // namespace MissingGround

#endif
