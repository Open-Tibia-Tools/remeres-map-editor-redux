//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/iomap_sec.h"
#include "io/sec/srv_creature_files.h"
#include "io/sec/srv_moveuse.h"
#include "io/sec/srv_script_lexer.h"
#include "io/sec/srv_world_files.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/sec_sector_parser.h"

#include "app/client_version.h"
#include "app/settings.h"
#include "brushes/creature/creature_brush.h"
#include "app/managers/version_manager.h"
#include "game/srv_homes.h"
#include "game/srv_markers.h"
#include "game/srv_liquids.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/creatures.h"
#include "game/house.h"
#include "game/material_database.h"
#include "game/materials.h"
#include "game/spawn.h"
#include "game/town.h"
#include "game/waypoints.h"
#include "game/item.h"
#include "io/iomap_otbm.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "ui/gui.h"
#include "util/file_system.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <spdlog/spdlog.h>

namespace {

	using rme::sec::InstanceAttribute;
	using rme::sec::readFile;
	using rme::sec::ObjectAttribute;
	using rme::sec::SectorObject;
	using rme::sec::SectorTile;
	using rme::sec::TileFlag;

	constexpr size_t kMaxListedUnknownTypes = 20;
	constexpr size_t kMaxListedUnplacedHomes = 30;

	std::filesystem::path toPath(const FileName& identifier) {
		return std::filesystem::path(identifier.GetFullPath().ToStdWstring());
	}

	bool isSectorFile(const std::filesystem::path& path) {
		return rme::sec::parseSectorFileName(path.filename().string()).has_value();
	}

	struct SectorFile {
		std::filesystem::path path;
		rme::sec::SectorCoordinates coordinates;
	};

