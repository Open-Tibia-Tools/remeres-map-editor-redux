#ifndef RME_MAP_INITIAL_POSITION_RESOLVER_H_
#define RME_MAP_INITIAL_POSITION_RESOLVER_H_

#include "map/position.h"
#include <string_view>

class Map;

/**
 * @class MapInitialPositionResolver
 * @brief Resolves the initial camera viewport position when a map is loaded.
 *
 * Implements a 4-tier fallback hierarchy:
 * 1. Last remembered position saved in application settings for this map file.
 * 2. First town temple position defined on the map.
 * 3. First non-empty ground tile (prioritizing floor 7, then any floor).
 * 4. First tile containing any item.
 * 5. Default fallback to geometric map center (width / 2, height / 2, 7).
 */
class MapInitialPositionResolver {
public:
	[[nodiscard]] static Position Resolve(Map& map, std::string_view mapPath);
};

#endif // RME_MAP_INITIAL_POSITION_RESOLVER_H_
