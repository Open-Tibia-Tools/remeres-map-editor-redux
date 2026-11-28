#include "app/application.h"
#include "app/definitions.h"
#include "editor/operations/search_query.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/spawn.h"
#include "map/map.h"
#include "map/missing_ground.h"
#include "map/missing_ground_chunk.h"
#include "map/missing_ground_tile.h"
#include "map/tile.h"
#include "rendering/core/missing_ground_overlay_cache.h"
#include "item_definitions/core/item_definition_store.h"
#include "item_definitions/core/item_definition_types.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

namespace rme::rendering {

    struct MissingGroundOverlayCacheTestAccess {
        [[nodiscard]] static size_t cachedChunkCount(const MissingGroundOverlayCache& cache) noexcept
        {
            return cache.chunks_.size();
        }

        [[nodiscard]] static bool isChunkValid(const MissingGroundOverlayCache& cache, int32_t cx, int32_t cy, int32_t z) noexcept
        {
            const auto it = cache.chunks_.find(ChunkCoord {cx, cy, z});
            return it != cache.chunks_.end() && it->second.is_valid;
        }
    };

} // namespace rme::rendering

using rme::rendering::MissingGroundOverlayCacheTestAccess;

// These two symbols live in application.cpp, which is excluded from test links.
Application& wxGetApp()
{
    static Application* application = nullptr;
    return *application;
}

void Application::Unload() { }

namespace {

    constexpr uint16_t kGroundId = 100;
    constexpr uint16_t kBlockingGroundId = 101; // e.g. 4958 ship sail (ground group, unpassable)
    constexpr uint16_t kPathAvoidGroundId = 102; // e.g. 459/469 invisible stair ground (BlockPathfinder)
    constexpr uint16_t kTrashGroundId = 103; // lava/trash-holder ground
    constexpr uint16_t kXmlBorderId = 200;
    constexpr uint16_t kClientClipBorderId = 201; // AlwaysOnBottom + topOrder==1 without IsBorder
    constexpr uint16_t kOptionalBorderId = 202;
    constexpr uint16_t kMountainBorderId = 203; // 873-style: IsBorder + AlwaysOnBottom + topOrder=2 + Unpassable + BlockMissiles
    constexpr uint16_t kWallId = 204; // IsWall + Unpassable + BlockMissiles
    constexpr uint16_t kTrimLedgeId = 205; // 10344/9647-style: Unpassable, !IsWall, !BlockMissiles
    constexpr uint16_t kCarpetId = 206; // IsCarpet
    constexpr uint16_t kStaticOrnamentId = 207; // 6763/6410-style: stackOrder=3, !Unpassable, !Moveable
    constexpr uint16_t kTableId = 208; // IsTable
    constexpr uint16_t kDoorId = 209; // ITEM_TYPE_DOOR without IsWall
    constexpr uint16_t kTeleportId = 210; // ITEM_TYPE_TELEPORT
    constexpr uint16_t kFloorChangeId = 211; // ItemFlag::FloorChange

