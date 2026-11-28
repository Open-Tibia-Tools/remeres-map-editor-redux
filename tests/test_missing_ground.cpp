#include "map/missing_ground.h"
#include "rendering/core/viewport_tile_bounds.h"
#include "rendering/indicators/technical_item_registry.h"
#include "rendering/indicators/zone_flags.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

using namespace MissingGround;
using rme::rendering::GetIndicatorBadgeStyle;
using rme::rendering::GetIndicatorTypeFromZoneFlags;
using rme::rendering::MakeIndicatorZoneFlag;
using rme::rendering::TechnicalItemRegistry;
using rme::rendering::TileIndicatorType;
using rme::rendering::ZONE_FLAG_BLOCKING;
using rme::rendering::ZONE_FLAG_HOUSE_GROUND;
using rme::rendering::ZONE_FLAG_INDICATOR_MASK;

namespace {

    constexpr uint16_t kOpen = kHasGround | kOpenFloor;
    constexpr uint16_t kWallTile = kHasWall | kBarrier;
    constexpr uint16_t kHiddenGroundWall = kHasGround | kHasWall | kBarrier;
    constexpr uint16_t kMountain = kBarrier;
    constexpr uint16_t kTrimProp = kIsTrim;
    constexpr uint16_t kBorderOnly = kRoleNone;
    constexpr uint16_t kCarpet = kHasSoftContent;
    constexpr uint16_t kStaticProp = kHasStaticContent;
    constexpr uint16_t kSail = kHasGround; // Non-walkable pseudo-ground (4958 sail)

    template <size_t Rows>
    struct RoleGrid {
        std::array<std::string_view, Rows> rows;

        [[nodiscard]] uint16_t at(int x, int y) const
        {
            if (y < 0 || y >= static_cast<int>(Rows) || x < 0 || x >= static_cast<int>(rows[y].size())) {
                return kRoleNone;
            }
            switch (rows[y][x]) {
                case '#':
                    return kOpen;
                case 'W':
                    return kWallTile;
                case 'G':
                    return kHiddenGroundWall;
                case 'M':
                    return kMountain;
                case 'T':
                    return kTrimProp;
                case 'b':
                case 'B':
                    return kBorderOnly;
                case 'C':
                    return kCarpet;
                case 'O':
                    return kStaticProp;
                case 'S':
                    return kSail;
                case 'F':
                    return kFunctional;
                default:
                    return kRoleNone;
            }
        }

        [[nodiscard]] MissingGroundReason eval(int x, int y) const
        {
            return EvaluateMissingGroundWithReason(
                at(x, y), at(x, y - 1), at(x + 1, y), at(x, y + 1), at(x - 1, y), at(x + 1, y + 1), at(x - 1, y - 1), at(x + 1, y - 1),
                at(x - 1, y + 1)
            );
        }
    };

} // namespace

TEST_CASE("IsFlatBorder excludes blocking and sight-blocking mountain borders", "[missing_ground]")
{
    REQUIRE(IsFlatBorder(true, false, 0, false, false, false));
    REQUIRE(IsFlatBorder(false, true, 1, false, false, false));
    REQUIRE(IsFlatBorder(false, false, 0, true, false, false));

    // Blocking mountain inner border (e.g. 873: IsBorder + AlwaysOnBottom + topOrder=2 + Unpassable + BlockMissiles)
    REQUIRE_FALSE(IsFlatBorder(true, true, 2, false, true, true));
    REQUIRE_FALSE(IsFlatBorder(true, true, 1, false, true, false));
    REQUIRE_FALSE(IsFlatBorder(true, false, 0, false, false, true));
    REQUIRE_FALSE(IsFlatBorder(false, true, 2, false, false, false));
}

