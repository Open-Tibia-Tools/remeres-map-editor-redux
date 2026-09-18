//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"
#include "brushes/door/door_brush.h"

#include "app/settings.h"
#include "brushes/wall/wall_brush.h"
#include "game/item.h"
#include "game/house.h"
#include "game/sprites.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"

#include <ranges>
#include <array>
#include <string_view>
static std::array<DoorBrush*, 16> s_door_brushes = {};

DoorBrush::DoorBrush(DoorType doortype) :
	doortype(doortype) {
	if (static_cast<size_t>(doortype) < s_door_brushes.size()) {
		s_door_brushes[doortype] = this;
	}
}

DoorBrush::~DoorBrush() {
	if (static_cast<size_t>(doortype) < s_door_brushes.size() && s_door_brushes[doortype] == this) {
		s_door_brushes[doortype] = nullptr;
	}
}

DoorBrush* DoorBrush::getDoorBrush(DoorType type) {
	if (static_cast<size_t>(type) < s_door_brushes.size()) {
		return s_door_brushes[type];
	}
	return nullptr;
}

std::string DoorBrush::getName() const {
	static constexpr std::array<std::pair<DoorType, std::string_view>, 8> doorNames = { { { WALL_DOOR_NORMAL, "Normal door brush" },
																						  { WALL_DOOR_LOCKED, "Locked door brush" },
																						  { WALL_DOOR_MAGIC, "Magic door brush" },
																						  { WALL_DOOR_QUEST, "Quest door brush" },
																						  { WALL_DOOR_NORMAL_ALT, "Normal door (alt) brush" },
																						  { WALL_ARCHWAY, "Archway brush" },
																						  { WALL_WINDOW, "Window brush" },
																						  { WALL_HATCH_WINDOW, "Hatch window brush" } } };

	auto it = std::ranges::find_if(doorNames, [this](const auto& p) { return p.first == doortype; });
	if (it != doorNames.end()) {
		return std::string(it->second);
	}
	return "Unknown door brush";
}

int DoorBrush::getLookID() const {
	static constexpr std::array<std::pair<DoorType, int>, 8> doorSprites = { { { WALL_DOOR_NORMAL, EDITOR_SPRITE_DOOR_NORMAL },
																			   { WALL_DOOR_LOCKED, EDITOR_SPRITE_DOOR_LOCKED },
																			   { WALL_DOOR_MAGIC, EDITOR_SPRITE_DOOR_MAGIC },
																			   { WALL_DOOR_QUEST, EDITOR_SPRITE_DOOR_QUEST },
																			   { WALL_DOOR_NORMAL_ALT, EDITOR_SPRITE_DOOR_NORMAL_ALT },
																			   { WALL_ARCHWAY, EDITOR_SPRITE_DOOR_ARCHWAY },
																			   { WALL_WINDOW, EDITOR_SPRITE_WINDOW_NORMAL },
																			   { WALL_HATCH_WINDOW, EDITOR_SPRITE_WINDOW_HATCH } } };

	auto it = std::ranges::find_if(doorSprites, [this](const auto& p) { return p.first == doortype; });
	if (it != doorSprites.end()) {
		return it->second;
	}
	return EDITOR_SPRITE_DOOR_NORMAL;
}

void DoorBrush::switchDoor(Item* item) {
	ASSERT(item);
	ASSERT(item->isBrushDoor());

	WallBrush* wb = item->getWallBrush();
	if (!wb) {
		return;
	}

	bool new_open = !item->isOpen();
	BorderType wall_alignment = item->getWallAlignment();

	DoorType type = wb->getDoorTypeFromID(item->getID());
	if (type == WALL_UNDEFINED) {
		return;
	}

	uint16_t oppositeVariant = 0;
	bool prefLocked = DoorBrush::isLocked();

	const auto& doorItems = wb->items.getDoorItems(wall_alignment);
	for (const auto& dt : doorItems) {
		if (dt.type != type) {
			continue;
		}

		ASSERT(dt.id);
		const auto it = g_item_definitions.get(dt.id);
		ASSERT(it);

		if (it.hasFlag(ItemFlag::IsOpen) == new_open) {
			if (!new_open || dt.locked == prefLocked) {
				item->setID(dt.id);
				return;
			}
			if (oppositeVariant == 0) {
				oppositeVariant = dt.id;
			}
		}
	}

	if (oppositeVariant != 0) {
		item->setID(oppositeVariant);
	}
}

