//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/map_dat_file.h"

#include "game/town.h"
#include "game/waypoints.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/srv_script_lexer.h"
#include "map/map.h"

#include <algorithm>
#include <format>
#include <iterator>

namespace rme::sec::map_dat {

	namespace {

		Position toPosition(const AbsolutePosition& position) {
			return Position(position.x, position.y, position.z);
		}

		AbsolutePosition toAbsolute(const Position& position) {
			return AbsolutePosition { .x = position.x, .y = position.y, .z = position.z };
		}

		// "Port Hope" and the mark "PortHope" name the same place.
		std::string placeKey(std::string_view name) {
			std::string key;
			for (const char c : name) {
				if (srv::isAlpha(c) || srv::isDigit(c)) {
					key.push_back(srv::toLower(c));
				}
			}
			return key;
		}

		// The mark of the town named `town_name`, if map.dat has one.
		const MapMark* townMark(const MapInfo& info, std::string_view town_name) {
			const auto mark = std::ranges::find(info.marks, placeKey(town_name), [](const MapMark& m) { return placeKey(m.name); });
			return mark != info.marks.end() ? &*mark : nullptr;
		}

		// A town per depot. The server has no temples: the mark named like
		// the town stands in for its temple, else the veteran start does.
		// A town's mark belongs to the town, so it is no waypoint as well.
		void addTownsAndWaypoints(Map& map, const MapInfo& info) {
			std::vector<const MapMark*> town_marks;
			for (const MapDepot& depot : info.depots) {
				auto town = std::make_unique<Town>(townIdOfDepot(depot.id));
				town->setName(depot.name);
				if (const MapMark* mark = townMark(info, depot.name)) {
					town->setTemplePosition(toPosition(mark->position));
					town_marks.push_back(mark);
				} else if (info.veteran_start) {
					town->setTemplePosition(toPosition(*info.veteran_start));
				}
				map.towns.addTown(std::move(town));
			}
			for (const MapMark& mark : info.marks) {
				if (std::ranges::find(town_marks, &mark) == town_marks.end()) {
					map.waypoints.addWaypoint(std::make_unique<Waypoint>(mark.name, toPosition(mark.position)));
				}
			}
		}

		// The towns as depots, keeping each depot's third value.
		std::vector<MapDepot> depotsOf(const Map& map, const MapInfo& previous) {
			std::vector<MapDepot> depots;
			for (const auto& [id, town] : map.towns) {
				if (!town || id == 0) {
					continue;
				}
				MapDepot depot { .id = depotOfTownId(id), .name = town->getName() };
				if (const auto it = std::ranges::find(previous.depots, depot.id, &MapDepot::id); it != previous.depots.end()) {
					depot.capacity = it->capacity;
				}
				depots.push_back(std::move(depot));
			}
			std::ranges::sort(depots, {}, &MapDepot::id);
			return depots;
		}

		// The waypoints and each town's mark at its temple. A town's mark
		// keeps its name in map.dat; a town without one gets a mark when its
		// temple is moved off the veteran start, which stood in for it.
		std::vector<MapMark> marksOf(const Map& map, const MapInfo& previous) {
			std::vector<MapMark> marks;
			for (const auto& [id, town] : map.towns) {
				if (!town) {
					continue;
				}
				const Position& temple = town->getTemplePosition();
				const AbsolutePosition position = toAbsolute(temple);
				if (const MapMark* mark = townMark(previous, town->getName())) {
					marks.push_back(MapMark { .name = mark->name, .position = position });
				} else if (temple.isValid() && position != previous.veteran_start) {
					marks.push_back(MapMark { .name = town->getName(), .position = position });
				}
			}
			for (const auto& [key, waypoint] : map.waypoints) {
				marks.push_back(MapMark { .name = waypoint->name, .position = toAbsolute(waypoint->pos) });
			}
			// Known marks keep their place in map.dat.
			const auto order = [&previous](const MapMark& mark) {
				const auto it = std::ranges::find(previous.marks, mark.name, &MapMark::name);
				return std::pair(std::distance(previous.marks.begin(), it), mark.name);
			};
			std::ranges::sort(marks, {}, order);
			return marks;
		}

	} // namespace

	std::optional<MapInfo> load(Map& map, const ServerDataFolder& folder, std::vector<std::string>& warnings) {
		std::string text;
		if (!readFile(folder.dat() / "map.dat", text)) {
			warnings.push_back("dat/map.dat was not found; no towns or waypoints were loaded.");
			return std::nullopt;
		}
		auto info = readMapInfo(text);
		if (!info) {
			warnings.push_back(std::format("map.dat:{}: {}; no towns or waypoints were loaded.", info.error().line, info.error().message));
			return std::nullopt;
		}
		addTownsAndWaypoints(map, *info);
		return std::move(*info);
	}

	void save(const Map& map, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings) {
		const std::filesystem::path path = folder.dat() / "map.dat";
		std::string text;
		if (!readFile(path, text)) {
			warnings.push_back("dat/map.dat was not found; towns and waypoints were not saved.");
			return;
		}
		const auto info = readMapInfo(text);
		if (!info) {
			warnings.push_back("dat/map.dat could not be read; towns and waypoints were not saved.");
			return;
		}
		writer.write(path, updateMapDat(text, marksOf(map, *info), depotsOf(map, *info)));
	}

} // namespace rme::sec::map_dat
