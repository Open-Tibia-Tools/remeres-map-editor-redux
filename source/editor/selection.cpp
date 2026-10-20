//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"

#include "editor/selection.h"
#include "editor/selection_thread.h"
#include "map/tile.h"
#include "lua/lua_script_manager.h"
#include "game/creature.h"
#include "game/item.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "ui/gui.h"

#include <ranges>
#include <algorithm>

Selection::Selection(Editor& editor) :
	busy(false),
	deferred(false),
	selectionChanged(false),
	editor(editor),
	session(nullptr),
	subsession(nullptr),
	bounds_dirty(true),
	cached_min(0, 0, 0),
	cached_max(0, 0, 0) {
	////
}
Selection::~Selection() {
	//
}

void Selection::recalculateBounds() const {
	if (!bounds_dirty) {
		return;
	}

	Position minPos(0x10000, 0x10000, 0x10);
	Position maxPos(0, 0, 0);

	if (tiles.empty()) {
		minPos = Position(0, 0, 0);
		maxPos = Position(0, 0, 0);
	} else {
		std::ranges::for_each(tiles, [&](Tile* tile) {
			const Position& pos = tile->getPosition();
			minPos.x = std::min(minPos.x, pos.x);
			minPos.y = std::min(minPos.y, pos.y);
			minPos.z = std::min(minPos.z, pos.z);

			maxPos.x = std::max(maxPos.x, pos.x);
			maxPos.y = std::max(maxPos.y, pos.y);
			maxPos.z = std::max(maxPos.z, pos.z);
		});
	}

	cached_min = minPos;
	cached_max = maxPos;
	bounds_dirty = false;
}

Position Selection::minPosition() const {
	recalculateBounds();
	return cached_min;
}

Position Selection::maxPosition() const {
	recalculateBounds();
	return cached_max;
}

void Selection::add(Tile* tile, Item* item) {
	ASSERT(tile);
	ASSERT(item);

	if (item->isSelected()) {
		return;
	}

	if (subsession) {
		// Make a copy of the tile with the item selected
		item->select();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		item->deselect();

		if (g_settings.getInteger(Config::BORDER_IS_GROUND)) {
			if (item->isBorder()) {
				TileOperations::selectGround(new_tile.get());
			}
		}

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		item->select();
		if (g_settings.getInteger(Config::BORDER_IS_GROUND)) {
			if (item->isBorder()) {
				TileOperations::selectGround(tile);
			}
		}
		TileOperations::updateSelectionState(tile);
		addInternal(tile);
	}
}

void Selection::add(Tile* tile, Spawn* spawn) {
	ASSERT(tile);
	ASSERT(spawn);

	if (spawn->isSelected()) {
		return;
	}

	if (subsession) {
		// Make a copy of the tile with the item selected
		spawn->select();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		spawn->deselect();

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		spawn->select();
		TileOperations::updateSelectionState(tile);
		addInternal(tile);
	}
}

void Selection::add(Tile* tile, Creature* creature) {
	ASSERT(tile);
	ASSERT(creature);

	if (creature->isSelected()) {
		return;
	}

	if (subsession) {
		// Make a copy of the tile with the item selected
		creature->select();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		creature->deselect();

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		creature->select();
		TileOperations::updateSelectionState(tile);
		addInternal(tile);
	}
}

void Selection::add(Tile* tile) {
	ASSERT(tile);

	if (subsession) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		TileOperations::select(new_tile.get());

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		TileOperations::select(tile);
		addInternal(tile);
	}
}

void Selection::remove(Tile* tile, Item* item) {
	ASSERT(tile);
	ASSERT(item);

	if (subsession) {
		bool tmp = item->isSelected();
		item->deselect();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		if (tmp) {
			item->select();
		}
		if (item->isBorder() && g_settings.getInteger(Config::BORDER_IS_GROUND)) {
			TileOperations::deselectGround(new_tile.get());
		}

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		item->deselect();
		if (item->isBorder() && g_settings.getInteger(Config::BORDER_IS_GROUND)) {
			TileOperations::deselectGround(tile);
		} else {
			TileOperations::updateSelectionState(tile);
		}
		if (!tile->isSelected()) {
			removeInternal(tile);
		}
	}
}

void Selection::remove(Tile* tile, Spawn* spawn) {
	ASSERT(tile);
	ASSERT(spawn);

	if (subsession) {
		bool tmp = spawn->isSelected();
		spawn->deselect();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		if (tmp) {
			spawn->select();
		}

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		spawn->deselect();
		TileOperations::updateSelectionState(tile);
		if (!tile->isSelected()) {
			removeInternal(tile);
		}
	}
}

void Selection::remove(Tile* tile, Creature* creature) {
	ASSERT(tile);
	ASSERT(creature);

	if (subsession) {
		bool tmp = creature->isSelected();
		creature->deselect();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		if (tmp) {
			creature->select();
		}

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		creature->deselect();
		TileOperations::updateSelectionState(tile);
		if (!tile->isSelected()) {
			removeInternal(tile);
		}
	}
}

void Selection::remove(Tile* tile) {

	if (subsession) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
		TileOperations::deselect(new_tile.get());

		subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
	} else {
		TileOperations::deselect(tile);
		removeInternal(tile);
	}
}

void Selection::addInternal(Tile* tile) {
	ASSERT(tile);

	if (deferred) {
		pending_adds.push_back(tile);
	} else {
		auto it = std::ranges::lower_bound(tiles, tile, tilePositionLessThan);
		if (it == tiles.end() || *it != tile) {
			tiles.insert(it, tile);
			bounds_dirty = true;
			selectionChanged = true;
		}
	}
}