	std::vector<SectorFile> listSectorFiles(const std::filesystem::path& folder) {
		std::vector<SectorFile> files;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
			if (!entry.is_regular_file(ec)) {
				continue;
			}
			if (const auto coordinates = rme::sec::parseSectorFileName(entry.path().filename().string())) {
				files.push_back(SectorFile { .path = entry.path(), .coordinates = *coordinates });
			}
		}
		return files;
	}

	struct LoadStats {
		std::map<uint16_t, size_t> unknown_types; // server type ID -> occurrences
		size_t custom_attributes = 0;
		size_t dropped_contents = 0;
		size_t replaced_grounds = 0;
	};

	// Turns sector objects into editor items. Server attributes without a
	// native equivalent on the item (key and keyhole numbers, quest chests,
	// door levels and quests, expiry) are kept as item attributes under their
	// server names; the item panel and tooltips show them.
	class ItemBuilder {
	public:
		explicit ItemBuilder(LoadStats& stats) :
			stats_(stats) { }

		// Item definitions come from the server's objects.srv, so its type
		// IDs are the editor's server IDs.
		std::unique_ptr<Item> create(const SectorObject& object) {
			if (!g_item_definitions.exists(object.type_id)) {
				++stats_.unknown_types[object.type_id];
				return nullptr;
			}

			std::unique_ptr<Item> item = Item::Create(object.type_id);
			if (!item) {
				++stats_.unknown_types[object.type_id];
				return nullptr;
			}

			// The server has no liquid in a pool or vial, and no charges on a
			// rune, until an attribute sets them; the editor's defaults for new
			// items are water and 1 charge.
			if (item->isSplash() || item->isFluidContainer()) {
				item->setSubtype(LIQUID_NONE);
			} else if (item->isCharged() && !item->isStackable()) {
				item->setSubtype(0);
			}
			for (const ObjectAttribute& attribute : object.attributes) {
				apply(*item, attribute);
			}

			if (!object.content.empty()) {
				if (Container* container = item->asContainer()) {
					for (const SectorObject& child : object.content) {
						if (auto child_item = create(child)) {
							container->getVector().push_back(std::move(child_item));
						}
					}
				} else {
					++stats_.dropped_contents;
				}
			}
			return item;
		}

	private:
		void apply(Item& item, const ObjectAttribute& attribute) {
			switch (attribute.key) {
				case InstanceAttribute::Amount:
					if (item.isStackable()) {
						item.setSubtype(clampSubtype(attribute.number));
						return;
					}
					break;
				case InstanceAttribute::Charges:
					if (item.isCharged()) {
						item.setSubtype(clampSubtype(attribute.number));
						return;
					}
					break;
				case InstanceAttribute::ContainerLiquidType:
				case InstanceAttribute::PoolLiquidType:
					if ((item.isFluidContainer() || item.isSplash()) && attribute.number >= 0 && attribute.number < static_cast<int64_t>(rme::srv::kLiquids.size())) {
						item.setSubtype(static_cast<uint16_t>(rme::srv::kLiquids[static_cast<size_t>(attribute.number)]));
						return;
					}
					break;
				case InstanceAttribute::String:
					item.setText(attribute.text);
					return;
				case InstanceAttribute::AbsTeleportDestination:
					if (Teleport* teleport = item.asTeleport()) {
						const auto destination = rme::sec::unpackAbsoluteCoordinate(attribute.number);
						teleport->setDestination(Position(destination.x, destination.y, destination.z));
						return;
					}
					break;
				default:
					break;
			}

			const std::string name(rme::sec::attributeName(attribute.key));
			if (attribute.key == InstanceAttribute::Editor) {
				item.setAttribute(name, attribute.text);
			} else {
				item.setAttribute(name, static_cast<int32_t>(attribute.number));
			}
			++stats_.custom_attributes;
		}

		static uint16_t clampSubtype(int64_t value) {
			return static_cast<uint16_t>(std::clamp<int64_t>(value, 0, 0xFFFF));
		}

		LoadStats& stats_;
	};

	// Sector files list ground and always-on-top items first, then the other
	// items with the topmost first. The editor keeps those bottom to top.
	void addObjects(Tile& tile, std::span<const SectorObject> objects, ItemBuilder& builder, LoadStats& stats) {
		std::vector<std::unique_ptr<Item>> stacked;
		for (const SectorObject& object : objects) {
			std::unique_ptr<Item> item = builder.create(object);
			if (!item) {
				continue;
			}
			if (item->isGroundTile()) {
				if (tile.hasGround()) {
					++stats.replaced_grounds;
				}
				tile.addItemFast(std::move(item));
			} else if (item->isAlwaysOnBottom()) {
				tile.addItemFast(std::move(item));
			} else {
				stacked.push_back(std::move(item));
			}
		}
		for (auto& item : stacked | std::views::reverse) {
			tile.addItemFast(std::move(item));
		}
	}

	// Monsters (mon/*.mon + dat/monster.db) and NPCs (npc/*.npc) of the server
	// whose data folder holds the map folder.
	class CreatureLoader {
	public:
		CreatureLoader(Map& map, std::vector<std::string>& warnings) :
			map_(map), warnings_(warnings), max_radius_(std::max(1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS))) { }

		void load(const std::filesystem::path& data_folder) {
			loadMonsterRaces(data_folder / "mon");
			loadMonsterHomes(data_folder / "dat" / "monster.db");
			loadNpcs(data_folder / "npc");
			addCreaturePalette();

			if (unplaced_ > 0) {
				warnings_.push_back(std::format("{} creatures could not be placed (their home tile is taken and no free walkable tile is within the radius):", unplaced_));
				for (const std::string& line : unplaced_lines_) {
					warnings_.push_back("    " + line);
				}
				if (unplaced_homes_ > unplaced_lines_.size()) {
					warnings_.push_back(std::format("    ... and {} more spawns", unplaced_homes_ - unplaced_lines_.size()));
				}
			}
			if (capped_radii_ > 0) {
				warnings_.push_back(std::format("{} monster homes and NPCs have a radius above the editor's maximum spawn radius ({}); their spawns use {}. Raise max_spawn_radius in config.toml to keep larger radii (most homes use 50).", capped_radii_, max_radius_, max_radius_));
			}
			spdlog::info("IOMapSec: placed {} monsters and {} NPCs", placed_monsters_, placed_npcs_);
		}

		// Monster race number -> its creature type.
		[[nodiscard]] const std::map<int, CreatureType*>& races() const {
			return races_;
		}

	private:
		// The server's monsters and NPCs as a Creature palette, so they can be
		// placed and picked from the map ("Select Creature" needs their brushes).
		void addCreaturePalette() {
			const auto tileset = [](std::string name, std::vector<CreatureType*>& types) {
				std::ranges::sort(types, {}, [](const CreatureType* type) { return as_lower_str(type->name); });
				DynamicTilesetDefinition definition { .name = std::move(name) };
				for (CreatureType* type : types) {
					if (CreatureBrush* brush = g_materials.ensureCreatureBrush(type)) {
						brush->flagAsVisible();
						if (!definition.containsBrush(brush)) {
							definition.brushes.push_back(brush);
						}
					}
				}
				return definition;
			};

			DynamicPaletteDefinition palette { .name = "Creature" };
			palette.tilesets.push_back(tileset("Monsters", monster_types_));
			palette.tilesets.push_back(tileset("NPCs", npc_types_));
			g_materials.paletteCatalog().addDynamicPalette(std::move(palette));
		}

		template <typename OnFile>
		void forEachFile(const std::filesystem::path& folder, std::string_view extension, OnFile on_file) {
			std::error_code ec;
			std::string text;
			for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
				if (!entry.is_regular_file(ec) || !rme::srv::equalsIgnoreCase(entry.path().extension().string(), extension)) {
					continue;
				}
				const std::string file_name = FileSystem::PathToUtf8(entry.path().filename());
				if (!readFile(entry.path(), text)) {
					warnings_.push_back(std::format("{}: could not be read, skipped.", file_name));
					continue;
				}
				on_file(file_name, text);
			}
			if (ec) {
				warnings_.push_back(std::format("{}: could not be listed, its creatures were not loaded.", FileSystem::PathToUtf8(folder)));
			}
		}

		CreatureType* creatureType(const std::string& name, bool is_npc, const rme::sec::CreatureOutfit& look) {
			if (CreatureType* type = g_creatures[name]) {
				return type;
			}
			Outfit outfit(look.look_type, look.head, look.body, look.legs, look.feet, 0);
			outfit.lookItem = look.look_item;
			return g_creatures.addCreatureType(name, is_npc, outfit);
		}

		void loadMonsterRaces(const std::filesystem::path& folder) {
			std::vector<std::pair<std::string, rme::sec::MonsterRace>> races;
			forEachFile(folder, ".mon", [&](const std::string& file_name, std::string_view text) {
				auto race = rme::sec::readMonsterRace(text);
				if (!race) {
					warnings_.push_back(std::format("{}:{}: {}; monster race skipped.", file_name, race.error().line, race.error().message));
					return;
				}
				races.emplace_back(FileSystem::PathToUtf8(std::filesystem::path(file_name).stem()), std::move(*race));
			});
			for (const rme::sec::NamedMonsterRace& named : rme::sec::nameMonsterRaces(std::move(races))) {
				CreatureType* type = creatureType(named.editor_name, false, named.race.outfit);
				races_[named.race.race] = type;
				monster_types_.push_back(type);
			}
		}

		void loadMonsterHomes(const std::filesystem::path& path) {
			std::string text;
			if (!readFile(path, text)) {
				warnings_.push_back(std::format("{} was not found; no monsters were placed.", FileSystem::PathToUtf8(path)));
				return;
			}
			const auto homes = rme::sec::readMonsterHomes(text);
			if (!homes) {
				warnings_.push_back(std::format("monster.db:{}: {}; no monsters were placed.", homes.error().line, homes.error().message));
				return;
			}

			std::map<int, size_t> unknown_races;
			for (const rme::sec::MonsterHome& home : *homes) {
				const auto race = races_.find(home.race);
				if (race == races_.end() || !race->second) {
					unknown_races[home.race] += static_cast<size_t>(std::max(0, home.amount));
					continue;
				}
				placed_monsters_ += place(race->second, false, home.position, home.radius, home.amount, home.regen);
			}
			for (const auto& [race, count] : unknown_races) {
				warnings_.push_back(std::format("monster.db: race {} has no mon/*.mon file; {} monsters were not placed.", race, count));
			}
		}

		void loadNpcs(const std::filesystem::path& folder) {
			const int spawn_time = g_settings.getInteger(Config::DEFAULT_SPAWNTIME);
			forEachFile(folder, ".npc", [&](const std::string& file_name, std::string_view text) {
				const auto npc = rme::sec::readNpc(text);
				if (!npc) {
					warnings_.push_back(std::format("{}:{}: {}; NPC skipped.", file_name, npc.error().line, npc.error().message));
					return;
				}
				CreatureType* type = creatureType(npc->name, true, npc->outfit);
				npc_types_.push_back(type);
				placed_npcs_ += place(type, true, npc->home, npc->radius, 1, spawn_time);
			});
		}

		// A monster home or NPC as one spawn at `home` with the server's radius
		// (up to the editor's maximum spawn radius),
		// holding one creature that carries the amount: on the home tile (trap
		// creatures sit inside walls there), or on the nearest free walkable
		// tile when another creature already stands on it. Returns `amount`
		// when placed, 0 otherwise.
		size_t place(CreatureType* type, bool is_npc, const rme::sec::AbsolutePosition& home, int radius, int amount, int spawn_time) {
			const auto wanted = static_cast<size_t>(std::max(0, amount));
			if (wanted == 0) {
				return 0;
			}
			if (home.z < 0 || home.z >= MAP_LAYERS || home.x < 0 || home.y < 0 || home.x >= MAP_MAX_WIDTH || home.y >= MAP_MAX_HEIGHT) {
				noteUnplaced(type, home, radius, wanted);
				return 0;
			}
			// Spawn coverage is counted per tile in the radius, so a few huge
			// radii (some homes use 5000) would cost minutes and gigabytes.
			const int spawn_radius = rme::srv::spawnSizeFor(radius, max_radius_);
			if (radius > max_radius_) {
				++capped_radii_;
			}
			Tile* center = map_.createTile(home.x, home.y, home.z);
			if (center->spawn) {
				if (center->spawn->getSize() < spawn_radius) {
					// Coverage is counted when a spawn is added, so re-add it at its new size.
					map_.removeSpawn(center);
					center->spawn->setSize(spawn_radius);
					map_.addSpawn(center);
				}
			} else {
				center->spawn = std::make_unique<Spawn>(spawn_radius);
				map_.addSpawn(center);
			}

			Tile* tile = center->creature ? findFreeTile(home, spawn_radius) : center;
			if (!tile) {
				noteUnplaced(type, home, radius, wanted);
				return 0;
			}
			tile->creature = std::make_unique<Creature>(type);
			tile->creature->setSpawnTime(spawn_time);
			tile->creature->setAmount(static_cast<int>(wanted));
			tile->creature->setHomeRadius(radius);
			if (is_npc) {
				tile->creature->markAsNpcSpawn();
			}
			return wanted;
		}

		Tile* findFreeTile(const rme::sec::AbsolutePosition& home, int radius) const {
			for (int ring = 1; ring <= radius; ++ring) {
				for (int dy = -ring; dy <= ring; ++dy) {
					for (int dx = -ring; dx <= ring; ++dx) {
						if (std::max(std::abs(dx), std::abs(dy)) != ring) {
							continue;
						}
						Tile* tile = map_.getTile(home.x + dx, home.y + dy, home.z);
						if (tile && tile->hasGround() && !tile->isBlocking() && !tile->creature) {
							return tile;
						}
					}
				}
			}
			return nullptr;
		}

		void noteUnplaced(const CreatureType* type, const rme::sec::AbsolutePosition& home, int radius, size_t count) {
			if (count == 0) {
				return;
			}
			unplaced_ += count;
			++unplaced_homes_;
			if (unplaced_lines_.size() < kMaxListedUnplacedHomes) {
				unplaced_lines_.push_back(std::format("{} x {} at [{}, {}, {}], radius {}", count, type ? type->name : std::string("?"), home.x, home.y, home.z, radius));
			}
		}

		Map& map_;
		std::vector<std::string>& warnings_;
		std::map<int, CreatureType*> races_;
		std::vector<CreatureType*> monster_types_;
		std::vector<CreatureType*> npc_types_;
		size_t placed_monsters_ = 0;
		size_t placed_npcs_ = 0;
		const int max_radius_;
		size_t capped_radii_ = 0;
		size_t unplaced_ = 0;
		size_t unplaced_homes_ = 0;
		std::vector<std::string> unplaced_lines_;
	};

	// Towns, named marks and houses from the server's dat folder.
	class WorldLoader {
	public:
		WorldLoader(Map& map, std::vector<std::string>& warnings) :
			map_(map), warnings_(warnings) { }

		void load(const std::filesystem::path& dat_folder) {
			std::string text;
			if (!readFile(dat_folder / "map.dat", text)) {
				warnings_.push_back("dat/map.dat was not found; no towns or waypoints were loaded.");
			} else if (auto info = rme::sec::readMapInfo(text); !info) {
				warnings_.push_back(std::format("map.dat:{}: {}; no towns or waypoints were loaded.", info.error().line, info.error().message));
			} else {
				addTowns(*info);
				addWaypoints(*info);
				info_ = std::move(*info);
			}

			if (!readFile(dat_folder / "houseareas.dat", text)) {
				warnings_.push_back("dat/houseareas.dat was not found; no houses were loaded.");
				return;
			}
			const auto areas = rme::sec::readHouseAreas(text);
			if (!areas) {
				warnings_.push_back(std::format("houseareas.dat:{}: {}; no houses were loaded.", areas.error().line, areas.error().message));
				return;
			}
			if (!readFile(dat_folder / "houses.dat", text)) {
				warnings_.push_back("dat/houses.dat was not found; no houses were loaded.");
				return;
			}
			const auto houses = rme::sec::readHouses(text);
			if (!houses) {
				warnings_.push_back(std::format("houses.dat:{}: {}; no houses were loaded.", houses.error().line, houses.error().message));
				return;
			}
			addHouses(*areas, *houses);
		}

		// map.dat, when it was read.
		[[nodiscard]] const rme::sec::MapInfo* mapInfo() const {
			return info_ ? &*info_ : nullptr;
		}

	private:
		// Editor town IDs start at 1 (0 means "no town"), depot numbers at 0.
		static uint32_t townId(int depot) {
			return static_cast<uint32_t>(depot) + 1;
		}

		static Position toPosition(const rme::sec::AbsolutePosition& position) {
			return Position(position.x, position.y, position.z);
		}

		// "Port Hope" and the mark "PortHope" name the same place.
		static std::string placeKey(std::string_view name) {
			std::string key;
			for (const char c : name) {
				if (rme::srv::isAlpha(c) || rme::srv::isDigit(c)) {
					key.push_back(rme::srv::toLower(c));
				}
			}
			return key;
		}

		// The server has no temple per town; the town's named mark (or the
		// veteran start) stands in for it.
		void addTowns(const rme::sec::MapInfo& info) {
			for (const rme::sec::MapDepot& depot : info.depots) {
				auto town = std::make_unique<Town>(townId(depot.id));
				town->setName(depot.name);
				const auto mark = std::ranges::find(info.marks, placeKey(depot.name), [](const rme::sec::MapMark& m) { return placeKey(m.name); });
				if (mark != info.marks.end()) {
					town->setTemplePosition(toPosition(mark->position));
				} else if (info.veteran_start) {
					town->setTemplePosition(toPosition(*info.veteran_start));
				}
				map_.towns.addTown(std::move(town));
			}
		}

		void addWaypoints(const rme::sec::MapInfo& info) {
			for (const rme::sec::MapMark& mark : info.marks) {
				map_.waypoints.addWaypoint(std::make_unique<Waypoint>(mark.name, toPosition(mark.position)));
			}
		}

		void addHouses(const std::vector<rme::sec::HouseArea>& areas, const std::vector<rme::sec::HouseDefinition>& houses) {
			size_t unknown_areas = 0;
			for (const rme::sec::HouseDefinition& definition : houses) {
				const auto area = std::ranges::find(areas, definition.area, &rme::sec::HouseArea::id);
				if (area == areas.end()) {
					++unknown_areas;
					continue;
				}

				auto new_house = std::make_unique<House>(map_);
				House* house = new_house.get();
				house->setID(static_cast<uint32_t>(definition.id));
				house->name = definition.name;
				house->townid = townId(area->depot);
				house->guildhall = definition.guild_house;
				// The server's rent: the area's price per square times the size, plus the offset
				house->rent = area->square_price * static_cast<int>(definition.fields.size()) + definition.rent_offset;
				if (!map_.houses.addHouse(std::move(new_house))) {
					warnings_.push_back(std::format("houses.dat: house {} is defined twice; the second one was skipped.", definition.id));
					continue;
				}

				for (const rme::sec::AbsolutePosition& field : definition.fields) {
					if (field.z < 0 || field.z >= MAP_LAYERS || field.x < 0 || field.y < 0) {
						continue;
					}
					Tile* tile = map_.getTile(field.x, field.y, field.z);
					if (!tile) {
						tile = map_.createTile(field.x, field.y, field.z);
					}
					house->addTile(tile);
				}
				if (definition.exit.x > 0) {
					house->setExit(toPosition(definition.exit));
				}
			}
			if (unknown_areas > 0) {
				warnings_.push_back(std::format("{} houses name an area that houseareas.dat does not define and were skipped.", unknown_areas));
			}
		}

		Map& map_;
		std::vector<std::string>& warnings_;
		std::optional<rme::sec::MapInfo> info_;
	};

	// The read-only markers (see game/srv_markers.h): the start positions of
	// map.dat, the tiles dat/moveuse.dat rules name and the raid waves of
	// mon/*.evt.
	class MarkerLoader {
	public:
		MarkerLoader(std::vector<std::string>& warnings, const rme::sec::MapInfo* info, const std::map<int, CreatureType*>& races) :
			warnings_(warnings), info_(info), races_(races) { }

		std::unique_ptr<rme::srv::ServerMarkers> load(const std::filesystem::path& data_folder) {
			if (info_) {
				addStart(info_->newbie_start, "Newbie start (NewbieStart in map.dat)");
				addStart(info_->veteran_start, "Veteran start (VeteranStart in map.dat)");
			}
			loadRules(data_folder / "dat" / "moveuse.dat");
			loadRaids(data_folder / "mon");
			markers_->finish();
			return std::move(markers_);
		}

	private:
		using MarkerKind = rme::srv::MarkerKind;
		using ScriptCall = rme::sec::ScriptCall;
		using ScriptValue = rme::sec::ScriptValue;

		static std::optional<Position> toPosition(const rme::sec::AbsolutePosition& position) {
			if (position.z < 0 || position.z >= MAP_LAYERS || position.x <= 0 || position.y <= 0 || position.x >= MAP_MAX_WIDTH || position.y >= MAP_MAX_HEIGHT) {
				return std::nullopt;
			}
			return Position(position.x, position.y, position.z);
		}

		void addStart(const std::optional<rme::sec::AbsolutePosition>& start, std::string text) {
			if (const auto position = start ? toPosition(*start) : std::nullopt) {
				markers_->add(*position, MarkerKind::Start, std::move(text));
			}
		}

		std::string depotName(int depot) const {
			if (info_) {
				if (const auto it = std::ranges::find(info_->depots, depot, &rme::sec::MapDepot::id); it != info_->depots.end()) {
					return std::format("{} (depot {})", it->name, depot);
				}
			}
			return std::format("depot {}", depot);
		}

		static const ScriptCall* findCall(const std::vector<ScriptCall>& calls, std::string_view name) {
			const auto it = std::ranges::find_if(calls, [&](const ScriptCall& call) { return rme::srv::equalsIgnoreCase(call.name, name); });
			return it != calls.end() ? &*it : nullptr;
		}

		static std::optional<int64_t> numberAt(const ScriptCall& call, size_t index) {
			if (index < call.arguments.size() && call.arguments[index].kind == ScriptValue::Kind::Number) {
				return call.arguments[index].number;
			}
			return std::nullopt;
		}

		// What a rule does with the position in argument `index` of `call`.
		static std::string_view roleOf(const ScriptCall& call, size_t index) {
			const auto is = [&](std::string_view name) { return rme::srv::equalsIgnoreCase(call.name, name); };
			if (is("IsPosition")) {
				return "triggers here";
			}
			if (is("IsObjectThere") || is("IsPlayerThere") || is("CountObjectsOnMap")) {
				return call.negated ? "needs this tile without it" : "checks this tile";
			}
			if (is("ChangeOnMap") || is("CreateOnMap") || is("DeleteOnMap") || is("DeleteTopOnMap")) {
				return "changes this tile";
			}
			if (is("MoveTopOnMap")) {
				return index == 0 ? "moves things off this tile" : "moves things here";
			}
			if (is("MoveTop") || is("Move")) {
				return "moves things here";
			}
			if (is("MonsterOnMap")) {
				return "summons a monster here";
			}
			if (is("SetStart")) {
				return "sets a player's start here";
			}
			return "names this tile";
		}

		// Calls that name positions the map does not need to keep: effects
		// only show an animation, and "Rel" calls use offsets, not positions.
		static bool skipsPositions(const ScriptCall& call) {
			return rme::srv::equalsIgnoreCase(call.name, "EffectOnMap") || (call.name.size() > 3 && rme::srv::equalsIgnoreCase(std::string_view(call.name).substr(call.name.size() - 3), "Rel"));
		}

		void loadRules(const std::filesystem::path& path) {
			std::string text;
			if (!readFile(path, text)) {
				warnings_.push_back("dat/moveuse.dat was not found; the tiles its rules use are not marked.");
				return;
			}
			const auto rules = rme::sec::readMoveUseRules(text);
			if (!rules) {
				warnings_.push_back(std::format("moveuse.dat:{}: {}; the tiles its rules use are not marked.", rules.error().line, rules.error().message));
				return;
			}
			for (const rme::sec::MoveUseRule& rule : *rules) {
				addRule(rule);
			}
		}

		void addRule(const rme::sec::MoveUseRule& rule) {
			// "Use lever": the event and the object type its IsType condition names.
			std::string subject = rule.event;
			if (const ScriptCall* is_type = findCall(rule.conditions, "IsType")) {
				if (const auto type = numberAt(*is_type, 1)) {
					const auto definition = g_item_definitions.get(static_cast<uint16_t>(*type));
					const std::string_view name = definition ? definition.name() : std::string_view {};
					subject += name.empty() ? std::format(" object {}", *type) : std::format(" {}", name);
				}
			}
			const std::string context = rule.name.empty() ? rule.section : std::format("\"{}\"", rule.name);

			std::vector<std::pair<int, std::string>> quests;
			for (const ScriptCall& condition : rule.conditions) {
				// HasQuestValue (Obj2,3,= ,0)
				if (rme::srv::equalsIgnoreCase(condition.name, "HasQuestValue") && condition.arguments.size() >= 4) {
					if (const auto quest = numberAt(condition, 1)) {
						const auto value = numberAt(condition, 3).value_or(0);
						quests.emplace_back(static_cast<int>(*quest), std::format("{}needs quest {} {} {}", condition.negated ? "not " : "", *quest, condition.arguments[2].text, value));
					}
				}
			}
			for (const ScriptCall& action : rule.actions) {
				// SetQuestValue(Obj2,3,1)
				if (rme::srv::equalsIgnoreCase(action.name, "SetQuestValue")) {
					if (const auto quest = numberAt(action, 1)) {
						quests.emplace_back(static_cast<int>(*quest), std::format("sets quest {} to {}", *quest, numberAt(action, 2).value_or(0)));
					}
				}
			}
			std::string quest_text;
			for (const auto& [quest, description] : quests) {
				quest_text += "; " + description;
			}

			// LoadDepot([chest],type,Obj2,depot) and SaveDepot: a depot booth.
			const ScriptCall* depot = findCall(rule.actions, "LoadDepot");
			if (!depot) {
				depot = findCall(rule.actions, "SaveDepot");
			}
			const std::optional<int64_t> depot_id = depot ? numberAt(*depot, 3) : std::nullopt;

			for (const auto* calls : { &rule.conditions, &rule.actions }) {
				for (const ScriptCall& call : *calls) {
					if (skipsPositions(call)) {
						continue;
					}
					for (size_t index = 0; index < call.arguments.size(); ++index) {
						if (call.arguments[index].kind != ScriptValue::Kind::Coordinate) {
							continue;
						}
						const auto position = toPosition(call.arguments[index].position);
						if (!position) {
							continue;
						}
						if (depot_id) {
							const bool chest = rme::srv::equalsIgnoreCase(call.name, "LoadDepot") || rme::srv::equalsIgnoreCase(call.name, "SaveDepot");
							markers_->add(*position, MarkerKind::Depot, std::format("{} {}", chest ? "Depot chest of" : "Opens the depot chest of", depotName(static_cast<int>(*depot_id))));
							continue;
						}
						markers_->add(*position, MarkerKind::Script, std::format("{}: {}{} ({})", subject, roleOf(call, index), quest_text, context));
						for (const auto& [quest, description] : quests) {
							markers_->addQuest(*position, quest, std::format("{}: {} ({})", subject, description, context));
						}
					}
				}
			}
		}

		std::string raceName(int race) const {
			const auto it = races_.find(race);
			return it != races_.end() && it->second ? it->second->name : std::format("race {}", race);
		}

		void loadRaids(const std::filesystem::path& folder) {
			std::error_code ec;
			std::string text;
			for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
				if (!entry.is_regular_file(ec) || !rme::srv::equalsIgnoreCase(entry.path().extension().string(), ".evt")) {
					continue;
				}
				const std::string file_name = FileSystem::PathToUtf8(entry.path().filename());
				if (!readFile(entry.path(), text)) {
					warnings_.push_back(std::format("{}: could not be read, skipped.", file_name));
					continue;
				}
				const auto raid = rme::sec::readRaid(text);
				if (!raid) {
					warnings_.push_back(std::format("{}:{}: {}; raid skipped.", file_name, raid.error().line, raid.error().message));
					continue;
				}
				addRaid(FileSystem::PathToUtf8(entry.path().stem()), *raid);
			}
		}

		// One line per raid and position: its waves there, how many monsters
		// they bring and which.
		void addRaid(const std::string& name, const rme::sec::Raid& raid) {
			struct Spot {
				size_t waves = 0;
				int min_count = 0;
				int max_count = 0;
				int spread = 0;
				std::vector<std::string> races;
			};
			std::map<Position, Spot> spots;
			for (const rme::sec::RaidWave& wave : raid.waves) {
				const auto position = toPosition(wave.position);
				if (!position) {
					continue;
				}
				Spot& spot = spots[*position];
				++spot.waves;
				spot.min_count += wave.min_count;
				spot.max_count += wave.max_count;
				spot.spread = std::max(spot.spread, wave.spread);
				std::string race = raceName(wave.race);
				if (std::ranges::find(spot.races, race) == spot.races.end()) {
					spot.races.push_back(std::move(race));
				}
			}

			constexpr int64_t kSecondsPerDay = 24 * 60 * 60;
			const std::string when = raid.interval > 0 ? std::format("about every {} days", std::max<int64_t>(1, raid.interval / kSecondsPerDay)) : "on a set date";
			for (const auto& [position, spot] : spots) {
				std::string races;
				for (const std::string& race : spot.races) {
					races += (races.empty() ? "" : ", ") + race;
				}
				markers_->add(position, MarkerKind::Raid, std::format("{}: {} {}, {}-{} {} within {} ({}, {})", name, spot.waves, spot.waves == 1 ? "wave" : "waves", spot.min_count, spot.max_count, races, spot.spread, raid.type, when));
				markers_->addRaidSpread(position, spot.spread);
			}
		}

		std::vector<std::string>& warnings_;
		const rme::sec::MapInfo* info_;
		const std::map<int, CreatureType*>& races_;
		std::unique_ptr<rme::srv::ServerMarkers> markers_ = std::make_unique<rme::srv::ServerMarkers>();
	};

	uint32_t toMapFlags(const SectorTile& sector_tile) {
		uint32_t flags = TILESTATE_NONE;
		if (sector_tile.hasFlag(TileFlag::ProtectionZone)) {
			flags |= TILESTATE_PROTECTIONZONE;
		}
		if (sector_tile.hasFlag(TileFlag::NoLogout)) {
			flags |= TILESTATE_NOLOGOUT;
		}
		if (sector_tile.hasFlag(TileFlag::Refresh)) {
			flags |= TILESTATE_REFRESH;
		}
		return flags;
	}

} // namespace

