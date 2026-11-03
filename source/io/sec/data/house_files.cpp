//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/house_files.h"

#include "game/house.h"
#include "game/town.h"
#include "io/sec/data/map_dat_file.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/srv_world_files.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <spdlog/spdlog.h>
#include <tuple>

namespace rme::sec::house_files {

	namespace {

		AbsolutePosition toAbsolute(const Position& position) {
			return AbsolutePosition { .x = position.x, .y = position.y, .z = position.z };
		}

		void addHouses(Map& map, const std::vector<HouseArea>& areas, const std::vector<HouseDefinition>& houses, std::vector<std::string>& warnings) {
			size_t unknown_areas = 0;
			for (const HouseDefinition& definition : houses) {
				const auto area = std::ranges::find(areas, definition.area, &HouseArea::id);
				if (area == areas.end()) {
					++unknown_areas;
					continue;
				}

				auto new_house = std::make_unique<House>(map);
				House* house = new_house.get();
				house->setID(static_cast<uint32_t>(definition.id));
				house->name = definition.name;
				house->townid = townIdOfDepot(area->depot);
				house->guildhall = definition.guild_house;
				// The server's rent: the area's price per square times the size, plus the offset
				house->rent = area->square_price * static_cast<int>(definition.fields.size()) + definition.rent_offset;
				if (!map.houses.addHouse(std::move(new_house))) {
					warnings.push_back(std::format("houses.dat: house {} is defined twice; the second one was skipped.", definition.id));
					continue;
				}

				for (const AbsolutePosition& field : definition.fields) {
					if (field.z < 0 || field.z >= MAP_LAYERS || field.x < 0 || field.y < 0) {
						continue;
					}
					Tile* tile = map.getTile(field.x, field.y, field.z);
					if (!tile) {
						tile = map.createTile(field.x, field.y, field.z);
					}
					house->addTile(tile);
				}
				if (definition.exit.x > 0) {
					house->setExit(Position(definition.exit.x, definition.exit.y, definition.exit.z));
				}
			}
			if (unknown_areas > 0) {
				warnings.push_back(std::format("{} houses name an area that houseareas.dat does not define and were skipped.", unknown_areas));
			}
		}

		// A house area for a town that has none: named after the town, with its
		// ID in the town's hundred (the original areas use 1xx for depot 0) and
		// no price per square, so the rent offset carries the whole rent.
		HouseArea newArea(const std::vector<HouseArea>& areas, int depot, const std::string& town_name) {
			const auto taken = [&areas](int id) { return std::ranges::any_of(areas, [id](const HouseArea& area) { return area.id == id; }); };
			const int first = (depot + 1) * 100;
			int id = first;
			while (taken(id) && id < first + 99) {
				++id;
			}
			if (taken(id)) {
				id = std::ranges::max(areas, {}, &HouseArea::id).id + 1;
			}
			return HouseArea { .id = id, .name = town_name, .square_price = 0, .depot = depot };
		}

	} // namespace

	void load(Map& map, const ServerDataFolder& folder, std::vector<std::string>& warnings) {
		std::string areas_text;
		std::string houses_text;
		if (!readFile(folder.dat() / "houseareas.dat", areas_text) || !readFile(folder.dat() / "houses.dat", houses_text)) {
			spdlog::info("IOMapSec: no dat/houseareas.dat or dat/houses.dat, so the map has no houses yet");
			return;
		}
		const auto areas = readHouseAreas(areas_text);
		if (!areas) {
			warnings.push_back(std::format("houseareas.dat:{}: {}; no houses were loaded.", areas.error().line, areas.error().message));
			return;
		}
		const auto houses = readHouses(houses_text);
		if (!houses) {
			warnings.push_back(std::format("houses.dat:{}: {}; no houses were loaded.", houses.error().line, houses.error().message));
			return;
		}
		addHouses(map, *areas, *houses, warnings);
	}

