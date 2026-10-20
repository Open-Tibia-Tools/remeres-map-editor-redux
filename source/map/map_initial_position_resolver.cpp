#include "map/map_initial_position_resolver.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/town.h"
#include "app/settings.h"
#include <wx/filename.h>
#include <sstream>

Position MapInitialPositionResolver::Resolve(Map& map, std::string_view mapPath) {
	// ------------------------------------------------------------------------
	// Tier 1: Last remembered position in application settings
	// ------------------------------------------------------------------------
	if (!mapPath.empty()) {
		const std::string saved_path = g_settings.getString(Config::RECENT_EDITED_MAP_PATH);
		if (!saved_path.empty()) {
			wxFileName saved_fn(saved_path);
			wxFileName current_fn(wxString::FromUTF8(mapPath.data(), mapPath.size()));
			if (saved_fn == current_fn) {
				const std::string pos_str = g_settings.getString(Config::RECENT_EDITED_MAP_POSITION);
				if (!pos_str.empty()) {
					std::istringstream stream(pos_str);
					Position saved_pos;
					if ((stream >> saved_pos) && saved_pos.isValid() && saved_pos != Position()) {
						return saved_pos;
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------
	// Tier 2: First town temple position defined on the map
	// ------------------------------------------------------------------------
	for (const auto& [_, town] : map.towns) {
		if (town) {
			const Position& temple_pos = town->getTemplePosition();
			if (temple_pos.isValid() && temple_pos != Position()) {
				return temple_pos;
			}
		}
	}

	// ------------------------------------------------------------------------
	// Tier 3 & 4: Single-pass scan over allocated map tiles
	//   3A. First non-empty ground on floor 7 (immediate return).
	//   3B. First non-empty ground on any floor (fallback if no floor 7 ground).
	//   4.  First tile with any item (fallback if no ground exists).
	// ------------------------------------------------------------------------
	Position ground_any_floor;
	Position first_item_any;

	for (const auto& loc : map.tiles()) {
		const Tile* tile = loc.get();
		if (!tile) {
			continue;
		}

		const Position& pos = loc.getPosition();
		if (!pos.isValid() || pos == Position()) {
			continue;
		}

		if (tile->hasGround()) {
			// Tier 3A: Ground on Floor 7 -> highest priority, early return!
			if (pos.z == 7) {
				return pos;
			}
			// Tier 3B candidate
			if (!ground_any_floor.isValid()) {
				ground_any_floor = pos;
			}
		} else if (!tile->items.empty() && !first_item_any.isValid()) {
			// Tier 4 candidate
			first_item_any = pos;
		}
	}

	// Tier 3B: Ground on any other floor
	if (ground_any_floor.isValid()) {
		return ground_any_floor;
	}

	// Tier 4: First tile containing any item
	if (first_item_any.isValid()) {
		return first_item_any;
	}

	// ------------------------------------------------------------------------
	// Fallback: Geometric center of map boundaries on floor 7
	// ------------------------------------------------------------------------
	int cx = map.getWidth() > 0 ? (map.getWidth() / 2) : 1000;
	int cy = map.getHeight() > 0 ? (map.getHeight() / 2) : 1000;
	return Position(cx, cy, 7);
}