bool IOMapSec::isSectorMap(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	if (std::filesystem::is_directory(path, ec)) {
		for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
			if (isSectorFile(entry.path())) {
				return true;
			}
		}
		return false;
	}
	return isSectorFile(path);
}

std::filesystem::path IOMapSec::sectorFolder(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	return std::filesystem::is_directory(path, ec) ? path : path.parent_path();
}

bool IOMapSec::isSectorTarget(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	return std::filesystem::is_directory(path, ec) || rme::srv::equalsIgnoreCase(path.extension().string(), rme::sec::kSectorFileExtension);
}

wxFileName IOMapSec::objectsFile(const FileName& identifier) {
	// Server data layout: <data>/<map folder>/*.sec and <data>/dat/objects.srv
	const std::filesystem::path path = sectorFolder(identifier).parent_path() / "dat" / "objects.srv";
	return wxFileName(wxString(path.wstring()));
}

ClientVersion* IOMapSec::findClientVersion() {
	const auto matches = [](const ClientVersion* version) {
		return version && version->getItemDefinitionMode() == ItemDefinitionMode::DatSrv;
	};

	if (ClientVersion* loaded = g_version.getLoadedVersion(); matches(loaded)) {
		return loaded;
	}

	ClientVersion* best = nullptr;
	for (ClientVersion* version : ClientVersion::getAllVisible()) {
		if (!matches(version)) {
			continue;
		}
		if (!best || (version->hasConfiguredClientPath() && !best->hasConfiguredClientPath())) {
			best = version;
		}
	}
	return best;
}

