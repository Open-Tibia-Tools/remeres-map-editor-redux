#include "map/missing_ground_tile.h"

#include "game/item.h"
#include "map/tile.h"

namespace MissingGround {

    namespace {

        [[nodiscard]] constexpr bool HasFlagMask(uint64_t mask, ItemFlag flag) noexcept
        {
            return (mask & flagMask(flag)) != 0;
        }

    } // namespace

    RoleFlags ClassifyNeighborRoles(const Tile* tile) noexcept
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
            if (HasFlagMask(flags, ItemFlag::IsWall)) {
                mask |= RoleFlags::HasWall | RoleFlags::Barrier;
                break;
            }
            if (HasFlagMask(flags, ItemFlag::BlockMissiles) && HasFlagMask(flags, ItemFlag::Unpassable)) {
                mask |= RoleFlags::Barrier;
            }
        }

        if (tile->ground) {
            const auto ground_def = tile->ground->getDefinition();
            const uint64_t ground_flags = ground_def.flagMask();
            const bool ground_walkable = !HasFlagMask(ground_flags, ItemFlag::Unpassable)
                && !HasFlagMask(ground_flags, ItemFlag::BlockPathfinder) && !ground_def.isTrashHolder();
            if (ground_walkable && !HasRole(mask, RoleFlags::HasWall | RoleFlags::Barrier)) {
                mask |= RoleFlags::OpenFloor;
            }
        }

        return mask;
    }

    RoleFlags ClassifyTile(const Tile* tile) noexcept
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
            const bool is_blocking = HasFlagMask(flags, ItemFlag::Unpassable);
            const bool blocks_missiles = HasFlagMask(flags, ItemFlag::BlockMissiles);
            const bool is_wall = HasFlagMask(flags, ItemFlag::IsWall);
            const bool is_border = HasFlagMask(flags, ItemFlag::IsBorder);
            const bool is_always_on_bottom = HasFlagMask(flags, ItemFlag::AlwaysOnBottom);
            const bool is_optional_border = HasFlagMask(flags, ItemFlag::IsOptionalBorder);
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

            const bool is_functional_item = HasFlagMask(flags, ItemFlag::FloorChange) || def.isTeleport() || (def.isDoor() && !is_wall);
            if (is_functional_item) {
                mask |= RoleFlags::Functional;
            }

            const bool is_soft_item = HasFlagMask(flags, ItemFlag::IsCarpet) || HasFlagMask(flags, ItemFlag::IsTable)
                || HasFlagMask(flags, ItemFlag::Moveable) || HasFlagMask(flags, ItemFlag::Pickupable);
            if (is_soft_item) {
                mask |= RoleFlags::HasSoftContent;
            }

            if (!is_flat_border && !is_wall && !blocks_missiles && !is_functional_item && !is_soft_item) {
                if (is_blocking) {
                    has_unpassable_trim = true;
                } else {
                    mask |= RoleFlags::HasStaticContent;
                }
            } else if (is_flat_border && !is_border && !is_optional_border && !is_wall && !is_functional_item && !is_soft_item) {
                mask |= RoleFlags::IsBottomClip;
            }
        }

        // Outer walls of houses/zones resting over exterior void shouldn't bypass edge checks.
        if (has_house_or_zone && !HasRole(mask, RoleFlags::HasWall | RoleFlags::IsTrim) && !has_unpassable_trim) {
            mask |= RoleFlags::Functional;
        }

        if (tile->ground) {
            const auto ground_def = tile->ground->getDefinition();
            const uint64_t ground_flags = ground_def.flagMask();
            const bool ground_walkable = !HasFlagMask(ground_flags, ItemFlag::Unpassable)
                && !HasFlagMask(ground_flags, ItemFlag::BlockPathfinder) && !ground_def.isTrashHolder();
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