    class ScopedItemDefinitions {
    public:
        ScopedItemDefinitions()
        {
            g_item_definitions.clear();
            Define(kGroundId, "grass", ITEM_GROUP_GROUND);
            Define(kBlockingGroundId, "ship sail", ITEM_GROUP_GROUND, flagMask(ItemFlag::Unpassable));
            Define(kPathAvoidGroundId, "void stair", ITEM_GROUP_GROUND, flagMask(ItemFlag::BlockPathfinder));
            Define(kTrashGroundId, "trash lava", ITEM_GROUP_GROUND, 0, 0, ITEM_TYPE_TRASHHOLDER);
            Define(kXmlBorderId, "grass border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));
            Define(kClientClipBorderId, "unlisted clip border", ITEM_GROUP_NONE, flagMask(ItemFlag::AlwaysOnBottom), 1);
            Define(kOptionalBorderId, "optional border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsOptionalBorder));
            Define(
                kMountainBorderId, "mountain wall border", ITEM_GROUP_NONE,
                flagMask(ItemFlag::IsBorder) | flagMask(ItemFlag::AlwaysOnBottom) | flagMask(ItemFlag::Unpassable)
                    | flagMask(ItemFlag::BlockMissiles),
                2
            );
            Define(
                kWallId, "stone wall", ITEM_GROUP_NONE,
                flagMask(ItemFlag::IsWall) | flagMask(ItemFlag::Unpassable) | flagMask(ItemFlag::BlockMissiles), 2
            );
            Define(kTrimLedgeId, "stone ledge", ITEM_GROUP_NONE, flagMask(ItemFlag::Unpassable), 0);
            Define(kCarpetId, "red carpet", ITEM_GROUP_NONE, flagMask(ItemFlag::IsCarpet) | flagMask(ItemFlag::AlwaysOnBottom), 1);
            Define(kStaticOrnamentId, "ice ornament", ITEM_GROUP_NONE, 0, 3);
            Define(kTableId, "wooden table", ITEM_GROUP_NONE, flagMask(ItemFlag::IsTable) | flagMask(ItemFlag::Unpassable), 2);
            Define(kDoorId, "wooden door", ITEM_GROUP_NONE, flagMask(ItemFlag::Unpassable), 2, ITEM_TYPE_DOOR);
            Define(kTeleportId, "magic forcefield", ITEM_GROUP_NONE, 0, 2, ITEM_TYPE_TELEPORT);
            Define(kFloorChangeId, "rope spot", ITEM_GROUP_NONE, flagMask(ItemFlag::FloorChange), 0);
        }
        ~ScopedItemDefinitions()
        {
            g_item_definitions.clear();
        }

        static void Define(
            uint16_t id, std::string name, ItemGroup_t group, uint64_t flags = 0, int always_on_top_order = 0,
            ItemTypes_t type = ITEM_TYPE_NONE
        )
        {
            ResolvedItemDefinitionRow row;
            row.server_id = id;
            row.client_id = id;
            row.group = group;
            row.type = type;
            row.flags = flags;
            row.always_on_top_order = always_on_top_order;
            row.name = std::move(name);
            g_item_definitions.append(std::move(row));
        }
    };

    void PutGround(Map& map, int x, int y, int z, uint16_t ground_id = kGroundId)
    {
        Tile* tile = map.createTile(x, y, z);
        REQUIRE(tile != nullptr);
        tile->addItem(std::make_unique<Item>(ground_id, 0));
        REQUIRE(tile->ground != nullptr);
    }

    void PutItem(Map& map, int x, int y, int z, uint16_t item_id)
    {
        Tile* tile = map.createTile(x, y, z);
        REQUIRE(tile != nullptr);
        tile->addItem(std::make_unique<Item>(item_id, 0));
    }

    void SurroundWithGround(Map& map, int x, int y, int z)
    {
        PutGround(map, x, y - 1, z);
        PutGround(map, x + 1, y, z);
        PutGround(map, x, y + 1, z);
        PutGround(map, x - 1, y, z);
    }

} // namespace

TEST_CASE("FindMissingGrounds returns nothing on an empty map", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;

    REQUIRE(SearchEngine::FindMissingGrounds(map).empty());

    SearchQuery query;
    query.categories = SearchCategory::MissingGround;
    query.progress_callback = [](float, std::string_view) { };
    REQUIRE(SearchEngine::Execute(map, query).empty());
}

TEST_CASE("FindMissingGrounds ignores unoccluded empty voids, exterior ledges, balcony brackets, and sail rows", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(256);
    map.setHeight(256);

    // Unoccluded empty void surrounded on 4 sides (no S/E/SE barrier) -> ignored
    SurroundWithGround(map, 10, 10, 7);

    // Exterior stone ledge row south of wall-with-hidden-ground and north of open street -> ignored
    for (int x = 30; x <= 33; ++x) {
        PutGround(map, x, 19, 7);
        PutItem(map, x, 19, 7, kWallId);
        PutItem(map, x, 20, 7, kTrimLedgeId);
        PutGround(map, x, 21, 7);
    }

    // Balcony bracket between two walkable balconies next to house wall -> ignored
    PutGround(map, 50, 19, 7);
    PutItem(map, 50, 19, 7, kWallId);
    PutGround(map, 50, 20, 7);
    PutItem(map, 50, 20, 7, kWallId);
    PutGround(map, 50, 21, 7);
    PutItem(map, 50, 21, 7, kWallId);
    PutGround(map, 51, 19, 7);
    PutItem(map, 51, 20, 7, kTrimLedgeId);
    PutGround(map, 51, 21, 7);

    // Ship sail (non-walkable pseudo-ground) on opposite sides of wall -> ignored
    PutItem(map, 70, 20, 7, kWallId);
    PutGround(map, 70, 19, 7, kBlockingGroundId);
    PutGround(map, 70, 21, 7, kBlockingGroundId);

    // Exterior mountain inner border edge (873-style) facing void -> ignored
    PutItem(map, 90, 20, 7, kMountainBorderId);
    PutGround(map, 90, 19, 7);

    // Bridge/platform static ornament edge & overlapping borders at 1/4 void edge -> ignored
    PutItem(map, 110, 20, 7, kStaticOrnamentId);
    PutGround(map, 110, 19, 7);
    PutItem(map, 120, 20, 7, kXmlBorderId);
    PutItem(map, 120, 20, 7, kOptionalBorderId);
    PutGround(map, 120, 19, 7);

    // Outer house wall painted with house_id over exterior void edge -> ignored
    PutItem(map, 130, 20, 7, kWallId);
    map.getTile(130, 20, 7)->setHouseID(1);
    PutGround(map, 130, 19, 7);

    // Zigzag diagonal cave/mountain cliff corner (N+W ground, E+S groundless cliff borders) -> ignored
    PutGround(map, 150, 19, 7);
    PutGround(map, 149, 20, 7);
    PutItem(map, 150, 20, 7, kMountainBorderId);
    PutItem(map, 151, 20, 7, kMountainBorderId);
    PutItem(map, 150, 21, 7, kMountainBorderId);

    // Adjacent back-to-back building walls: upper south wall has hidden ground (y=19),
    // lower north wall lacks ground (y=20) with open floor on S (y=21) -> ignored
    for (int x = 170; x <= 172; ++x) {
        PutGround(map, x, 19, 7);
        PutItem(map, x, 19, 7, kWallId);
        PutItem(map, x, 20, 7, kWallId);
        PutGround(map, x, 21, 7);
    }

    // Exterior octagonal NW wall elbow with ungrounded outer walls (N, W) and interior floor (E, S) -> ignored
    PutItem(map, 190, 19, 7, kWallId);
    PutItem(map, 189, 20, 7, kWallId);
    PutItem(map, 190, 20, 7, kWallId);
    PutGround(map, 191, 20, 7);
    PutGround(map, 190, 21, 7);

    REQUIRE(SearchEngine::FindMissingGrounds(map).empty());
}

TEST_CASE("FindMissingGrounds detects 2.5D occluded unallocated tiles across cell boundaries and deduplicates", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(128);
    map.setHeight(128);

    // W=(62,15) allocates the 4x4 node/floor shared with unallocated slot (63,15) (tile == nullptr),
    // E=(64,15) crosses into a separate GridCell, and S=(63,16) + SE=(64,16) barriers must deduplicate.
    PutGround(map, 62, 15, 7);
    PutGround(map, 64, 15, 7);
    PutItem(map, 63, 16, 7, kMountainBorderId);
    PutItem(map, 64, 16, 7, kMountainBorderId);

    const auto results = SearchEngine::FindMissingGrounds(map);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].position == Position(63, 15, 7));
    REQUIRE(results[0].item_id == 0);
    REQUIRE(results[0].subtitle == "Empty tile (occluded behind wall/mountain)");
}