bool IOMapSec::peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info) {
	out_info = {};
	out_info.is_sector_map = true;

	const std::filesystem::path folder = sectorFolder(identifier);
	out_info.map_name = wxstr(FileSystem::PathToUtf8(folder.filename()));
	wxDateTime modified_time;
	if (identifier.GetTimes(nullptr, &modified_time, nullptr)) {
		out_info.modified_time = modified_time;
	}

	const std::vector<SectorFile> files = listSectorFiles(folder);
	if (files.empty()) {
		out_info.has_error = true;
		out_info.error_message = wxstr(std::format("No sector files (XXXX-YYYY-ZZ.sec) found in \"{}\".", FileSystem::PathToUtf8(folder)));
		return false;
	}

	const wxFileName objects = objectsFile(identifier);
	if (!objects.FileExists()) {
		out_info.has_error = true;
		out_info.error_message = wxstr(std::format("objects.srv was not found at \"{}\".", nstr(objects.GetFullPath())));
		return false;
	}

	int max_sector_x = 0;
	int max_sector_y = 0;
	for (const SectorFile& file : files) {
		max_sector_x = std::max(max_sector_x, file.coordinates.x);
		max_sector_y = std::max(max_sector_y, file.coordinates.y);
	}
	const auto extent = [](int max_sector) {
		return static_cast<uint16_t>(std::clamp((max_sector + 1) * rme::sec::kSectorSize, 0, 0xFFFF));
	};
	out_info.width = extent(max_sector_x);
	out_info.height = extent(max_sector_y);
	out_info.description = wxstr(std::format("Sector map: {} sector files. Items from {}", files.size(), nstr(objects.GetFullPath())));
	return true;
}