TEST_CASE("Proposal 1 rejects exterior architecture false positives", "[missing_ground]")
{
    // FP #1: Exterior stone ledge row (T) south of castle wall with hidden walkable ground (G) and north of street (#)
    const RoleGrid<3> castle_ledge {{
        "GGGGG",
        ".TTTT",
        "#####",
    }};
    for (int x = 1; x <= 4; ++x) {
        REQUIRE(castle_ledge.eval(x, 1) == MissingGroundReason::None);
    }

    // FP #2: 1-tile air gap with wooden bracket (T) between two walkable balconies (#) next to house wall (G)
    const RoleGrid<3> balcony_gap {{
        "G#.",
        "GT.",
        "G#.",
    }};
    REQUIRE(balcony_gap.eval(1, 1) == MissingGroundReason::None);

    // FP #3: Outer ice/mountain border edge with cliffs/trim over black void
    const RoleGrid<3> outer_edge {{
        "##.",
        "Tb.",
        "...",
    }};
    REQUIRE(outer_edge.eval(0, 1) == MissingGroundReason::None);
    REQUIRE(outer_edge.eval(1, 1) == MissingGroundReason::None);

    // FP #4: Exterior unpassable sail ground (S) next to rope/wall
    const RoleGrid<3> ship_edge {{
        ".S.",
        ".W.",
        ".S.",
    }};
    REQUIRE(ship_edge.eval(1, 1) == MissingGroundReason::None);

    // Exterior N/W building perimeter (empty outside wall) stays quiet
    const RoleGrid<3> outer_building {{
        "...",
        ".WW",
        ".W#",
    }};
    REQUIRE(outer_building.eval(1, 0) == MissingGroundReason::None);
    REQUIRE(outer_building.eval(0, 1) == MissingGroundReason::None);

    // Exterior diagonal corner adjacent to wall at SE with E+S floors stays quiet
    const RoleGrid<3> diag_corner {{
        ".#.",
        "#W#",
        ".#.",
    }};
    REQUIRE(diag_corner.eval(0, 0) == MissingGroundReason::None);

    // Bridge/platform edge or igloo overhang touching void stays quiet even with functional/multi-border/soft flags
    const RoleGrid<3> bridge_and_island_edges {{
        "#..",
        "BF#",
        ".C#",
    }};
    REQUIRE(bridge_and_island_edges.eval(0, 1) == MissingGroundReason::None); // multi-border with 1 open floor & 3 voids
    REQUIRE(bridge_and_island_edges.eval(1, 1) == MissingGroundReason::None); // functional with 1 open floor & 3 voids
    REQUIRE(bridge_and_island_edges.eval(1, 2) == MissingGroundReason::None); // soft item with 1 open floor & 3 voids

    // Zigzag diagonal mountain/cave cliff corner (N+W ground, E+S groundless cliff borders, SE air) stays quiet
    const RoleGrid<3> zigzag_cliff {{
        "###",
        "#MM",
        "#M.",
    }};
    REQUIRE(zigzag_cliff.eval(1, 1) == MissingGroundReason::None);

    // Adjacent back-to-back building walls (parallel wall seam with only 1 open floor) stay quiet
    const RoleGrid<4> horiz_building_seam {{
        "###",
        "GGG",
        "WWW",
        "###",
    }};
    REQUIRE(horiz_building_seam.eval(1, 2) == MissingGroundReason::None);

    const RoleGrid<3> vert_building_seam {{
        "#GW#",
        "#GW#",
        "#GW#",
    }};
    REQUIRE(vert_building_seam.eval(2, 1) == MissingGroundReason::None);

    // Exterior diagonal/octagonal wall elbow resting on ungrounded outer walls over void stays quiet
    const RoleGrid<3> outer_wall_elbow {{
        ".W#",
        "WW#",
        "###",
    }};
    REQUIRE(outer_wall_elbow.eval(1, 1) == MissingGroundReason::None);
    // Intentional 1x1 and 1x2 cave holes ringed solely by borders stay quiet.
    const RoleGrid<4> bordered_cave_holes {{
        "####",
        "#B##",
        "##B#",
        "##B#",
    }};
    REQUIRE(bordered_cave_holes.eval(1, 1) == MissingGroundReason::None);
    REQUIRE(bordered_cave_holes.eval(2, 2) == MissingGroundReason::None);
    REQUIRE(bordered_cave_holes.eval(2, 3) == MissingGroundReason::None);
}

