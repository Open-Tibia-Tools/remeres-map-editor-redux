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

#ifndef RME_EDITOR_H
#define RME_EDITOR_H

#include "game/item.h"
#include "map/tile.h"
#include "io/iomap.h"
#include "map/map.h"

#include "editor/action.h"
#include "editor/selection.h"
#include "editor/operations/selection_operations.h"
#include "map/operations/map_processor.h"
#include "editor/persistence/editor_persistence.h"

#include <memory>

class BaseMap;
class CopyBuffer;
class LiveClient;
class LiveServer;
class LiveSocket;

#include "live/live_manager.h"

#include <functional>

class Editor {
public:
	Editor(CopyBuffer& copybuffer, const MapVersion& version, std::unique_ptr<LiveClient> client);
	Editor(CopyBuffer& copybuffer, const MapVersion& version, const std::filesystem::path& fn, const MapLoadOptions& load_options = {});
	Editor(CopyBuffer& copybuffer, const MapVersion& version);
	~Editor();

	// Live Manager
	LiveManager live_manager;

public:
	// Public members
	std::unique_ptr<ActionQueue> actionQueue;
	Selection selection;
	CopyBuffer& copybuffer;
	GroundBrush* replace_brush;
	Map map; // The map that is being edited
	Map* getMap() { return &map; }

	std::function<void()> onStateChange;
	void notifyStateChange();

	std::function<void(const Position&)> onTileModified;
	void markTileModified(const Position& pos);

public: // Functions
	// Map handling

	// Adds an action to the action queue (this allows the user to undo the action)
	// Invalidates the action pointer
	void addBatch(std::unique_ptr<BatchAction> action, int stacking_delay = 0);
	void addAction(std::unique_ptr<Action> action, int stacking_delay = 0);

	// Selection
	bool hasSelection() const {
		return selection.size() != 0;
	}
	// Some simple actions that work on the map (these will work through the undo queue)
	// Moves the selected area by the offset
	void moveSelection(Position offset);
	// Deletes all selected items
	DestroySelectionResult destroySelection();
	// Borderizes the selected region
	bool borderizeSelection();
	// Randomizes the ground in the selected region
	bool randomizeSelection();

	// Same as above although it applies to the entire map
	// action queue is flushed when these functions are called
	void borderizeMap(std::function<void(int)> on_progress = nullptr);
	void randomizeMap(std::function<void(int)> on_progress = nullptr);
	void clearInvalidHouseTiles(std::function<void(int)> on_progress = nullptr);
	void clearModifiedTileState(std::function<void(int)> on_progress = nullptr);

	// Draw using the specified brush to the target position
	// alt is whether the ALT key is pressed
	void draw(Brush* brush, const Position& offset, bool alt, int brush_size = 1);
	void undraw(Brush* brush, const Position& offset, bool alt, int brush_size = 1);
	void draw(Brush* brush, const PositionVector& posvec, bool alt, int brush_size = 1);
	void draw(Brush* brush, const PositionVector& todraw, PositionVector& toborder, bool alt, int brush_size = 1);
	void undraw(Brush* brush, const PositionVector& posvec, bool alt, int brush_size = 1);
	void undraw(Brush* brush, const PositionVector& todraw, PositionVector& toborder, bool alt, int brush_size = 1);

protected:
	void drawInternal(Brush* brush, const Position offset, bool alt, bool dodraw, int brush_size);
	void drawInternal(Brush* brush, const PositionVector& posvec, bool alt, bool dodraw, int brush_size);
	void drawInternal(Brush* brush, const PositionVector& todraw, PositionVector& toborder, bool alt, bool dodraw, int brush_size);

	Editor(const Editor&);
	Editor& operator=(const Editor&);
};

inline void Editor::draw(Brush* brush, const Position& offset, bool alt, int brush_size) {
	drawInternal(brush, offset, alt, true, brush_size);
}
inline void Editor::undraw(Brush* brush, const Position& offset, bool alt, int brush_size) {
	drawInternal(brush, offset, alt, false, brush_size);
}
inline void Editor::draw(Brush* brush, const PositionVector& posvec, bool alt, int brush_size) {
	drawInternal(brush, posvec, alt, true, brush_size);
}
inline void Editor::draw(Brush* brush, const PositionVector& todraw, PositionVector& toborder, bool alt, int brush_size) {
	drawInternal(brush, todraw, toborder, alt, true, brush_size);
}
inline void Editor::undraw(Brush* brush, const PositionVector& posvec, bool alt, int brush_size) {
	drawInternal(brush, posvec, alt, false, brush_size);
}
inline void Editor::undraw(Brush* brush, const PositionVector& todraw, PositionVector& toborder, bool alt, int brush_size) {
	drawInternal(brush, todraw, toborder, alt, false, brush_size);
}

#endif
