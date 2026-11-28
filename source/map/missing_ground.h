#ifndef RME_MAP_MISSING_GROUND_H_
#define RME_MAP_MISSING_GROUND_H_

#include <cstdint>
#include <string_view>

// "Show missing ground" & "Find Missing Grounds": specific domain rules catching
// ungrounded tiles that hide invisible holes/blocking traps while keeping
// exterior architecture (ledges, balcony brackets, cliff edges, sails) quiet:
// 1. Functional tiles (creatures, spawns, houses, PZ, zones, stairs, teleports, non-wall doors).
// 2. Walkable-looking / masked tiles (carpets, tables, moveable/pickupable items).
// 3. 2.5D occluded tiles behind E/S/SE walls or mountains along a corridor/pocket.
// 4. Interior walls, sealed pockets, and non-border static content surrounded by open floors.
namespace MissingGround {

    enum class RoleFlags : uint16_t {
        None = 0,
        HasGround = 1u << 0,
        OpenFloor = 1u << 1,
        Barrier = 1u << 2,
        HasWall = 1u << 3,
        Functional = 1u << 4,
        HasSoftContent = 1u << 5,
        HasStaticContent = 1u << 6,
        IsTrim = 1u << 7,
        IsBottomClip = 1u << 8,
    };

    [[nodiscard]] constexpr RoleFlags operator|(RoleFlags a, RoleFlags b) noexcept
    {
        return static_cast<RoleFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
    }

    [[nodiscard]] constexpr RoleFlags operator&(RoleFlags a, RoleFlags b) noexcept
    {
        return static_cast<RoleFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
    }

    constexpr RoleFlags& operator|=(RoleFlags& a, RoleFlags b) noexcept
    {
        a = a | b;
        return a;
    }

    [[nodiscard]] constexpr bool HasRole(RoleFlags mask, RoleFlags flag) noexcept
    {
        return (static_cast<uint16_t>(mask) & static_cast<uint16_t>(flag)) != 0u;
    }

    [[nodiscard]] constexpr bool HasAllRoles(RoleFlags mask, RoleFlags required) noexcept
    {
        return (mask & required) == required;
    }

    enum class MissingGroundReason : uint8_t {
        None = 0,
        Functional,
        MaskedByCarpetOrContent,
        OccludedBehindBarrier,
        InteriorWall,
        SealedPocket,
        SurroundedInterior,
    };

    [[nodiscard]] constexpr bool IsFlatBorder(
        bool is_border, bool is_always_on_bottom, int top_order, bool is_optional_border, bool is_blocking, bool blocks_missiles
    ) noexcept
    {
        if (is_blocking || blocks_missiles) {
            return false;
        }
        return is_border || (is_always_on_bottom && top_order == 1) || is_optional_border;
    }

