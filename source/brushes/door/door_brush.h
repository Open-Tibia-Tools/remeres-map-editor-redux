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

	void getRelatedItems(std::vector<uint16_t>& items) override;

	[[nodiscard]] uint16_t getDoorItemId(BaseMap* map, const Position& pos) const;
	[[nodiscard]] uint16_t getFallbackDoorItemId() const;

protected:
	DoorType doortype;
	mutable uint16_t cached_fallback_id = 0;

private:
	static inline bool pref_locked = false;
};

#endif