	void save(Map& map, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings) {
		const std::filesystem::path houses_path = folder.dat() / "houses.dat";
		const std::filesystem::path areas_path = folder.dat() / "houseareas.dat";
		std::string houses_text;
		std::string areas_text;
		const bool has_houses_file = readFile(houses_path, houses_text);
		const bool has_areas_file = readFile(areas_path, areas_text);
		if (!has_houses_file && map.houses.count() == 0) {
			return;
		}
		std::expected<std::vector<HouseDefinition>, ParseError> original = std::vector<HouseDefinition> {};
		if (has_houses_file) {
			original = readHouses(houses_text);
		}
		std::expected<std::vector<HouseArea>, ParseError> areas = std::vector<HouseArea> {};
		if (has_areas_file) {
			areas = readHouseAreas(areas_text);
		}
		if (!original || !areas) {
			warnings.push_back("dat/houses.dat or dat/houseareas.dat could not be read; houses were not saved.");
			return;
		}

		std::vector<const House*> houses;
		for (const auto& [id, house] : map.houses) {
			houses.push_back(house.get());
		}
		std::ranges::sort(houses, {}, &House::getID);

		std::vector<HouseDefinition> definitions;
		std::vector<HouseArea> new_areas;
		size_t without_town = 0;
		for (const House* house : houses) {
			const auto previous = std::ranges::find(*original, static_cast<int>(house->getID()), &HouseDefinition::id);
			HouseDefinition definition = previous != original->end() ? *previous : HouseDefinition { .id = static_cast<int>(house->getID()) };

			// Towns are depots; keep the house's area while its depot still
			// matches, else take the town's first area, or give the town one.
			const int depot = depotOfTownId(house->townid);
			auto area = std::ranges::find(*areas, definition.area, &HouseArea::id);
			if (area == areas->end() || area->depot != depot) {
				area = std::ranges::find(*areas, depot, &HouseArea::depot);
			}
			if (area == areas->end()) {
				const Town* town = map.towns.getTown(house->townid);
				if (!town) {
					++without_town;
					continue;
				}
				new_areas.push_back(newArea(*areas, depot, town->getName()));
				areas->push_back(new_areas.back());
				area = areas->end() - 1;
			}

			std::vector<AbsolutePosition> fields;
			for (const Position& position : house->getTiles()) {
				fields.push_back(toAbsolute(position));
			}
			const auto field_order = [](const AbsolutePosition& p) { return std::tuple(p.z, p.y, p.x); };
			std::ranges::sort(fields, {}, field_order);
			// Unchanged fields keep the order the file has them in.
			std::vector<AbsolutePosition> previous_fields = definition.fields;
			std::ranges::sort(previous_fields, {}, field_order);
			if (previous_fields == fields) {
				fields = definition.fields;
			}

			// The server's rent is the area's price per square times the
			// size plus the offset: keep the offset unless the rent was edited.
			const int previous_size = static_cast<int>(definition.fields.size());
			const auto previous_area = std::ranges::find(*areas, definition.area, &HouseArea::id);
			const int previous_rent = previous_area != areas->end() ? previous_area->square_price * previous_size + definition.rent_offset : -1;
			if (house->rent != previous_rent) {
				definition.rent_offset = house->rent - area->square_price * static_cast<int>(fields.size());
			}

			definition.name = house->name;
			definition.area = area->id;
			definition.guild_house = house->guildhall;
			definition.exit = toAbsolute(house->getExit());
			if (previous == original->end() && !fields.empty()) {
				const auto [min_x, max_x] = std::ranges::minmax(fields | std::views::transform(&AbsolutePosition::x));
				const auto [min_y, max_y] = std::ranges::minmax(fields | std::views::transform(&AbsolutePosition::y));
				definition.center = AbsolutePosition { .x = (min_x + max_x) / 2, .y = (min_y + max_y) / 2, .z = definition.exit.z };
			}
			definition.fields = std::move(fields);
			definitions.push_back(std::move(definition));
		}
		if (without_town > 0) {
			warnings.push_back(std::format("{} houses have no town, which their house area needs, and were not saved.", without_town));
		}
		if (!new_areas.empty()) {
			writer.write(areas_path, appendHouseAreas(areas_text, new_areas));
		}
		// Written only when a house changed; how the file formats them does not count.
		std::vector<HouseDefinition> previous = *original;
		std::ranges::sort(previous, {}, &HouseDefinition::id);
		if (has_houses_file && previous == definitions) {
			return;
		}
		writer.write(houses_path, writeHouses(definitions, has_houses_file ? newlineOf(houses_text) : std::string_view("\r\n")));
	}

} // namespace rme::sec::house_files
