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

#ifndef RME_TOWN_TEMPLE_BRUSH_H_
#define RME_TOWN_TEMPLE_BRUSH_H_

#include "brushes/brush.h"
#include <functional>

class Town;

class TownTempleBrush : public Brush {
public:
	TownTempleBrush();
	~TownTempleBrush() override;

	bool load(pugi::xml_node node, std::vector<std::string>& warnings) override {
		return true;
	}

	bool canDraw(BaseMap* map, const Position& position) const override;
	void draw(BaseMap* map, Tile* tile, void* parameter) override;
	void undraw(BaseMap* map, Tile* tile) override;

	bool canDrag() const override {
		return false;
	}
	bool canSmear() const override {
		return false;
	}
	bool oneSizeFitsAll() const override {
		return true;
	}

	BrushCursor getCursor() const override {
		return BrushCursor::Crosshair;
	}

	void setTown(Town* town);
	uint32_t getTownID() const;

	void setPlacementCallback(std::function<void(const Position&)> callback) {
		on_placement_callback_ = std::move(callback);
	}
	void setCancelCallback(std::function<void()> callback) {
		on_cancel_callback_ = std::move(callback);
	}
	bool hasPlacementCallback() const {
		return static_cast<bool>(on_placement_callback_);
	}
	void invokePlacementCallback(const Position& pos) {
		if (on_placement_callback_) {
			auto cb = std::move(on_placement_callback_);
			on_placement_callback_ = nullptr;
			on_cancel_callback_ = nullptr;
			cb(pos);
		}
	}
	void invokeCancelCallback() {
		if (on_cancel_callback_) {
			auto cb = std::move(on_cancel_callback_);
			on_placement_callback_ = nullptr;
			on_cancel_callback_ = nullptr;
			cb();
		}
	}
	void clearCallbacks() {
		on_placement_callback_ = nullptr;
		on_cancel_callback_ = nullptr;
	}

	int getLookID() const override {
		return 0;
	}
	std::string getName() const override {
		return "Town Temple Brush";
	}

protected:
	uint32_t draw_town;
	std::function<void(const Position&)> on_placement_callback_;
	std::function<void()> on_cancel_callback_;
};

#endif