TEST_CASE("FindMissingGrounds detects borders/carpets, interior walls, sealed pockets, creatures, and spawns", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(128);
    map.setHeight(128);

    // Static non-border ornament surrounded by walkable floors -> surrounded interior without ground
    PutItem(map, 10, 10, 7, kStaticOrnamentId);
    SurroundWithGround(map, 10, 10, 7);

    // Intentional 1x1 cave hole with border items only inside walkable floor -> ignored by FindMissingGrounds
    PutItem(map, 15, 10, 7, kXmlBorderId);
    PutItem(map, 15, 10, 7, kOptionalBorderId);
    SurroundWithGround(map, 15, 10, 7);

    // Carpet path between walkable floors (W and E) -> masked by carpet/content
    PutItem(map, 20, 10, 7, kCarpetId);
    PutGround(map, 19, 10, 7);
    PutGround(map, 21, 10, 7);

    // Interior wall strip between open rooms (N and S walkable) -> interior wall
    for (int x = 30; x <= 32; ++x) {
        PutItem(map, x, 20, 7, kWallId);
        PutGround(map, x, 19, 7);
        PutGround(map, x, 21, 7);
    }
    PutGround(map, 29, 20, 7);
    PutGround(map, 33, 20, 7);

    // Creature and spawn on groundless tiles inside floor edges (>= 2 open neighbours) -> functional
    Tile* creature_tile = map.createTile(50, 50, 7);
    REQUIRE(creature_tile != nullptr);
    creature_tile->creature = std::make_unique<Creature>("Rat");
    PutGround(map, 50, 49, 7);
    PutGround(map, 50, 51, 7);

    Tile* spawn_tile = map.createTile(60, 60, 7);
    REQUIRE(spawn_tile != nullptr);
    spawn_tile->spawn = std::make_unique<Spawn>(3);
    PutGround(map, 59, 60, 7);
    PutGround(map, 61, 60, 7);

    // Wall T-junction enclosed by walls + blocking ground (e.g. cave wall on N, grounded walls on W/E/S)
    PutItem(map, 70, 20, 7, kWallId);
    PutGround(map, 70, 19, 7, kBlockingGroundId);
    PutGround(map, 69, 20, 7);
    PutItem(map, 69, 20, 7, kWallId);
    PutGround(map, 71, 20, 7);
    PutItem(map, 71, 20, 7, kWallId);
    PutGround(map, 70, 21, 7);
    PutItem(map, 70, 21, 7, kWallId);

    const auto entries = SearchEngine::FindMissingGrounds(map);
    REQUIRE(entries.size() == 8);

    REQUIRE(entries[0].position == Position(10, 10, 7));
    REQUIRE(entries[0].subtitle == "ice ornament (surrounded interior without ground)");
    REQUIRE(entries[1].position == Position(20, 10, 7));
    REQUIRE(entries[1].subtitle == "red carpet (masked by carpet/content)");
    REQUIRE(entries[2].position == Position(30, 20, 7));
    REQUIRE(entries[2].subtitle == "stone wall (interior wall without ground)");
    REQUIRE(entries[3].position == Position(31, 20, 7));
    REQUIRE(entries[3].subtitle == "stone wall (interior wall without ground)");
    REQUIRE(entries[4].position == Position(32, 20, 7));
    REQUIRE(entries[4].subtitle == "stone wall (interior wall without ground)");
    REQUIRE(entries[5].position == Position(70, 20, 7));
    REQUIRE(entries[5].subtitle == "stone wall (sealed pocket without ground)");
    REQUIRE(entries[6].position == Position(50, 50, 7));
    REQUIRE(entries[6].subtitle == "Creature: Rat (functional tile without ground)");
    REQUIRE(entries[7].position == Position(60, 60, 7));
    REQUIRE(entries[7].subtitle == "Spawn (functional tile without ground)");
}

