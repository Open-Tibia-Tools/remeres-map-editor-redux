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

#ifndef RME_EDITOR_ACTION_QUEUE_H
#define RME_EDITOR_ACTION_QUEUE_H

#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include "editor/action.h"

class Editor;
class Action;
class BatchAction;

class ActionQueue {
public:
	ActionQueue(Editor& editor);
	virtual ~ActionQueue();

	using ActionList = std::deque<std::unique_ptr<BatchAction>>;

	void resetTimer();

	virtual std::unique_ptr<Action> createAction(ActionIdentifier ident);
	virtual std::unique_ptr<Action> createAction(BatchAction* parent);
	virtual std::unique_ptr<BatchAction> createBatch(ActionIdentifier ident);

	void addBatch(std::unique_ptr<BatchAction> action, int stacking_delay = 0);
	void addAction(std::unique_ptr<Action> action, int stacking_delay = 0);

	void undo();
	void redo();
	void clear();

	bool canUndo() {
		return current > 0;
	}
	bool canRedo() {
		return current < actions.size();
	}

	size_t getCurrentIndex() const {
		return current;
	}
	size_t getSize() const {
		return actions.size();
	}
	std::string getActionName(size_t index) const;

	// Monotonic counts of batches stored as a new entry and of batches merged into the previous
	// entry. Unlike indexes they survive the oldest entries being dropped, so a caller can tell
	// exactly how many entries its own changes added.
	uint64_t getPushedCount() const {
		return pushed_count;
	}
	uint64_t getMergedCount() const {
		return merged_count;
	}

protected:
	size_t current;
	size_t memory_size;
	uint64_t pushed_count = 0;
	uint64_t merged_count = 0;
	Editor& editor;
	ActionList actions;
};

#endif
