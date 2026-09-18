#ifndef RME_EDITOR_OPERATIONS_CLEAN_OPERATIONS_H_
#define RME_EDITOR_OPERATIONS_CLEAN_OPERATIONS_H_

#include "map/map.h"
#include "game/item.h"

#include <functional>
#include <utility>

namespace EditorOperations {

	constexpr int PROGRESS_UPDATE_INTERVAL = 0x8000;
	using ProgressCallback = std::function<void(uint32_t percent)>;

	struct RemoveItemCondition {
		RemoveItemCondition(uint16_t itemId, ProgressCallback on_progress = nullptr) :
			itemId(itemId), on_progress(std::move(on_progress)) { }

		uint16_t itemId;
		ProgressCallback on_progress;

		bool operator()(Map& map, Item* item, int64_t removed, int64_t done) {
			if (on_progress && done % PROGRESS_UPDATE_INTERVAL == 0) {
				on_progress(static_cast<uint32_t>(100 * done / map.getTileCount()));
			}
			return item->getID() == itemId && !item->isComplex();
		}
	};

}

#endif