bool IOMapSec::loadMap(Map& map, const FileName& identifier) {
	if (!g_version.IsVersionLoaded() || g_version.GetCurrentVersion().getItemDefinitionMode() != ItemDefinitionMode::DatSrv) {
		errorstr = "Sector maps need a client version that reads its items from objects.srv (configType dat_srv).";
		return false;
	}

	const ClientVersion& client = g_version.GetCurrentVersion();
	version.otbm = client.getPrefferedMapVersionID();
	version.client = client.getProtocolID();
	version.items_major = client.getOTBVersion().format_version;

	const std::filesystem::path folder = sectorFolder(identifier);
	std::vector<SectorFile> files = listSectorFiles(folder);
	if (files.empty()) {
		errorstr = wxstr(std::format("No sector files (XXXX-YYYY-ZZ.sec) found in \"{}\".", FileSystem::PathToUtf8(folder)));
		return false;
	}
	std::ranges::sort(files, {}, [](const SectorFile& file) {
		return std::tuple(file.coordinates.z, file.coordinates.y, file.coordinates.x);
	});

	const auto started = std::chrono::steady_clock::now();
	LoadStats stats;
	ItemBuilder builder(stats);
	std::string text;
	int max_x = 0;
	int max_y = 0;
	size_t loaded_sectors = 0;

	for (size_t index = 0; index < files.size(); ++index) {
		const SectorFile& file = files[index];
		if (index % 64 == 0) {
			const int percent = static_cast<int>(index * 100 / files.size());
			g_gui.SetLoadDone(percent, wxstr(std::format("Loading sectors ({}/{})...", index, files.size())));
		}

		const std::string file_name = FileSystem::PathToUtf8(file.path.filename());
		const auto [sector_x, sector_y, sector_z] = file.coordinates;
		const int base_x = sector_x * rme::sec::kSectorSize;
		const int base_y = sector_y * rme::sec::kSectorSize;
		if (sector_z < 0 || sector_z >= MAP_LAYERS || base_x < 0 || base_y < 0 || base_x + rme::sec::kSectorSize > MAP_MAX_WIDTH || base_y + rme::sec::kSectorSize > MAP_MAX_HEIGHT) {
			warnings.push_back(std::format("{}: sector is outside the map area, skipped.", file_name));
			continue;
		}

		if (!readFile(file.path, text)) {
			warnings.push_back(std::format("{}: could not be read, skipped.", file_name));
			continue;
		}

		const auto sector = rme::sec::parseSector(text);
		if (!sector) {
			warnings.push_back(std::format("{}:{}: {}; sector skipped.", file_name, sector.error().line, sector.error().message));
			continue;
		}

		for (const SectorTile& sector_tile : sector->tiles) {
			const int x = base_x + sector_tile.offset_x;
			const int y = base_y + sector_tile.offset_y;
			Tile* tile = map.createTile(x, y, sector_z);
			if (!tile) {
				continue;
			}
			tile->addMapFlags(toMapFlags(sector_tile));
			addObjects(*tile, sector_tile.objects, builder, stats);
			TileOperations::update(tile);
			max_x = std::max(max_x, x);
			max_y = std::max(max_y, y);
		}
		++loaded_sectors;
	}

	map.setWidth(max_x + 1);
	map.setHeight(max_y + 1);
	map.setMapDescription(std::format("Imported from sector files in {}", FileSystem::PathToUtf8(folder)));

	g_gui.SetLoadDone(100, "Loading towns, houses, monsters and NPCs...");
	WorldLoader world(map, warnings);
	world.load(folder.parent_path() / "dat");
	CreatureLoader creatures(map, warnings);
	creatures.load(folder.parent_path());
	map.server_markers = MarkerLoader(warnings, world.mapInfo(), creatures.races()).load(folder.parent_path());

	if (const rme::sec::MapInfo* info = world.mapInfo(); info && info->sectors) {
		const auto outside = [&](const SectorFile& file) { return !info->sectors->contains(file.coordinates); };
		if (const auto first = std::ranges::find_if(files, outside); first != files.end()) {
			warnings.push_back(std::format("{} sector files lie outside the sectors map.dat lets the server load ({}-{}, {}-{}, floors {}-{}), starting with {}; the server ignores them.", std::ranges::count_if(files, outside), info->sectors->min.x, info->sectors->max.x, info->sectors->min.y, info->sectors->max.y, info->sectors->min.z, info->sectors->max.z, FileSystem::PathToUtf8(first->path.filename())));
		}
	}

	if (!stats.unknown_types.empty()) {
		size_t total = 0;
		size_t listed_count = 0;
		std::string listed;
		for (const auto& [type_id, count] : stats.unknown_types) {
			total += count;
			if (listed_count++ < kMaxListedUnknownTypes) {
				listed += std::format("{}{} (x{})", listed.empty() ? "" : ", ", type_id, count);
			}
		}
		warnings.push_back(std::format("{} objects of {} type IDs are not defined in objects.srv and were skipped: {}{}", total, stats.unknown_types.size(), listed, stats.unknown_types.size() > kMaxListedUnknownTypes ? ", ..." : ""));
	}
	if (stats.dropped_contents > 0) {
		warnings.push_back(std::format("{} objects had contents but are not containers in objects.srv; the contents were skipped.", stats.dropped_contents));
	}
	if (stats.replaced_grounds > 0) {
		warnings.push_back(std::format("{} tiles listed more than one ground item; only the last one was kept.", stats.replaced_grounds));
	}
	// OTBM 4 stores them in the item attribute map; older versions drop them on save.
	if (stats.custom_attributes > 0 && version.otbm != MAP_OTBM_4) {
		warnings.push_back(std::format("{} server attributes (key and keyhole numbers, quest chests, door levels and quests, expiry times) are kept on the items, but this client saves OTBM version {}, which drops them. OTBM version 4 keeps them.", stats.custom_attributes, static_cast<int>(version.otbm) + 1));
	}

	spdlog::info("IOMapSec: loaded {} of {} sectors ({} tiles) from {} in {:.2f} s", loaded_sectors, files.size(), map.getTileCount(), FileSystem::PathToUtf8(folder), std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());

	if (loaded_sectors == 0) {
		errorstr = "None of the sector files could be loaded.";
		return false;
	}
	return true;
}