TEST_CASE("FindMissingGrounds clamps candidate tiles and neighbours to map width and height", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(64);
    map.setHeight(64);

    // Wall at right map edge (63, 5) has 1 in-bounds ground (62, 5) + out-of-bounds ground (64, 5) -> ignored
    PutItem(map, 63, 5, 7, kWallId);
    PutGround(map, 62, 5, 7);
    PutGround(map, 64, 5, 7);

    PutItem(map, 68, 68, 7, kWallId);
    SurroundWithGround(map, 68, 68, 7);

    REQUIRE(SearchEngine::FindMissingGrounds(map).empty());
}

TEST_CASE("SearchEngine::Execute groups Missing Grounds per item, creature, spawn, and empty tile", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;
    PutItem(map, 8, 8, 7, kWallId);
    SurroundWithGround(map, 8, 8, 7);

    SearchQuery query;
    query.categories = SearchCategory::MissingGround;
    query.query_title = "Find Missing Grounds (Map)";
    query.progress_callback = [](float, std::string_view) { };

    const auto single_group = SearchEngine::Execute(map, query);
    REQUIRE(single_group.size() == 1);
    REQUIRE(single_group[0].id == kWallId);
    REQUIRE(single_group[0].name == "stone wall");
    REQUIRE(single_group[0].lowercase_name == "stone wall");
    REQUIRE(single_group[0].id_str == std::to_string(kWallId));
    REQUIRE(single_group[0].expanded);
    REQUIRE(single_group[0].children.size() == 1);
    REQUIRE(single_group[0].children[0].position == Position(8, 8, 7));

    // Add 2 carpet traps, 1 Rat creature without ground, and 1 empty occluded tile
    PutItem(map, 20, 20, 7, kCarpetId);
    SurroundWithGround(map, 20, 20, 7);
    PutItem(map, 24, 20, 7, kCarpetId);
    SurroundWithGround(map, 24, 20, 7);

    Tile* rat_tile = map.createTile(40, 40, 7);
    REQUIRE(rat_tile != nullptr);
    rat_tile->creature = std::make_unique<Creature>("Rat");
    PutGround(map, 39, 40, 7);
    PutGround(map, 41, 40, 7);

    // Occluded empty tile at (60, 60, 7) behind mountain on S (60, 61, 7)
    PutGround(map, 59, 60, 7);
    PutGround(map, 61, 60, 7);
    PutItem(map, 60, 61, 7, kMountainBorderId);

    const auto groups = SearchEngine::Execute(map, query);
    REQUIRE(groups.size() == 4);
    // Highest count first: red carpet (2 matches), then alphabetical among 1-match groups
    REQUIRE(groups[0].id == kCarpetId);
    REQUIRE(groups[0].name == "red carpet");
    REQUIRE(groups[0].children.size() == 2);
    REQUIRE_FALSE(groups[0].expanded);

    REQUIRE(groups[1].name == "Empty Tiles");
    REQUIRE(groups[1].id == 0);
    REQUIRE(groups[1].use_missing_ground_badge);
    REQUIRE(groups[1].children.size() == 1);
    REQUIRE(groups[1].children[0].use_missing_ground_badge);

    REQUIRE(groups[2].name == "Rat");
    REQUIRE(groups[2].is_creature);
    REQUIRE(groups[2].children.size() == 1);

    REQUIRE(groups[3].id == kWallId);
    REQUIRE(groups[3].name == "stone wall");
    REQUIRE(groups[3].children.size() == 1);
}

