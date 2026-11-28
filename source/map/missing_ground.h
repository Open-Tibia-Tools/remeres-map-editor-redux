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

    enum RoleFlags : uint16_t {
        kRoleNone = 0,
        kHasGround = 1u << 0,
        kOpenFloor = 1u << 1,
        kBarrier = 1u << 2,
        kHasWall = 1u << 3,
        kFunctional = 1u << 4,
        kHasSoftContent = 1u << 5,
        kHasStaticContent = 1u << 6,
        kIsTrim = 1u << 7,
    };

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
        uint16_t self, uint16_t n, uint16_t e, uint16_t s, uint16_t w, uint16_t se, uint16_t nw = kRoleNone, uint16_t ne = kRoleNone,
        uint16_t sw = kRoleNone
    ) noexcept
    {
        if ((self & kHasGround) != 0u) {
            return MissingGroundReason::None;
        }

        const auto countFlag = [&](uint16_t flag) noexcept -> int {
            return static_cast<int>((n & flag) != 0u) + static_cast<int>((e & flag) != 0u) + static_cast<int>((s & flag) != 0u)
                + static_cast<int>((w & flag) != 0u);
        };
        const auto isVoid = [](uint16_t nb) noexcept -> int { return static_cast<int>((nb & (kHasGround | kBarrier)) == 0u); };

        const int open_floors = countFlag(kOpenFloor);
        const int any_grounds = countFlag(kHasGround);
        const int voids = isVoid(n) + isVoid(e) + isVoid(s) + isVoid(w);
        const bool opposite_open_floors
            = ((n & kOpenFloor) != 0u && (s & kOpenFloor) != 0u) || ((e & kOpenFloor) != 0u && (w & kOpenFloor) != 0u);
        const bool opposite_any_grounds
            = ((n & kHasGround) != 0u && (s & kHasGround) != 0u) || ((e & kHasGround) != 0u && (w & kHasGround) != 0u);
        const bool occluded_25d = ((e | s | se) & kBarrier) != 0u;

        const bool not_void_edge = (voids == 0 && (open_floors >= 1 || any_grounds >= 2)) || opposite_open_floors || open_floors >= 3;

        if ((self & kFunctional) != 0u && (not_void_edge || open_floors >= 2)) {
            return MissingGroundReason::Functional;
        }

        const bool is_unblocked_surface = (self & (kHasWall | kBarrier | kIsTrim)) == 0u;
        if (is_unblocked_surface && (self & kHasSoftContent) != 0u && (not_void_edge || open_floors >= 2)) {
            return MissingGroundReason::MaskedByCarpetOrContent;
        }

        if (occluded_25d && is_unblocked_surface && (opposite_open_floors || open_floors >= 3 || (voids == 0 && open_floors >= 1))) {
            return MissingGroundReason::OccludedBehindBarrier;
        }

        if ((self & kHasWall) != 0u) {
            if (opposite_open_floors || open_floors >= 3) {
                return MissingGroundReason::InteriorWall;
            }
            const bool horiz_seam = (((n & kOpenFloor) != 0u && (s & kHasWall) != 0u) || ((s & kOpenFloor) != 0u && (n & kHasWall) != 0u))
                && (e & kHasWall) != 0u && (w & kHasWall) != 0u;
            const bool vert_seam = (((e & kOpenFloor) != 0u && (w & kHasWall) != 0u) || ((w & kOpenFloor) != 0u && (e & kHasWall) != 0u))
                && (n & kHasWall) != 0u && (s & kHasWall) != 0u;
            if (open_floors == 1 && (horiz_seam || vert_seam)) {
                return MissingGroundReason::None;
            }
            const auto isGroundedWallCorner
                = [](uint16_t wall_a, uint16_t wall_b, uint16_t ground_a, uint16_t ground_b, uint16_t diag) noexcept {
                      return (wall_a & kBarrier) != 0u && (wall_b & kBarrier) != 0u && (ground_a & kHasGround) != 0u
                          && (ground_b & kHasGround) != 0u && (diag & kHasGround) != 0u;
                  };
            const bool grounded_corner = isGroundedWallCorner(n, w, e, s, nw) || isGroundedWallCorner(n, e, w, s, ne)
                || isGroundedWallCorner(s, w, n, e, sw) || isGroundedWallCorner(s, e, n, w, se);
            if (any_grounds >= 3 || (voids == 0 && (opposite_any_grounds || grounded_corner))) {
                return MissingGroundReason::SealedPocket;
            }
            return MissingGroundReason::None;
        }

        if ((self & kBarrier) != 0u) {
            const auto hasGroundedWall = [](uint16_t nb) noexcept { return (nb & (kHasWall | kHasGround)) == (kHasWall | kHasGround); };
            const bool open_floor_opposite_grounded_wall = ((n & kOpenFloor) != 0u && hasGroundedWall(s))
                || ((s & kOpenFloor) != 0u && hasGroundedWall(n)) || ((e & kOpenFloor) != 0u && hasGroundedWall(w))
                || ((w & kOpenFloor) != 0u && hasGroundedWall(e));
            if (opposite_open_floors || open_floors >= 3 || any_grounds >= 3 || open_floor_opposite_grounded_wall) {
                return MissingGroundReason::SurroundedInterior;
            }
            return MissingGroundReason::None;
        }

        if ((self & kIsTrim) != 0u) {
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

        if ((self & kHasStaticContent) != 0u) {
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
        uint16_t self, uint16_t n, uint16_t e, uint16_t s, uint16_t w, uint16_t se, uint16_t nw = kRoleNone, uint16_t ne = kRoleNone,
        uint16_t sw = kRoleNone
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
