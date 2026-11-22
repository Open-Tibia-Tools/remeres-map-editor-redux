#include "app/main.h"

#include "ui/tile_item_actions.h"

#include <memory>

#include "editor/editor.h"
#include "editor/action_queue.h"
#include "game/item.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "map/tile_stack_rows.h"

bool TileItemActions::DeleteRow(Editor& editor, Tile* tile, int row) {
	if (!tile) {
		return false;
	}

	const auto resolved = TileStackRows::Resolve(row, tile->ground != nullptr, tile->items.size());
	if (!resolved) {
		return false;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
	Item* target = resolved->is_ground ? new_tile->ground.get() : new_tile->items[resolved->item_index].get();
	if (!TileOperations::removeItem(new_tile.get(), target)) {
		return false;
	}

	// Keep one row selected so the panel follows the edited tile after the change is applied.
	const bool new_has_ground = new_tile->ground != nullptr;
	const std::size_t remaining_rows = TileStackRows::RowCount(new_has_ground, new_tile->items.size());
	if (const auto fallback_row = TileStackRows::FallbackRowAfterDelete(row, remaining_rows)) {
		const auto fallback = TileStackRows::Resolve(*fallback_row, new_has_ground, new_tile->items.size());
		Item* fallback_item = fallback->is_ground ? new_tile->ground.get() : new_tile->items[fallback->item_index].get();
		fallback_item->select();
	}

	std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
	action->addChange(std::make_unique<Change>(std::move(new_tile)));
	editor.addAction(std::move(action));
	return true;
}
