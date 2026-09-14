#ifndef RME_EDITOR_OPERATIONS_CLEAN_OPERATIONS_H_
#define RME_EDITOR_OPERATIONS_CLEAN_OPERATIONS_H_

#include "map/map.h"
#include "game/item.h"
#include "ui/gui.h"

namespace EditorOperations {

	constexpr int PROGRESS_UPDATE_INTERVAL = 0x8000;

	struct RemoveItemCondition {
		RemoveItemCondition(uint16_t itemId) :
			itemId(itemId) { }

		uint16_t itemId;

		bool operator()(Map& map, Item* item, int64_t removed, int64_t done) {
			if (done % PROGRESS_UPDATE_INTERVAL == 0) {
				g_gui.SetLoadDone(static_cast<uint32_t>(100 * done / map.getTileCount()));
			}
			return item->getID() == itemId && !item->isComplex();
		}
	};

}

#endif
