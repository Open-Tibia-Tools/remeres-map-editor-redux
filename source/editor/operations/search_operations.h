#ifndef RME_EDITOR_OPERATIONS_SEARCH_OPERATIONS_H_
#define RME_EDITOR_OPERATIONS_SEARCH_OPERATIONS_H_

#include "map/map.h"
#include "map/tile.h"
#include "brushes/brush.h"
#include "brushes/creature/creature_brush.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/complexitem.h"

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EditorOperations {

	constexpr int SEARCH_UPDATE_INTERVAL = 0x8000;
	using ProgressCallback = std::function<void(uint32_t percent)>;

	struct ItemSearcher {
		ItemSearcher(uint16_t itemId, uint32_t maxCount, uint32_t offset = 0, ProgressCallback on_progress = nullptr) :
			itemId(itemId), maxCount(maxCount), offset(offset), on_progress(std::move(on_progress)) { }

		uint16_t itemId;
		uint32_t maxCount;
		uint32_t offset;
		ProgressCallback on_progress;
		uint32_t totalMatches = 0;
		std::vector<std::pair<Tile*, Item*>> result;

		void operator()(Map& map, Tile* tile, Item* item, long long done) {
			if (on_progress && done % SEARCH_UPDATE_INTERVAL == 0) {
				on_progress(static_cast<uint32_t>(100 * done / map.getTileCount()));
			}

			if (item->getID() == itemId) {
				++totalMatches;
				if (totalMatches <= offset) {
					return;
				}

				if (result.size() < static_cast<size_t>(maxCount)) {
					result.emplace_back(tile, item);
				}
			}
		}
	};

	struct MapSearcher {
		MapSearcher(ProgressCallback on_progress = nullptr) :
			on_progress(std::move(on_progress)),
			search_unique(false),
			search_action(false),
			search_container(false),
			search_writeable(false) { }

		ProgressCallback on_progress;
		bool search_unique;
		bool search_action;
		bool search_container;
		bool search_writeable;
		std::vector<std::pair<Tile*, Item*>> found;

		void operator()(Map& map, Tile* tile, Item* item, long long done) {
			if (on_progress && done >= 0 && done % SEARCH_UPDATE_INTERVAL == 0) {
				on_progress(static_cast<uint32_t>(100 * done / map.getTileCount()));
			}
			Container* container;
			if ((search_unique && item->getUniqueID() > 0) || (search_action && item->getActionID() > 0) || (search_container && ((container = dynamic_cast<Container*>(item)) && container->getItemCount())) || (search_writeable && item->getText().length() > 0)) {
				found.push_back(std::make_pair(tile, item));
			}
		}

		std::string desc(Item* item) {
			std::string label;
			if (search_action) {
				if (item->getActionID() > 0) {
					label += std::format("AID:{} ", item->getActionID());
				}
				if (item->getUniqueID() > 0) {
					label += std::format("UID:{} ", item->getUniqueID());
				}
			} else {
				if (item->getUniqueID() > 0) {
					label += std::format("UID:{} ", item->getUniqueID());
				}
				if (item->getActionID() > 0) {
					label += std::format("AID:{} ", item->getActionID());
				}
			}

			label += item->getName();

			if (dynamic_cast<Container*>(item)) {
				label += " (Container) ";
			}

			if (item->getText().length() > 0) {
				label += std::format(" (Text: {}) ", item->getText());
			}

			return label;
		}

		void sort() {
			if (search_unique && !search_action) {
				std::sort(found.begin(), found.end(), [](const std::pair<Tile*, Item*>& pair1, const std::pair<Tile*, Item*>& pair2) {
					const Item* item1 = pair1.second;
					const Item* item2 = pair2.second;

					uint16_t u1 = item1->getUniqueID();
					uint16_t u2 = item2->getUniqueID();
					if (u1 != u2) {
						return u1 < u2;
					}
					return item1->getActionID() < item2->getActionID();
				});
			} else if (search_action && !search_unique) {
				std::sort(found.begin(), found.end(), [](const std::pair<Tile*, Item*>& pair1, const std::pair<Tile*, Item*>& pair2) {
					const Item* item1 = pair1.second;
					const Item* item2 = pair2.second;

					uint16_t a1 = item1->getActionID();
					uint16_t a2 = item2->getActionID();
					if (a1 != a2) {
						return a1 < a2;
					}
					return item1->getUniqueID() < item2->getUniqueID();
				});
			} else if (search_unique || search_action) {
				std::sort(found.begin(), found.end(), MapSearcher::compare);
			}
		}

		static bool compare(const std::pair<Tile*, Item*>& pair1, const std::pair<Tile*, Item*>& pair2) {
			const Item* item1 = pair1.second;
			const Item* item2 = pair2.second;

			if (item1->getActionID() != 0 || item2->getActionID() != 0) {
				return item1->getActionID() < item2->getActionID();
			} else if (item1->getUniqueID() != 0 || item2->getUniqueID() != 0) {
				return item1->getUniqueID() < item2->getUniqueID();
			}

			return false;
		}
	};

	struct CreatureSearcher {
		CreatureSearcher(const Brush* creatureBrush, uint32_t maxCount, uint32_t offset = 0, ProgressCallback on_progress = nullptr) :
			creatureBrush(creatureBrush), maxCount(maxCount), offset(offset), on_progress(std::move(on_progress)) { }

		const Brush* creatureBrush;
		uint32_t maxCount;
		uint32_t offset;
		ProgressCallback on_progress;
		uint32_t totalMatches = 0;
		std::vector<std::pair<Tile*, Creature*>> result;

		void operator()(Map& map, Tile* tile, long long done) {
			if (on_progress && done % SEARCH_UPDATE_INTERVAL == 0) {
				on_progress(static_cast<uint32_t>(100 * done / map.getTileCount()));
			}

			if (tile->creature != nullptr && static_cast<const Brush*>(tile->creature->getBrush()) == creatureBrush) {
				++totalMatches;
				if (totalMatches <= offset) {
					return;
				}

				if (result.size() < static_cast<size_t>(maxCount)) {
					result.emplace_back(tile, tile->creature.get());
				}
			}
		}
	};
}

#endif