TEST_CASE("Proposal 1 catches hidden occluded holes, carpet/content traps, interior walls, and sealed pockets", "[missing_ground]")
{
    // Rule 1: Functional tile without ground next to >= 2 open floors (interior/cutout, not void edge)
    const RoleGrid<3> functional_grid {{
        "...",
        "#F#",
        "...",
    }};
    REQUIRE(functional_grid.eval(1, 1) == MissingGroundReason::Functional);

    // Rule 2: Carpet or soft content masking void along open floor
    const RoleGrid<3> carpet_edge {{
        "...",
        "#C#",
        "...",
    }};
    REQUIRE(carpet_edge.eval(1, 1) == MissingGroundReason::MaskedByCarpetOrContent);

    // Static non-border prop surrounded by open floors still triggers SurroundedInterior
    const RoleGrid<3> surrounded_static_prop {{
        "###",
        "#O#",
        "###",
    }};
    REQUIRE(surrounded_static_prop.eval(1, 1) == MissingGroundReason::SurroundedInterior);

    // Rule 3: Empty tile occluded behind S/E/SE mountain or wall along a corridor
    const RoleGrid<3> mountain_corridor {{
        "MMM",
        "#.#",
        "MMM",
    }};
    REQUIRE(mountain_corridor.eval(1, 1) == MissingGroundReason::OccludedBehindBarrier);

    // Unoccluded empty tile surrounded by open floor stays quiet (no accidental empty voids)
    const RoleGrid<3> unoccluded_empty {{
        "###",
        "#.#",
        "###",
    }};
    REQUIRE(unoccluded_empty.eval(1, 1) == MissingGroundReason::None);

    // Rule 4a: Interior partition wall strip between two open rooms
    const RoleGrid<3> interior_wall {{
        "#####",
        "#WWW#",
        "#####",
    }};
    REQUIRE(interior_wall.eval(1, 1) == MissingGroundReason::InteriorWall);
    REQUIRE(interior_wall.eval(2, 1) == MissingGroundReason::InteriorWall);
    REQUIRE(interior_wall.eval(3, 1) == MissingGroundReason::InteriorWall);

    // Rule 4b: Sealed corner pocket (grounded walls + open floors, or unoccluded floor cutout in ungrounded walls)
    const RoleGrid<3> room_corner_wall {{
        "GG.",
        "GW#",
        ".##",
    }};
    REQUIRE(room_corner_wall.eval(1, 1) == MissingGroundReason::SealedPocket);

    // NW corner floor cutout behind E/S barrier walls is 2.5D occluded, and soft content on a wall corner is flagged.
    const RoleGrid<3> room_corner_floor {{
        ".#.",
        "#.W",
        ".W.",
    }};
    REQUIRE(room_corner_floor.eval(1, 1) == MissingGroundReason::OccludedBehindBarrier);

    const RoleGrid<3> room_corner_carpet {{
        "WW.",
        "WC#",
        ".##",
    }};
    REQUIRE(room_corner_carpet.eval(1, 1) == MissingGroundReason::MaskedByCarpetOrContent);

    // Rule 4c: Wall T-junction enclosed by walls + blocking rock/ground (0 open floors, 0 voids, >=2 grounded neighbours)
    const RoleGrid<3> t_junction_against_rock {{
        "SGS",
        "GWG",
        ".G.",
    }};
    REQUIRE(t_junction_against_rock.eval(1, 1) == MissingGroundReason::SealedPocket);

    // Rule 4d: Wall corner separating two grounded areas (all 4 orientations) triggers SealedPocket even when adjacent walls lack ground
    const RoleGrid<3> se_grounded_corner {{
        "#W#",
        "WW#",
        "###",
    }};
    REQUIRE(se_grounded_corner.eval(1, 1) == MissingGroundReason::SealedPocket);

    const RoleGrid<3> sw_grounded_corner {{
        "#W#",
        "#WW",
        "###",
    }};
    REQUIRE(sw_grounded_corner.eval(1, 1) == MissingGroundReason::SealedPocket);

    const RoleGrid<3> ne_grounded_corner {{
        "###",
        "WW#",
        "#W#",
    }};
    REQUIRE(ne_grounded_corner.eval(1, 1) == MissingGroundReason::SealedPocket);

    const RoleGrid<3> nw_grounded_corner {{
        "###",
        "#WW",
        "#W#",
    }};
    REQUIRE(nw_grounded_corner.eval(1, 1) == MissingGroundReason::SealedPocket);

    // Exterior cliff border with BlockPathfinder/non-open ground (e.g. 459/469 drop-off stair) on S and void border on E stays quiet.
    const RoleGrid<3> cliff_border_next_to_drop {{
        "##.",
        "#bb",
        ".S.",
    }};
    REQUIRE(cliff_border_next_to_drop.eval(1, 1) == MissingGroundReason::None);

    // Horizontal mountain/lava cliff border (open floor on N, non-open hazard ground on S, cliff borders on E/W) stays quiet.
    const RoleGrid<3> lava_cliff_border {{
        "###",
        "MMM",
        "SSS",
    }};
    REQUIRE(lava_cliff_border.eval(1, 1) == MissingGroundReason::None);

    // Diagonal barrier piece separating grounded wall (W) from open floor (E) above an empty hole (S) triggers SurroundedInterior.
    const RoleGrid<3> barrier_between_wall_and_floor {{
        "#W#",
        "GM#",
        "G.G",
    }};
    REQUIRE(barrier_between_wall_and_floor.eval(1, 1) == MissingGroundReason::SurroundedInterior);

    // Groundless corner barrier/wall with 3 grounded neighbours and 1 adjacent empty hole triggers despite voids == 1.
    const RoleGrid<3> corner_with_three_grounds_and_hole {{
        "#.#",
        "GMS",
        "#SS",
    }};
    REQUIRE(corner_with_three_grounds_and_hole.eval(1, 1) == MissingGroundReason::SurroundedInterior);

    // Trim edge next to an ungrounded pit/opening (3 open floors + 1 void neighbour) stays consistent and quiet, while 0-void trim triggers.
    const RoleGrid<4> trim_along_shaft {{
        "###",
        "#T#",
        "#T#",
        "###",
    }};
    REQUIRE(trim_along_shaft.eval(1, 1) == MissingGroundReason::None);
    REQUIRE(trim_along_shaft.eval(1, 2) == MissingGroundReason::None);

    const RoleGrid<3> fully_enclosed_trim {{
        "###",
        "#T#",
        "###",
    }};
    REQUIRE(fully_enclosed_trim.eval(1, 1) == MissingGroundReason::SurroundedInterior);
}