TEST_CASE(
    "SearchEngine::Execute groups creatures with parentheses cleanly, builds groups before 1.0f, and honors selection and combined "
    "categories",
    "[search][missing_ground]"
)
{
    ScopedItemDefinitions defs;
    Map map;

    Tile* rat_cave = map.createTile(10, 10, 7);
    REQUIRE(rat_cave != nullptr);
    rat_cave->creature = std::make_unique<Creature>("Rat (Cave)");
    PutGround(map, 9, 10, 7);
    PutGround(map, 11, 10, 7);

    Tile* rat_plain = map.createTile(20, 10, 7);
    REQUIRE(rat_plain != nullptr);
    rat_plain->creature = std::make_unique<Creature>("Rat");
    PutGround(map, 19, 10, 7);
    PutGround(map, 21, 10, 7);

    // Selected ungrounded carpet at (30, 10) plus unselected ungrounded carpet at (40, 10)
    Tile* selected_carpet = map.createTile(30, 10, 7);
    REQUIRE(selected_carpet != nullptr);
    selected_carpet->addItem(Item::Create(kCarpetId));
    selected_carpet->select();
    SurroundWithGround(map, 30, 10, 7);

    PutItem(map, 40, 10, 7, kCarpetId);
    SurroundWithGround(map, 40, 10, 7);

    // Verify creature names with " (" remain separate groups and groups are built before 1.0f callback.
    {
        SearchQuery query;
        query.categories = SearchCategory::MissingGround;
        query.progress_callback = [](float progress, std::string_view) {
            if (progress >= 1.0f) {
                g_item_definitions.clear();
            }
        };

        const auto groups = SearchEngine::Execute(map, query);
        REQUIRE(groups.size() == 3);
        REQUIRE(groups[0].id == kCarpetId);
        REQUIRE(groups[0].name == "red carpet");
        REQUIRE(groups[0].children.size() == 2);
        REQUIRE(groups[1].name == "Rat");
        REQUIRE(groups[1].is_creature);
        REQUIRE(groups[1].children.size() == 1);
        REQUIRE(groups[2].name == "Rat (Cave)");
        REQUIRE(groups[2].is_creature);
        REQUIRE(groups[2].children.size() == 1);
    }

    // Re-seed definitions after completion callback cleared them; verify on_selection + combined categories.
    ScopedItemDefinitions reseeded;
    Tile* selected_uid = map.createTile(50, 10, 7);
    REQUIRE(selected_uid != nullptr);
    selected_uid->addItem(Item::Create(kGroundId));
    auto uid_item = Item::Create(kStaticOrnamentId);
    uid_item->setUniqueID(4242);
    selected_uid->addItem(std::move(uid_item));
    selected_uid->select();

    SearchQuery combined_query;
    combined_query.categories = SearchCategory::MissingGround | SearchCategory::UniqueID;
    combined_query.on_selection = true;
    combined_query.progress_callback = [](float, std::string_view) { };

    const auto combined = SearchEngine::Execute(map, combined_query);
    REQUIRE(combined.size() == 2);
    REQUIRE(combined[0].id == kStaticOrnamentId);
    REQUIRE(combined[0].children.size() == 1);
    REQUIRE(combined[0].children[0].position == Position(50, 10, 7));
    REQUIRE(combined[1].id == kCarpetId);
    REQUIRE(combined[1].children.size() == 1);
    REQUIRE(combined[1].children[0].position == Position(30, 10, 7));
}

TEST_CASE("FindMissingGrounds excludes BlockPathfinder ground from open-floor neighbour count", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;

    // (10, 10) has N open ground, E mountain barrier, W void, and S BlockPathfinder ground -> quiet.
    // Replacing S with normal ground creates opposite N/S open floors behind the E barrier -> occluded result.
    PutGround(map, 10, 9, 7);
    PutItem(map, 11, 10, 7, kMountainBorderId);
    PutGround(map, 10, 11, 7, kPathAvoidGroundId);

    REQUIRE(SearchEngine::FindMissingGrounds(map).empty());

    PutGround(map, 10, 11, 7, kGroundId);
    const auto results = SearchEngine::FindMissingGrounds(map);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].position == Position(10, 10, 7));
    REQUIRE(results[0].subtitle == "Empty tile (occluded behind wall/mountain)");
}

TEST_CASE("FindMissingGrounds catches wall corner separating grounded room from outer ground", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;

    // SE wall corner at (20, 20): N & W are ungrounded walls, E & S & NW are grounded floors.
    PutGround(map, 19, 19, 7);
    PutItem(map, 20, 19, 7, kWallId);
    PutItem(map, 19, 20, 7, kWallId);
    PutItem(map, 20, 20, 7, kWallId);
    PutGround(map, 21, 20, 7);
    PutGround(map, 20, 21, 7);

    const auto results = SearchEngine::FindMissingGrounds(map);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].position == Position(20, 20, 7));
    REQUIRE(results[0].subtitle == "stone wall (sealed pocket without ground)");
}