bool DoorBrush::canDraw(BaseMap* map, const Position& position) const {
	Tile* tile = map->getTile(position);
	if (!tile) {
		return false;
	}

	Item* item = tile->getWall();
	if (!item) {
		return false;
	}

	WallBrush* wb = item->getWallBrush();
	if (!wb) {
		return false;
	}

	BorderType wall_alignment = item->getWallAlignment();
	uint16_t discarded_id = 0;
	bool close_match = false;
	bool open = item->isBrushDoor() && item->isOpen();
	bool prefLocked = DoorBrush::isLocked();

	WallBrush* test_brush = wb;
	do {
		const auto& doorItems = test_brush->items.getDoorItems(wall_alignment);
		for (const auto& dt : doorItems) {
			if (dt.type == doortype) {
				ASSERT(dt.id);
				const auto it = g_item_definitions.get(dt.id);
				ASSERT(it);

				if (it.hasFlag(ItemFlag::IsOpen) == open) {
					if (open || dt.locked == prefLocked) {
						return true;
					}
					discarded_id = dt.id;
					close_match = true;
				} else if (!close_match) {
					discarded_id = dt.id;
					close_match = true;
				}
			}
		}
		test_brush = test_brush->redirect_to;
	} while (test_brush != wb && test_brush != nullptr);

	return discarded_id != 0;
}

void DoorBrush::undraw(BaseMap* map, Tile* tile) {
	for (const auto& item : tile->items) {
		if (item->isBrushDoor()) {
			item->getWallBrush()->draw(map, tile, nullptr);
			if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
				TileOperations::wallize(tile, map);
			}
			TileOperations::update(tile);
			return;
		}
	}
}

void DoorBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	for (auto it = tile->items.begin(); it != tile->items.end();) {
		Item* item = it->get();
		if (!item->isWall()) {
			++it;
			continue;
		}

		WallBrush* wb = item->getWallBrush();
		if (!wb) {
			++it;
			continue;
		}

		BorderType wall_alignment = item->getWallAlignment();
		uint16_t discarded_id = 0;
		bool close_match = false;
		bool perfect_match = false;
		bool open = parameter ? *static_cast<bool*>(parameter) : (item->isBrushDoor() && item->isOpen());
		bool prefLocked = DoorBrush::isLocked();

		WallBrush* test_brush = wb;
		do {
			const auto& doorItems = test_brush->items.getDoorItems(wall_alignment);
			for (const auto& dt : doorItems) {
				if (dt.type == doortype) {
					ASSERT(dt.id);
					const auto i_type = g_item_definitions.get(dt.id);
					ASSERT(i_type);

					if (i_type.hasFlag(ItemFlag::IsOpen) == open) {
						if (open || dt.locked == prefLocked) {
							item = TileOperations::transformItem(item, dt.id, tile);
							perfect_match = true;
							break;
						}
						discarded_id = dt.id;
						close_match = true;
					} else if (!close_match) {
						discarded_id = dt.id;
						close_match = true;
					}
				}
			}
			if (perfect_match) {
				break;
			}
			test_brush = test_brush->redirect_to;
		} while (test_brush != wb && test_brush != nullptr);

		if (!perfect_match && discarded_id) {
			item = TileOperations::transformItem(item, discarded_id, tile);
		}

		if (g_settings.getInteger(Config::AUTO_ASSIGN_DOORID) && tile->isHouseTile()) {
			if (auto* mmap = dynamic_cast<Map*>(map)) {
				if (auto* door = dynamic_cast<Door*>(item)) {
					if (House* house = mmap->houses.getHouse(tile->getHouseID())) {
						door->setDoorID(house->getEmptyDoorID());
					}
				}
			}
		}

		// Decorations
		while (true) {
			auto found_it = std::ranges::find_if(tile->items, [item](const auto& ptr) { return ptr.get() == item; });
			if (found_it == tile->items.end()) {
				return;
			}

			auto next_it = ++found_it;
			if (next_it == tile->items.end()) {
				return;
			}

			item = next_it->get();
			if (item->isWall()) {
				if (WallBrush* brush = item->getWallBrush(); brush && brush->is<WallDecorationBrush>()) {
					perfect_match = false;
					close_match = false;
					discarded_id = 0;

					const auto& decDoorItems = brush->items.getDoorItems(wall_alignment);
					for (const auto& dt : decDoorItems) {
						if (dt.type == doortype) {
							ASSERT(dt.id);
							const auto i_type = g_item_definitions.get(dt.id);
							ASSERT(i_type);

							if (i_type.hasFlag(ItemFlag::IsOpen) == open) {
								if (open || dt.locked == prefLocked) {
									item = TileOperations::transformItem(item, dt.id, tile);
									perfect_match = true;
									break;
								}
								discarded_id = dt.id;
								close_match = true;
							} else if (!close_match) {
								discarded_id = dt.id;
								close_match = true;
							}
						}
					}
					if (!perfect_match && discarded_id) {
						item = TileOperations::transformItem(item, discarded_id, tile);
					}
					continue;
				}
			}
			break;
		}
		return;
	}
}
