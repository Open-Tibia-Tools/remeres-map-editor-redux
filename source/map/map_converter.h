#ifndef RME_MAP_CONVERTER_H_
#define RME_MAP_CONVERTER_H_

#include <cstdint>
#include <functional>
#include <string_view>
#include "map/map.h"

class MapConverter {
public:
	using ProgressCallback = std::function<void(int percent, std::string_view msg)>;

	static bool convert(Map& map, MapVersion to);
	static bool convert(Map& map, const ConversionMap& cm, ProgressCallback progress_cb = nullptr);
	static void cleanInvalidTiles(Map& map, ProgressCallback progress_cb = nullptr);
	static void cleanInvalidZones(Map& map, ProgressCallback progress_cb = nullptr);
	static void convertHouseTiles(Map& map, uint32_t fromId, uint32_t toId, ProgressCallback progress_cb = nullptr);
};

#endif
