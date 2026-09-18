#include "editor/operations/map_version_changer.h"

#include "editor/editor.h"
#include "editor/action_queue.h"
#include "app/managers/version_manager.h"
#include "game/creatures.h"
#include "game/creature.h"
#include "map/tile.h"

#include <unordered_map>

struct MapConversionContext {
	struct CreatureInfo {
		std::string name;
		bool is_npc;
		Outfit outfit;
	};
	using CreatureMap = std::unordered_map<std::string, CreatureInfo>;
	CreatureMap creature_types;

	void operator()(Map& map, Tile* tile, long long done) {
		if (tile->creature) {
			CreatureMap::iterator f = creature_types.find(tile->creature->getName());
			if (f == creature_types.end()) {
				CreatureInfo info = {
					tile->creature->getName(),
					tile->creature->isNpc(),
					tile->creature->getLookType()
				};
				creature_types[tile->creature->getName()] = info;
			}
		}
	}
};

MapVersionChangeResult MapVersionChanger::changeMapVersion(Editor& editor, MapVersion new_ver, const MapConversionOptions& options) {
	MapVersionChangeResult result;
	Map& map = editor.map;
	MapVersion old_ver = map.getVersion();

	if (new_ver.client != old_ver.client) {
		// Switch version
		editor.selection.clear();
		editor.actionQueue->clear();

		if (new_ver.client < old_ver.client) {
			// Remember all creatures types on the map
			MapConversionContext conversion_context;
			foreach_TileOnMap(map, conversion_context);

			// Perform the conversion
			map.convert(new_ver);

			// Load the new version
			ClientVersion* target = ClientVersion::getBestMatch(new_ver.client);
			std::string load_error_1;
			if (!target || !g_version.LoadVersion(target->getID(), load_error_1, result.warnings)) {
				result.error = load_error_1;
				if (result.error.empty() && !target) {
					result.error = "No compatible client version found for this protocol.";
				}
				return result;
			}

			// Remove all creatures that were present are present in the new version
			std::erase_if(conversion_context.creature_types, [](const auto& pair) {
				return g_creatures[pair.first] != nullptr;
			});

			if (!conversion_context.creature_types.empty() && options.keep_unrecognized_creatures) {
				for (const auto& [name, info] : conversion_context.creature_types) {
					g_creatures.addCreatureType(info.name, info.is_npc, info.outfit);
				}
			}

			map.cleanInvalidTiles();
		} else {
			ClientVersion* target = ClientVersion::getBestMatch(new_ver.client);
			std::string load_error_2;
			if (!target || !g_version.LoadVersion(target->getID(), load_error_2, result.warnings)) {
				result.error = load_error_2;
				if (result.error.empty() && !target) {
					result.error = "No compatible client version found for this protocol.";
				}
				return result;
			}
			map.convert(new_ver);
		}
	} else {
		map.convert(new_ver);
	}
	result.success = true;
	return result;
}