TEST_CASE("FindMissingGrounds catches barrier wall seam and 3-ground corner adjacent to hole", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;

    // 1. Diagonal mountain barrier (30, 30) between grounded wall (W=29,30) and open floor (E=31,30), with empty hole (30, 31)
    PutGround(map, 29, 30, 7);
    PutItem(map, 29, 30, 7, kWallId);
    PutItem(map, 30, 30, 7, kMountainBorderId);
    PutGround(map, 31, 30, 7);

    // 2. Diagonal mountain barrier (50, 50) with 3 grounded neighbours (W, E, S) and an unoccluded void on N (50, 49)
    PutGround(map, 49, 50, 7, kBlockingGroundId);
    PutGround(map, 51, 50, 7, kBlockingGroundId);
    PutGround(map, 50, 51, 7, kBlockingGroundId);
    PutItem(map, 50, 50, 7, kMountainBorderId);

    const auto results = SearchEngine::FindMissingGrounds(map);
    REQUIRE(results.size() == 2);
    REQUIRE(results[0].position == Position(30, 30, 7));
    REQUIRE(results[0].subtitle == "mountain wall border (surrounded interior without ground)");
    REQUIRE(results[1].position == Position(50, 50, 7));
    REQUIRE(results[1].subtitle == "mountain wall border (surrounded interior without ground)");
}

TEST_CASE("FindMissingGrounds keeps wooden trim along an ungrounded shaft consistent and quiet", "[search][missing_ground]")
{
    ScopedItemDefinitions defs;
    Map map;

    // Trim edge at (10, 10) touches 3 wooden floors (N, W, E) and 1 adjacent groundless trim (S = 10, 11) -> both quiet.
    PutGround(map, 10, 9, 7);
    PutGround(map, 9, 10, 7);
    PutGround(map, 11, 10, 7);
    PutGround(map, 9, 11, 7);
    PutGround(map, 11, 11, 7);
    PutItem(map, 10, 10, 7, kTrimLedgeId);
    PutItem(map, 10, 11, 7, kTrimLedgeId);

    REQUIRE(SearchEngine::FindMissingGrounds(map).empty());
}

TEST_CASE("ClassifyNeighborRoles matches ClassifyTile neighbor bits across ground and item variants", "[missing_ground][cache]")
{
    ScopedItemDefinitions defs;
    constexpr MissingGround::RoleFlags kNeighborMask = MissingGround::RoleFlags::HasGround | MissingGround::RoleFlags::OpenFloor
        | MissingGround::RoleFlags::Barrier | MissingGround::RoleFlags::HasWall;

    REQUIRE(MissingGround::ClassifyNeighborRoles(nullptr) == MissingGround::RoleFlags::None);

    Map map;
    auto checkNeighborParity = [&](int x, int y) {
        const Tile* t = map.getTile(x, y, 7);
        REQUIRE(MissingGround::ClassifyNeighborRoles(t) == (MissingGround::ClassifyTile(t) & kNeighborMask));
    };

    // Open floor, blocking ground, BlockPathfinder ground, TrashHolder ground
    PutGround(map, 1, 1, 7, kGroundId);
    PutGround(map, 2, 1, 7, kBlockingGroundId);
    PutGround(map, 3, 1, 7, kPathAvoidGroundId);
    PutGround(map, 4, 1, 7, kTrashGroundId);
    for (int x = 1; x <= 4; ++x) {
        checkNeighborParity(x, 1);
    }

    // Grounded wall, grounded mountain barrier, grounded clip border + ornaments + trim
    PutGround(map, 5, 1, 7, kGroundId);
    PutItem(map, 5, 1, 7, kWallId);
    PutGround(map, 6, 1, 7, kGroundId);
    PutItem(map, 6, 1, 7, kMountainBorderId);
    PutGround(map, 7, 1, 7, kGroundId);
    PutItem(map, 7, 1, 7, kClientClipBorderId);
    PutItem(map, 7, 1, 7, kStaticOrnamentId);
    PutItem(map, 7, 1, 7, kTrimLedgeId);
    checkNeighborParity(5, 1);
    checkNeighborParity(6, 1);
    checkNeighborParity(7, 1);
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyNeighborRoles(map.getTile(7, 1, 7)), MissingGround::RoleFlags::OpenFloor));

    // Ungrounded functional/soft items (Door, Teleport, FloorChange, Table)
    PutItem(map, 8, 1, 7, kDoorId);
    PutItem(map, 9, 1, 7, kTeleportId);
    PutItem(map, 10, 1, 7, kFloorChangeId);
    PutItem(map, 11, 1, 7, kTableId);
    for (int x = 8; x <= 11; ++x) {
        checkNeighborParity(x, 1);
    }
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyTile(map.getTile(8, 1, 7)), MissingGround::RoleFlags::Functional));
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyTile(map.getTile(9, 1, 7)), MissingGround::RoleFlags::Functional));
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyTile(map.getTile(10, 1, 7)), MissingGround::RoleFlags::Functional));
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyTile(map.getTile(11, 1, 7)), MissingGround::RoleFlags::HasSoftContent));
}

