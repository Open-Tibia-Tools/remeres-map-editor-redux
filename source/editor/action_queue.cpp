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

#include "app/definitions.h"

#include "editor/action_queue.h"
#include "lua/lua_script_manager.h"
#include "editor/action.h"
#include "editor/editor.h"
#include "app/settings.h"
#include "map/map.h"
#include "boost/range/adaptor/reversed.hpp"

#include "game/creature.h"
#include "game/spawn.h"

ActionQueue::ActionQueue(Editor& editor) :
	current(0), memory_size(0), editor(editor) {
	////
}

ActionQueue::~ActionQueue() {
	actions.clear();
}

std::unique_ptr<Action> ActionQueue::createAction(ActionIdentifier ident) {
	return std::unique_ptr<Action>(new Action(editor, ident));
}

std::unique_ptr<Action> ActionQueue::createAction(BatchAction* batch) {
	return std::unique_ptr<Action>(new Action(editor, batch->getType()));
}

std::unique_ptr<BatchAction> ActionQueue::createBatch(ActionIdentifier ident) {
	return std::unique_ptr<BatchAction>(new BatchAction(editor, ident));
}

std::string ActionQueue::getActionName(size_t index) const {
	if (index >= actions.size()) {
		return "Unknown";
	}
	const BatchAction* batch = actions[index].get();
	if (batch && !batch->getLabel().empty()) {
		return batch->getLabel();
	}
	switch (batch->getType()) {
		case ACTION_MOVE: return "Move";
		case ACTION_REMOTE: return "Remote";
		case ACTION_SELECT: return "Select";
		case ACTION_DELETE_TILES: return "Delete";
		case ACTION_CUT_TILES: return "Cut";
		case ACTION_PASTE_TILES: return "Paste";
		case ACTION_RANDOMIZE: return "Randomize";
		case ACTION_BORDERIZE: return "Borderize";
		case ACTION_DRAW: return "Draw";
		case ACTION_SWITCHDOOR: return "Switch Door";
		case ACTION_ROTATE_ITEM: return "Rotate Item";
		case ACTION_ROTATE: return "Rotate";
		case ACTION_REPLACE_ITEMS: return "Replace Items";
		case ACTION_CHANGE_PROPERTIES: return "Change Properties";
		case ACTION_LUA_SCRIPT: return "Lua Script";
		default: return "Unknown";
	}
}

void ActionQueue::resetTimer() {
	if (!actions.empty()) {
		actions.back()->resetTimer();
	}
}

void ActionQueue::addBatch(std::unique_ptr<BatchAction> batch, int stacking_delay) {
	ASSERT(batch);
	ASSERT(current <= actions.size());

	if (batch->size() == 0) {
		return;
	}

	// Commit any uncommited actions...
	batch->commit();

	// Update title and notify state change if map changed
	if (editor.map.doChange()) {
		editor.notifyStateChange();
	}

	if (batch->getType() == ACTION_REMOTE) {
		return;
	}

	while (current != actions.size()) {
		memory_size -= actions.back()->memsize();
		actions.pop_back();
	}

	while (memory_size > size_t(1024 * 1024 * g_settings.getInteger(Config::UNDO_MEM_SIZE)) && !actions.empty()) {
		memory_size -= actions.front()->memsize();
		actions.pop_front();
		current--;
	}

	if (actions.size() > size_t(g_settings.getInteger(Config::UNDO_SIZE)) && !actions.empty()) {
		memory_size -= actions.front()->memsize();
		actions.pop_front();
		current--;
	}

	do {
		if (!actions.empty()) {
			BatchAction* lastAction = actions.back().get();
			if (lastAction->getType() == batch->getType() && g_settings.getInteger(Config::GROUP_ACTIONS) && time(nullptr) - stacking_delay < lastAction->timestamp) {
				lastAction->merge(batch.get());
				lastAction->timestamp = time(nullptr);
				memory_size -= lastAction->memsize();
				memory_size += lastAction->memsize(true);
				break;
			}
		}
		memory_size += batch->memsize();
		batch->timestamp = time(nullptr);
		actions.push_back(std::move(batch));
		current++;
	} while (false);
	g_luaScripts.emit("actionChange");
}

void ActionQueue::addAction(std::unique_ptr<Action> action, int stacking_delay) {
	std::unique_ptr<BatchAction> batch = createBatch(action->getType());
	batch->addAndCommitAction(std::move(action)); // BatchAction takes ownership
	if (batch->size() == 0) {
		return;
	}

	addBatch(std::move(batch), stacking_delay);
}

void ActionQueue::undo() {
	if (current > 0) {
		current--;
		BatchAction* batch = actions[current].get();
		batch->undo();
		editor.notifyStateChange();
		g_luaScripts.emit("actionChange");
	}
}

void ActionQueue::redo() {
	if (current < actions.size()) {
		BatchAction* batch = actions[current].get();
		batch->redo();
		current++;
		editor.notifyStateChange();
		g_luaScripts.emit("actionChange");
	}
}

void ActionQueue::clear() {
	actions.clear();
	current = 0;
	g_luaScripts.emit("actionChange");
}
