//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SRV_WORLD_FILES_H_
#define RME_IO_SEC_SRV_WORLD_FILES_H_

// Readers for the world data of the original game server:
//   dat/map.dat         sector range, named marks, depots (the towns) and start positions
//   dat/houseareas.dat  house areas: name, price per square, depot
//   dat/houses.dat      houses: name, rent offset, area, exit and fields

#include "io/sec/sec_sector_parser.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rme::sec {

	struct MapMark {
		std::string name;
		AbsolutePosition position;
	};

	// The third value of every Depot line in the original map.dat.
	inline constexpr int kDefaultDepotCapacity = 1000;

	// A town: players keep their items in its depot.
	struct MapDepot {
		int id = 0;
		std::string name;
		// Depot = (0,"Thais",1000): most likely how many items a depot holds;
		// the editor keeps it as it is.
		int capacity = kDefaultDepotCapacity;

		bool operator==(const MapDepot&) const = default;
	};

	// The sectors the server loads (map.dat SectorXMin ... SectorZMax, inclusive).
	struct SectorRange {
		SectorCoordinates min;
		SectorCoordinates max;

		[[nodiscard]] bool contains(const SectorCoordinates& sector) const {
			return sector.x >= min.x && sector.x <= max.x && sector.y >= min.y && sector.y <= max.y && sector.z >= min.z && sector.z <= max.z;
		}
	};

	struct MapInfo {
		std::optional<SectorRange> sectors; // set when map.dat names all six bounds
		std::vector<MapMark> marks;
		std::vector<MapDepot> depots;
		std::optional<AbsolutePosition> newbie_start;
		std::optional<AbsolutePosition> veteran_start;
	};

	struct HouseArea {
		int id = 0;
		std::string name;
		int square_price = 0;
		int depot = 0;
	};

	struct HouseDefinition {
		int id = 0;
		std::string name;
		std::string description;
		int rent_offset = 0;
		int area = 0;
		bool guild_house = false;
		AbsolutePosition exit;
		AbsolutePosition center;
		std::vector<AbsolutePosition> fields;

		bool operator==(const HouseDefinition&) const = default;
	};

	[[nodiscard]] std::expected<MapInfo, ParseError> readMapInfo(std::string_view text);
	[[nodiscard]] std::expected<std::vector<HouseArea>, ParseError> readHouseAreas(std::string_view text);
	[[nodiscard]] std::expected<std::vector<HouseDefinition>, ParseError> readHouses(std::string_view text);

	// houses.dat in the server's layout, houses in the given order separated
	// by a blank line, each line ending in `newline` (the file uses CRLF).
	[[nodiscard]] std::string writeHouses(const std::vector<HouseDefinition>& houses, std::string_view newline);

	// map.dat with its Mark and Depot lines replaced by `marks` and `depots`,
	// each written where the first line of its kind was, or at the end when
	// there was none; every other line is kept as it is.
	[[nodiscard]] std::string updateMapDat(std::string_view text, const std::vector<MapMark>& marks, const std::vector<MapDepot>& depots);

} // namespace rme::sec

#endif