TEST_CASE("ComputeViewportTileBounds handles upper-floor shifts at 100% and 800% zoom", "[missing_ground]")
{
    constexpr int floor_offset = (7 - 2) * 32;
    constexpr int view_scroll_x = 100 * 32 - floor_offset - 80;
    constexpr int view_scroll_y = 100 * 32 - floor_offset - 45;
    const ViewportTileBounds zoomed_upper
        = ComputeViewportTileBounds(-view_scroll_x - floor_offset, -view_scroll_y - floor_offset, 160.0f, 90.0f, 32, 1);
    REQUIRE(zoomed_upper.start_x <= 100);
    REQUIRE(zoomed_upper.end_x >= 100);
    REQUIRE(zoomed_upper.start_y <= 100);
    REQUIRE(zoomed_upper.end_y >= 100);

    const ViewportTileBounds normal_ground = ComputeViewportTileBounds(-(100 * 32 - 640), -(100 * 32 - 360), 1280.0f, 720.0f, 32, 1);
    REQUIRE(normal_ground.start_x <= 100);
    REQUIRE(normal_ground.end_x >= 100);
    REQUIRE(normal_ground.start_y <= 100);
    REQUIRE(normal_ground.end_y >= 100);

    const ViewportTileBounds empty_view = ComputeViewportTileBounds(0, 0, 0.0f, 90.0f);
    REQUIRE(empty_view.end_x < empty_view.start_x);
    REQUIRE(empty_view.end_y < empty_view.start_y);
}

TEST_CASE("missing ground indicator occupies a 5-bit zone_flags slot cleanly", "[missing_ground]")
{
    const uint8_t rule_id = TechnicalItemRegistry::GetIndicatorId(TileIndicatorType::MissingGround);
    REQUIRE(rule_id == 18);

    const uint32_t packed = MakeIndicatorZoneFlag(rule_id);
    REQUIRE((packed & ~ZONE_FLAG_INDICATOR_MASK) == 0u);
    REQUIRE((packed & (ZONE_FLAG_BLOCKING | ZONE_FLAG_HOUSE_GROUND)) == 0u);
    REQUIRE(GetIndicatorTypeFromZoneFlags(packed) == rule_id);

    const auto rule_style = GetIndicatorBadgeStyle(TileIndicatorType::MissingGround);
    REQUIRE(std::string_view(rule_style.text) == "no ground");
}
