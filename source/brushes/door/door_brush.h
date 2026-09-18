//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_DOOR_BRUSH_H_
#define RME_DOOR_BRUSH_H_

#include "brushes/brush.h"

class DoorBrush : public Brush {
public:
	explicit DoorBrush(DoorType doortype);
	~DoorBrush() override;

	static DoorBrush* getDoorBrush(DoorType type);

	static void switchDoor(Item* door);

	bool canDraw(BaseMap* map, const Position& position) const override;
	void draw(BaseMap* map, Tile* tile, void* parameter) override;
	void undraw(BaseMap* map, Tile* tile) override;

	int getLookID() const override;
	std::string getName() const override;
	bool oneSizeFitsAll() const override {
		return true;
	}

	static void setLocked(bool locked) { pref_locked = locked; }
	static bool isLocked() { return pref_locked; }

protected:
	DoorType doortype;

private:
	static inline bool pref_locked = false;
};

#endif