TEST_CASE(
    "MissingGround::EvaluateChunk matches SearchEngine::FindMissingGrounds across chunk borders and map bounds", "[missing_ground][cache]"
)
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(64);
    map.setHeight(64);

    // 1. Cross-chunk OccludedBehindBarrier empty void at (15, 15) inside chunk (0,0) with barrier at (16, 15) in chunk (1,0)
    PutGround(map, 15, 14, 7);
    PutGround(map, 15, 16, 7);
    PutItem(map, 16, 15, 7, kMountainBorderId);

    // 2. Wall near right map edge (63, 10): W and N grounded (non-opposite), E (64, 10) is out-of-bounds ground -> must be ignored.
    PutGround(map, 62, 10, 7);
    PutGround(map, 63, 9, 7);
    PutItem(map, 63, 10, 7, kWallId);
    PutGround(map, 64, 10, 7);

    // Build expected set from FindMissingGrounds
    const auto search_results = SearchEngine::FindMissingGrounds(map);
    std::set<std::pair<int, int>> expected_positions;
    for (const auto& entry : search_results) {
        expected_positions.emplace(entry.position.x, entry.position.y);
    }
    REQUIRE(expected_positions.count({15, 15}) == 1);
    REQUIRE(expected_positions.count({63, 10}) == 0);

    std::set<std::pair<int, int>> chunk_positions;
    std::vector<MissingGround::RoleFlags> scratch;
    ChunkTileMask mask;
    map.visitPopulatedChunks(0, 0, 4, 4, 7, [&](int cx, int cy) {
        if (MissingGround::EvaluateChunk(map, cx, cy, 7, map.getWidth(), map.getHeight(), mask.words, scratch)) {
            mask.forEach([&](int tx, int ty) { chunk_positions.emplace((cx << 4) + tx, (cy << 4) + ty); });
        }
    });

    REQUIRE(chunk_positions == expected_positions);
}

