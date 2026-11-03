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

		// The server has no temple per town; the town's named mark (or the
		// veteran start) stands in for it.
		void addTowns(Map& map, const MapInfo& info) {
			for (const MapDepot& depot : info.depots) {
				auto town = std::make_unique<Town>(townIdOfDepot(depot.id));
				town->setName(depot.name);
				const auto mark = std::ranges::find(info.marks, placeKey(depot.name), [](const MapMark& m) { return placeKey(m.name); });
				if (mark != info.marks.end()) {
					town->setTemplePosition(toPosition(mark->position));
				} else if (info.veteran_start) {
					town->setTemplePosition(toPosition(*info.veteran_start));
				}
				map.towns.addTown(std::move(town));
			}
		}

		void addWaypoints(Map& map, const MapInfo& info) {
			for (const MapMark& mark : info.marks) {
				map.waypoints.addWaypoint(std::make_unique<Waypoint>(mark.name, toPosition(mark.position)));
			}
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
		addTowns(map, *info);
		addWaypoints(map, *info);
		return std::move(*info);
	}

	void save(const Map& map, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings) {
		const std::filesystem::path path = folder.dat() / "map.dat";
		std::string text;
		if (!readFile(path, text)) {
			warnings.push_back("dat/map.dat was not found; waypoints were not saved.");
			return;
		}
		const auto info = readMapInfo(text);
		if (!info) {
			warnings.push_back("dat/map.dat could not be read; waypoints were not saved.");
			return;
		}

		// Known marks keep their place in map.dat.
		std::vector<MapMark> marks;
		for (const auto& [key, waypoint] : map.waypoints) {
			const Position& position = waypoint->pos;
			marks.push_back(MapMark { .name = waypoint->name, .position = AbsolutePosition { .x = position.x, .y = position.y, .z = position.z } });
		}
		const auto order = [&info](const MapMark& mark) {
			const auto it = std::ranges::find(info->marks, mark.name, &MapMark::name);
			return std::pair(std::distance(info->marks.begin(), it), mark.name);
		};
		std::ranges::sort(marks, {}, order);
		writer.write(path, updateMapMarks(text, marks));
	}

} // namespace rme::sec::map_dat
