//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Compatibility header: provides the legacy g_items / ItemType API
// used by source/lua/ code, bridging to the new ItemDefinitionStore.
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_ITEMS_H_
#define RME_GAME_ITEMS_H_

#include "item_definitions/core/item_definition_store.h"

// Legacy ItemType struct — a thin adapter around ItemDefinitionView.
// Fields are computed on construction; the struct is cheap to copy.
struct ItemType {
	ServerItemId id = 0;
	ClientItemId clientID = 0;
	std::string name;
	std::string description;

	bool stackable = false;
	bool moveable = false;
	bool pickupable = false;
	bool hasElevation = false;
	bool isBorder = false;
	bool isWall = false;
	bool isTable = false;
	bool isCarpet = false;

	RAWBrush* raw_brush = nullptr;

	bool isGroundTile() const {
		return group == ITEM_GROUP_GROUND;
	}

	bool isDoor() const {
		return type == ITEM_TYPE_DOOR;
	}

private:
	friend class ItemDatabaseAdapter;
	ItemGroup_t group = ITEM_GROUP_NONE;
	ItemTypes_t type = ITEM_TYPE_NONE;
};

// Adapter that wraps ItemDefinitionStore and exposes the old API.
class ItemDatabaseAdapter {
public:
	explicit ItemDatabaseAdapter(ItemDefinitionStore& store) :
		store_(store) { }

	bool typeExists(int id) const {
		return store_.typeExists(static_cast<ServerItemId>(id));
	}

	int getMaxID() const {
		return static_cast<int>(store_.getMaxID());
	}

	// Returns a temporary ItemType populated from the definition store.
	ItemType getItemType(int id) const {
		ItemType it;
		auto view = store_.get(static_cast<ServerItemId>(id));
		if (!view) {
			return it;
		}
		it.id = static_cast<ServerItemId>(id);
		it.clientID = view.clientId();
		it.name = std::string(view.name());
		it.description = std::string(view.description());
		it.stackable = view.hasFlag(ItemFlag::Stackable);
		it.moveable = view.hasFlag(ItemFlag::Moveable);
		it.pickupable = view.hasFlag(ItemFlag::Pickupable);
		it.hasElevation = view.hasFlag(ItemFlag::HasElevation);
		it.isBorder = view.hasFlag(ItemFlag::IsBorder);
		it.isWall = view.hasFlag(ItemFlag::IsWall);
		it.isTable = view.hasFlag(ItemFlag::IsTable);
		it.isCarpet = view.hasFlag(ItemFlag::IsCarpet);
		it.group = view.group();
		it.type = view.type();
		it.raw_brush = view.editorData().raw_brush;
		return it;
	}

	ItemType operator[](int id) const {
		return getItemType(id);
	}

private:
	ItemDefinitionStore& store_;
};

// Global legacy alias — the lua code uses "g_items" everywhere.
inline ItemDatabaseAdapter g_items { g_item_definitions };

#endif // RME_GAME_ITEMS_H_