void Selection::removeInternal(Tile* tile) {
	ASSERT(tile);
	if (deferred) {
		pending_removes.push_back(tile);
	} else {
		auto it = std::ranges::lower_bound(tiles, tile, tilePositionLessThan);
		if (it != tiles.end() && *it == tile) {
			tiles.erase(it);
			bounds_dirty = true;
			selectionChanged = true;
		}
	}
}

void Selection::flush() {
	if (pending_adds.empty() && pending_removes.empty()) {
		return;
	}

	const std::vector<Tile*> previousTiles = tiles;
	bounds_dirty = true;

	if (!pending_removes.empty()) {
		std::ranges::sort(pending_removes, tilePositionLessThan);
		auto [first, last] = std::ranges::unique(pending_removes, [](Tile* a, Tile* b) {
			return a->getPosition() == b->getPosition();
		});
		pending_removes.erase(first, last);

		std::vector<Tile*> result;
		result.reserve(tiles.size());
		std::ranges::set_difference(tiles, pending_removes, std::back_inserter(result), tilePositionLessThan);
		tiles = std::move(result);
	}

	if (!pending_adds.empty()) {
		std::ranges::sort(pending_adds, tilePositionLessThan);
		auto [first, last] = std::ranges::unique(pending_adds, [](Tile* a, Tile* b) {
			return a->getPosition() == b->getPosition();
		});
		pending_adds.erase(first, last);

		std::vector<Tile*> merged;
		merged.reserve(tiles.size() + pending_adds.size());
		std::ranges::set_union(tiles, pending_adds, std::back_inserter(merged), tilePositionLessThan);
		tiles = std::move(merged);
	}

	pending_adds.clear();
	pending_removes.clear();
	selectionChanged = selectionChanged || (tiles != previousTiles);
}

void Selection::clear() {
	if (tiles.empty() && pending_adds.empty() && pending_removes.empty()) {
		return;
	}

	if (session) {
		std::ranges::for_each(tiles, [&](Tile* tile) {
			std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
			TileOperations::deselect(new_tile.get());
			subsession->addChange(std::make_unique<Change>(std::move(new_tile)));
		});
	} else {
		std::ranges::for_each(tiles, [](Tile* tile) {
			TileOperations::deselect(tile);
		});
		if (deferred) {
			std::ranges::for_each(pending_adds, [](Tile* tile) {
				TileOperations::deselect(tile);
			});
			pending_adds.clear();
			pending_removes.clear();
		}
	}
	tiles.clear();
	bounds_dirty = true;
	selectionChanged = true;
}

void Selection::markChanged() {
	bounds_dirty = true;
	selectionChanged = true;
}

void Selection::start(SessionFlags flags) {
	selectionChanged = false;
	if (!(flags & INTERNAL)) {
		if (flags & SUBTHREAD) {
			;
		} else {
			session = std::move(editor.actionQueue->createBatch(ACTION_SELECT));
		}
		subsession = editor.actionQueue->createAction(ACTION_SELECT);
	} else {
		deferred = true;
		pending_adds.clear();
		pending_removes.clear();
	}
	busy = true;
}

void Selection::commit() {
	if (session) {
		ASSERT(subsession);
		// We need to step out of the session before we do the action, else peril awaits us!
		std::unique_ptr<BatchAction> tmp = std::move(session);
		session = nullptr;

		// Do the action
		tmp->addAndCommitAction(std::move(subsession));

		// Create a newd action for subsequent selects
		subsession = editor.actionQueue->createAction(ACTION_SELECT);
		session = std::move(tmp);
	}
}

void Selection::finish(SessionFlags flags) {
	if (!(flags & INTERNAL)) {
		if (flags & SUBTHREAD) {
			subsession = nullptr;
		} else {
			ASSERT(session);
			ASSERT(subsession);
			// We need to exit the session before we do the action, else peril awaits us!
			std::unique_ptr<BatchAction> tmp = std::move(session);
			session = nullptr;

			tmp->addAndCommitAction(std::move(subsession));
			editor.addBatch(std::move(tmp), 2);

			session = nullptr;
			subsession = nullptr;
		}
	} else {
		flush();
		deferred = false;
	}
	busy = false;

	// Notify Lua scripts only if we're on the main thread and the selection actually changed
	if (selectionChanged && !(flags & (INTERNAL | SUBTHREAD))) {
		if (g_luaScripts.isInitialized()) {
			g_luaScripts.emit("selectionChange");
		}
	}
	selectionChanged = false;
}

void Selection::addListener(uintptr_t id, std::function<void(size_t)> callback) {
	removeListener(id);
	listeners.push_back({ id, std::move(callback) });
}

void Selection::removeListener(uintptr_t id) {
	std::erase_if(listeners, [id](const SelectionListener& l) {
		return l.id == id;
	});
}

void Selection::updateSelectionCount() {
	if (onSelectionChange) {
		onSelectionChange(size());
	}
	const size_t currentSize = size();
	for (const auto& listener : listeners) {
		if (listener.callback) {
			listener.callback(currentSize);
		}
	}
}

void Selection::join(std::unique_ptr<SelectionThread> thread) {
	thread->Wait();

	ASSERT(session);
	session->addAction(std::move(thread->result));
	thread->selection.subsession = nullptr;
}