    [[nodiscard]] constexpr MissingGroundReason EvaluateMissingGroundWithReason(
        RoleFlags self, RoleFlags n, RoleFlags e, RoleFlags s, RoleFlags w, RoleFlags se, RoleFlags nw = RoleFlags::None,
        RoleFlags ne = RoleFlags::None, RoleFlags sw = RoleFlags::None
    ) noexcept
    {
        if (HasRole(self, RoleFlags::HasGround)) {
            return MissingGroundReason::None;
        }

        const auto countFlag = [&](RoleFlags flag) noexcept -> int {
            return static_cast<int>(HasRole(n, flag)) + static_cast<int>(HasRole(e, flag)) + static_cast<int>(HasRole(s, flag))
                + static_cast<int>(HasRole(w, flag));
        };
        const auto isVoid
            = [](RoleFlags nb) noexcept -> int { return static_cast<int>(!HasRole(nb, RoleFlags::HasGround | RoleFlags::Barrier)); };

        const int open_floors = countFlag(RoleFlags::OpenFloor);
        const int any_grounds = countFlag(RoleFlags::HasGround);
        const int voids = isVoid(n) + isVoid(e) + isVoid(s) + isVoid(w);
        const bool opposite_open_floors = (HasRole(n, RoleFlags::OpenFloor) && HasRole(s, RoleFlags::OpenFloor))
            || (HasRole(e, RoleFlags::OpenFloor) && HasRole(w, RoleFlags::OpenFloor));
        const bool opposite_any_grounds = (HasRole(n, RoleFlags::HasGround) && HasRole(s, RoleFlags::HasGround))
            || (HasRole(e, RoleFlags::HasGround) && HasRole(w, RoleFlags::HasGround));
        const bool occluded_25d = HasRole(e | s | se, RoleFlags::Barrier);

        const bool not_void_edge = (voids == 0 && (open_floors >= 1 || any_grounds >= 2)) || opposite_open_floors || open_floors >= 3;

        if (HasRole(self, RoleFlags::Functional) && (not_void_edge || open_floors >= 2)) {
            return MissingGroundReason::Functional;
        }

        const bool is_unblocked_surface = !HasRole(self, RoleFlags::HasWall | RoleFlags::Barrier | RoleFlags::IsTrim);
        if (is_unblocked_surface
            && ((HasRole(self, RoleFlags::HasSoftContent) && (not_void_edge || open_floors >= 2))
                || (HasRole(self, RoleFlags::IsBottomClip) && not_void_edge))) {
            return MissingGroundReason::MaskedByCarpetOrContent;
        }

        if (occluded_25d && is_unblocked_surface && (opposite_open_floors || open_floors >= 3 || (voids == 0 && open_floors >= 1))) {
            return MissingGroundReason::OccludedBehindBarrier;
        }

        if (HasRole(self, RoleFlags::HasWall)) {
            if (opposite_open_floors || open_floors >= 3) {
                return MissingGroundReason::InteriorWall;
            }
            const bool horiz_seam = ((HasRole(n, RoleFlags::OpenFloor) && HasRole(s, RoleFlags::HasWall))
                                     || (HasRole(s, RoleFlags::OpenFloor) && HasRole(n, RoleFlags::HasWall)))
                && HasRole(e, RoleFlags::HasWall) && HasRole(w, RoleFlags::HasWall);
            const bool vert_seam = ((HasRole(e, RoleFlags::OpenFloor) && HasRole(w, RoleFlags::HasWall))
                                    || (HasRole(w, RoleFlags::OpenFloor) && HasRole(e, RoleFlags::HasWall)))
                && HasRole(n, RoleFlags::HasWall) && HasRole(s, RoleFlags::HasWall);
            if (open_floors == 1 && (horiz_seam || vert_seam)) {
                return MissingGroundReason::None;
            }
            const auto isGroundedWallCorner = [](RoleFlags wall_a, RoleFlags wall_b, RoleFlags ground_a, RoleFlags ground_b,
                                                 RoleFlags diag) noexcept {
                return HasRole(wall_a, RoleFlags::Barrier) && HasRole(wall_b, RoleFlags::Barrier) && HasRole(ground_a, RoleFlags::HasGround)
                    && HasRole(ground_b, RoleFlags::HasGround) && HasRole(diag, RoleFlags::HasGround);
            };
            const bool grounded_corner = isGroundedWallCorner(n, w, e, s, nw) || isGroundedWallCorner(n, e, w, s, ne)
                || isGroundedWallCorner(s, w, n, e, sw) || isGroundedWallCorner(s, e, n, w, se);
            if (any_grounds >= 3 || (voids == 0 && (opposite_any_grounds || grounded_corner))) {
                return MissingGroundReason::SealedPocket;
            }
            return MissingGroundReason::None;
        }

        if (HasRole(self, RoleFlags::Barrier)) {
            const auto hasGroundedWall = [](RoleFlags nb) noexcept { return HasAllRoles(nb, RoleFlags::HasWall | RoleFlags::HasGround); };
            const bool open_floor_opposite_grounded_wall = (HasRole(n, RoleFlags::OpenFloor) && hasGroundedWall(s))
                || (HasRole(s, RoleFlags::OpenFloor) && hasGroundedWall(n)) || (HasRole(e, RoleFlags::OpenFloor) && hasGroundedWall(w))
                || (HasRole(w, RoleFlags::OpenFloor) && hasGroundedWall(e));
            if (opposite_open_floors || open_floors >= 3 || any_grounds >= 3 || open_floor_opposite_grounded_wall) {
                return MissingGroundReason::SurroundedInterior;
            }
            return MissingGroundReason::None;
        }

        if (HasRole(self, RoleFlags::IsTrim)) {
            if (voids == 0) {
                if (open_floors >= 3) {
                    return MissingGroundReason::SurroundedInterior;
                }
                if (open_floors >= 2) {
                    return MissingGroundReason::SealedPocket;
                }
            }
            return MissingGroundReason::None;
        }

        if (HasRole(self, RoleFlags::HasStaticContent)) {
            if (open_floors >= 3) {
                return MissingGroundReason::SurroundedInterior;
            }
            if (voids == 0 && (opposite_any_grounds || any_grounds >= 3)) {
                return MissingGroundReason::SealedPocket;
            }
        }

        return MissingGroundReason::None;
    }

    [[nodiscard]] constexpr bool EvaluateMissingGround(
        RoleFlags self, RoleFlags n, RoleFlags e, RoleFlags s, RoleFlags w, RoleFlags se, RoleFlags nw = RoleFlags::None,
        RoleFlags ne = RoleFlags::None, RoleFlags sw = RoleFlags::None
    ) noexcept
    {
        return EvaluateMissingGroundWithReason(self, n, e, s, w, se, nw, ne, sw) != MissingGroundReason::None;
    }

    [[nodiscard]] constexpr std::string_view FormatReason(MissingGroundReason reason) noexcept
    {
        switch (reason) {
            case MissingGroundReason::Functional:
                return "functional tile without ground";
            case MissingGroundReason::MaskedByCarpetOrContent:
                return "masked by carpet/content";
            case MissingGroundReason::OccludedBehindBarrier:
                return "occluded behind wall/mountain";
            case MissingGroundReason::InteriorWall:
                return "interior wall without ground";
            case MissingGroundReason::SealedPocket:
                return "sealed pocket without ground";
            case MissingGroundReason::SurroundedInterior:
                return "surrounded interior without ground";
            case MissingGroundReason::None:
            default:
                return "missing ground";
        }
    }

} // namespace MissingGround

#endif