TEST_CASE(
    "MissingGroundOverlayCache tracks SpatialChangeTracker safely, handles moves, invalidates 3x3 chunk halos, handles disabling, and "
    "prunes",
    "[missing_ground][cache]"
)
{
    ScopedItemDefinitions defs;

    // 1. Verify safe destruction if SpatialChangeTracker dies BEFORE MissingGroundOverlayCache (no UAF)
    {
        rme::rendering::MissingGroundOverlayCache cache;
        {
            auto temp_map = std::make_unique<Map>();
            PutItem(*temp_map, 8, 8, 7, kCarpetId);
            SurroundWithGround(*temp_map, 8, 8, 7);
            cache.syncDirtyState(temp_map->getChangeTracker(), *temp_map, temp_map->getWidth(), temp_map->getHeight(), true);
            const auto& chunk = cache.getOrEvaluateChunk(0, 0, 7, *temp_map, temp_map->getWidth(), temp_map->getHeight());
            REQUIRE(chunk.has_any);
            REQUIRE(chunk.mask.test(8, 8));
        } // temp_map + SpatialChangeTracker destroyed here; cache destructs next without crashing.
    }

    // 1b. Verify moving an attached SpatialChangeTracker updates the live tracker address safely without dangling detach
    {
        Map dummy_map;
        SpatialChangeTracker tracker_a;
        rme::rendering::MissingGroundOverlayCache cache;
        cache.syncDirtyState(tracker_a, dummy_map, dummy_map.getWidth(), dummy_map.getHeight(), true);
        (void)cache.getOrEvaluateChunk(0, 0, 7, dummy_map, dummy_map.getWidth(), dummy_map.getHeight());
        REQUIRE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 7));

        SpatialChangeTracker tracker_b(std::move(tracker_a));
        tracker_b.markTileDirty(16, 15, 7);
        cache.syncDirtyState(tracker_b, dummy_map, dummy_map.getWidth(), dummy_map.getHeight(), true);
        REQUIRE_FALSE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 7));
    }

    // 2. Cross-chunk 3x3 halo invalidation, floor isolation, resize, disable clear, and far-floor pruning
    Map map;
    map.setWidth(64);
    map.setHeight(64);
    PutItem(map, 15, 15, 7, kCarpetId);
    SurroundWithGround(map, 15, 15, 7);
    PutGround(map, 5, 5, 6);
    PutGround(map, 5, 5, 11);

    rme::rendering::MissingGroundOverlayCache cache;
    cache.advanceFrame(7);
    cache.syncDirtyState(map.getChangeTracker(), map, map.getWidth(), map.getHeight(), true);
    {
        const auto& c00 = cache.getOrEvaluateChunk(0, 0, 7, map, map.getWidth(), map.getHeight());
        REQUIRE(c00.is_valid);
        REQUIRE(c00.has_any);
        REQUIRE(c00.mask.test(15, 15));
    }
    REQUIRE(cache.getOrEvaluateChunk(0, 0, 6, map, map.getWidth(), map.getHeight()).is_valid);
    REQUIRE(cache.getOrEvaluateChunk(0, 0, 11, map, map.getWidth(), map.getHeight()).is_valid);

    // Edit neighbor (16, 15, 7) in chunk (1, 0), NOT (0, 0), so only 3x3 halo propagation invalidates (0, 0, 7).
    Tile* east_neighbor = map.getTile(16, 15, 7);
    REQUIRE(east_neighbor != nullptr);
    east_neighbor->ground.reset();
    // Removing N/S too would drop open floors below 2; leave only N ground so (15,15) stops triggering.
    map.getTile(15, 16, 7)->ground.reset();
    map.getTile(14, 15, 7)->ground.reset();
    map.getChangeTracker().markTileDirty(16, 15, 7);

    cache.syncDirtyState(map.getChangeTracker(), map, map.getWidth(), map.getHeight(), true);
    REQUIRE_FALSE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 7));
    REQUIRE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 6)); // Floor 6 must remain untouched

    {
        const auto& c00_after = cache.getOrEvaluateChunk(0, 0, 7, map, map.getWidth(), map.getHeight());
        REQUIRE(c00_after.is_valid);
        REQUIRE_FALSE(c00_after.has_any);
    }

    // Map resize invalidates cached chunks
    map.setWidth(128);
    cache.syncDirtyState(map.getChangeTracker(), map, map.getWidth(), map.getHeight(), true);
    REQUIRE_FALSE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 7));
    REQUIRE_FALSE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 6));
    REQUIRE(cache.getOrEvaluateChunk(0, 0, 7, map, map.getWidth(), map.getHeight()).is_valid);
    REQUIRE(cache.getOrEvaluateChunk(0, 0, 6, map, map.getWidth(), map.getHeight()).is_valid);
    REQUIRE(cache.getOrEvaluateChunk(0, 0, 11, map, map.getWidth(), map.getHeight()).is_valid);

    // Far-floor prune removes empty far floor z=11 while retaining surface floor z=6
    for (uint64_t f = 0; f <= rme::rendering::MissingGroundOverlayCache::PRUNE_INTERVAL_FRAMES; ++f) {
        cache.advanceFrame(7);
    }
    REQUIRE_FALSE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 11));
    REQUIRE(MissingGroundOverlayCacheTestAccess::isChunkValid(cache, 0, 0, 6));

    // Disabling overlay clears cached chunks and frees state
    cache.syncDirtyState(map.getChangeTracker(), map, map.getWidth(), map.getHeight(), false);
    REQUIRE(MissingGroundOverlayCacheTestAccess::cachedChunkCount(cache) == 0);
}

TEST_CASE(
    "Bottom-clip carpet without IsCarpet XML catches interior missing ground while exterior void edge stays quiet",
    "[search][missing_ground]"
)
{
    ScopedItemDefinitions defs;
    Map map;
    map.setWidth(128);
    map.setHeight(128);

    // Exterior clip border next to single ground neighbor stays quiet
    PutGround(map, 10, 9, 7);
    PutItem(map, 10, 10, 7, kClientClipBorderId);

    // Unregistered bottom-clip carpet (e.g. 1798 in 10.98 where IsCarpet is omitted from brushes.xml) surrounded by walkable floors
    PutItem(map, 30, 30, 7, kClientClipBorderId);
    SurroundWithGround(map, 30, 30, 7);

    const auto results = SearchEngine::FindMissingGrounds(map);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].position == Position(30, 30, 7));
    REQUIRE(results[0].subtitle == "unlisted clip border (masked by carpet/content)");

    // ClassifyTile marks kClientClipBorderId with IsBottomClip while ClassifyNeighborRoles stays strictly parity-safe.
    REQUIRE(MissingGround::HasRole(MissingGround::ClassifyTile(map.getTile(30, 30, 7)), MissingGround::RoleFlags::IsBottomClip));
    REQUIRE(MissingGround::ClassifyNeighborRoles(map.getTile(30, 30, 7)) == MissingGround::RoleFlags::None);
}
