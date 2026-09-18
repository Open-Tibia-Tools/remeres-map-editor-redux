//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "brushes/wall/wall_brush.h"
#include "brushes/wall/wall_brush_loader.h"
#include "brushes/wall/wall_border_calculator.h"

#include "ui/gui.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/basemap.h"
#include "map/tile_operations.h"

uint32_t WallBrush::full_border_types[16];
uint32_t WallBrush::half_border_types[16];

WallBrush::WallBrush() :
	redirect_to(nullptr) {
	////
}

WallBrush::~WallBrush() {
	////
}

bool WallBrush::load(pugi::xml_node node, std::vector<std::string>& warnings) {
	return WallBrushLoader::load(this, items, node, warnings);
}

void WallBrush::undraw(BaseMap* map, Tile* tile) {
	TileOperations::cleanWalls(tile, this);
}

void WallBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	ASSERT(tile);
	bool b = (parameter ? *reinterpret_cast<bool*>(parameter) : false);
	if (b) {
		// Find a matching wall item on this tile, and shift the id
		for (const auto& item : tile->items) {
			if (item->isWall()) {
				WallBrush* wb = item->getWallBrush();
				if (wb == this) {
					// Ok, shift alignment
					BorderType alignment = item->getWallAlignment();
					uint16_t id = 0;
					WallBrush* try_brush = this;
					while (true) {
						if (id != 0) {
							break;
						}
						if (try_brush == nullptr) {
							return;
						}

						for (int i = alignment + 1; i != alignment; ++i) {
							if (i == WallBrushItems::WALL_ALIGNMENT_COUNT) {
								i = 0;
							}
							id = try_brush->items.getRandomWallId(i);
							if (id != 0) {
								break;
							}
						}

						try_brush = try_brush->redirect_to;
						if (try_brush == this) {
							break;
						}
					}
					if (id != 0) {
						item->setID(id);
					}
					return;
				}
			}
		}
	}

	TileOperations::cleanWalls(tile, this);

	// Just find a valid item and place it, the bordering algorithm will change it to the proper shape.
	uint16_t id = 0;
	WallBrush* try_brush = this;

	while (true) {
		if (id != 0) {
			break;
		}
		if (try_brush == nullptr) {
			return;
		}

		for (int i = 0; i < WallBrushItems::WALL_ALIGNMENT_COUNT; ++i) {
			id = try_brush->items.getRandomWallId(i);
			if (id != 0) {
				break;
			}
		}

		try_brush = try_brush->redirect_to;
		if (try_brush == this) {
			break;
		}
	}

	TileOperations::addWallItem(tile, Item::Create(id));
}

void WallBrush::doWalls(BaseMap* map, Tile* tile) {
	WallBorderCalculator::doWalls(map, tile);
}

void WallBrush::getRelatedItems(std::vector<uint16_t>& items_out) {
	for (int i = 0; i < WallBrushItems::WALL_ALIGNMENT_COUNT; ++i) {
		const auto& node = items.getWallNode(i);
		for (const auto& item : node.items) {
			if (item.id != 0) {
				items_out.push_back(item.id);
			}
		}
		const auto& doors = items.getDoorItems(i);
		for (const auto& door : doors) {
			if (door.id != 0) {
				items_out.push_back(door.id);
			}
		}
	}
}

bool WallBrush::hasWall(Item* item) {
	ASSERT(item->isWall());
	::BorderType bt = item->getWallAlignment();

	WallBrush* test_wall = this;

	while (test_wall != nullptr) {
		if (test_wall->items.hasWall(item->getID(), bt)) {
			return true;
		}

		test_wall = test_wall->redirect_to;
		if (test_wall == this) {
			return false; // Prevent infinite loop
		}
	}
	return false;
}

::DoorType WallBrush::getDoorTypeFromID(uint16_t id) {
	return items.getDoorTypeFromID(id);
}

//=============================================================================
// Wall Decoration brush

WallDecorationBrush::WallDecorationBrush() {
	////
}

WallDecorationBrush::~WallDecorationBrush() {
	////
}

void WallDecorationBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	ASSERT(tile);

	auto iter = tile->items.begin();

	bool prefLocked = g_gui.HasDoorLocked();

	TileOperations::cleanWalls(tile, this);
	while (iter != tile->items.end()) {
		Item* item = iter->get();
		if (item->isBorder()) {
			++iter;
			continue;
		}

		if (item->isWall()) {
			// Now we found something interesting.

			// Is it just a decoration, like what we're trying to add?
			WallBrush* brush = item->getWallBrush();
			if (brush && brush->is<WallDecorationBrush>()) {
				// It is, discard and advance!
				++iter;
				continue;
			}

			// We first need to figure out the alignment of this item (wall)
			BorderType wall_alignment = item->getWallAlignment();

			// Now we need to figure out if we got an item that mights suffice to place on this tile..

			int id = 0;
			if (item->isBrushDoor()) {
				// If it's a door
				::DoorType doortype = brush->getDoorTypeFromID(item->getID());
				uint16_t discarded_id = 0;
				bool close_match = false;
				bool open = item->isOpen();

				const auto& doorItems = items.getDoorItems(wall_alignment);
				for (const auto& dt : doorItems) {
					if (dt.type != doortype) {
						continue;
					}

					ASSERT(dt.id);
					const auto it = g_item_definitions.get(dt.id);
					ASSERT(it);

					if (it.hasFlag(ItemFlag::IsOpen) == open) {
						if (open || dt.locked == prefLocked) {
							id = dt.id;
							break;
						}
						if (!close_match) {
							discarded_id = dt.id;
							close_match = true;
						}
					} else if (!close_match) {
						discarded_id = dt.id;
						close_match = true;
					}
				}
				if (id == 0) {
					id = discarded_id;
					if (id == 0) {
						++iter;
						continue;
					}
				}
			} else {
				// If it's a normal wall...
				id = items.getRandomWallId(wall_alignment);
				if (id == 0) {
					// No fitting item, exit
					++iter;
					continue;
				}
			}
			// If we found an invalid id we should've already exited the loop
			ASSERT(id);

			// Add a matching item above this item.
			std::unique_ptr<Item> deco_item = Item::Create(id);
			++iter;
			iter = tile->items.insert(iter, std::move(deco_item));
		}
		++iter;
	}
}
