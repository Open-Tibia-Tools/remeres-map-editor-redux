//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_WALL_BRUSH_H
#define RME_WALL_BRUSH_H

#include "brushes/brush.h"
#include "brushes/wall/wall_brush_items.h"
#include "item_definitions/core/item_definition_types.h"

#include <optional>

//=============================================================================
// Wallbrush, for drawing walls

class Tile;

class WallBrush : public TerrainBrush {
public:
	static void init();

	WallBrush();
	~WallBrush() override;


	bool load(pugi::xml_node node, std::vector<std::string>& warnings) override;

	bool canDraw(BaseMap* map, const Position& position) const override {
		return true;
	}

	// Draw to the target tile
	// Note that this actually only puts the first WALL_NORMAL item on the tile.
	// It's up to the doWalls function to change it to the correct alignment
	void draw(BaseMap* map, Tile* tile, void* parameter) override;
	void undraw(BaseMap* map, Tile* tile) override;
	// Creates walls on the target tile (does not depend on brush in any way)
	static void doWalls(BaseMap* map, Tile* tile);

	// If the specified wall item is part of this wall
	bool hasWall(Item* item);
	::DoorType getDoorTypeFromID(uint16_t id);
	std::optional<BorderType> findAlignmentForItem(ServerItemId item_id) const;
	std::optional<ServerItemId> findNextWallVariant(ServerItemId item_id) const;

	void getRelatedItems(std::vector<uint16_t>& items) override;

	[[nodiscard]] uint16_t getPlacementItemId(const Tile* tile = nullptr) const;

	bool canSmear() const override {
		return false;
	}
	bool canDrag() const override {
		return true;
	}

	static uint32_t full_border_types[16];
	static uint32_t half_border_types[16];

	WallBrushItems items;

protected:
	WallBrush* redirect_to;

	friend class DoorBrush;
	friend class WallBrushLoader;
	friend class WallBorderCalculator;
};

//=============================================================================
// Wall decoration brush, for drawing decoration on walls

class WallDecorationBrush : public WallBrush {
public:
	WallDecorationBrush();
	~WallDecorationBrush() override;


	// We use the exact same loading algorithm as normal walls

	void draw(BaseMap* map, Tile* tile, void* parameter) override;
};

#endif
