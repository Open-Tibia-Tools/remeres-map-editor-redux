//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_PROPERTIES_SRV_ATTRIBUTE_FIELDS_H_
#define RME_UI_PROPERTIES_SRV_ATTRIBUTE_FIELDS_H_

// Editing of the server item attributes (keyhole and key numbers, quest
// chests, door levels and quests, expiry and uses) in the property dialogs
// and the tile panel.

#include "game/srv_item_attributes.h"

#include <array>

class Item;
class wxFlexGridSizer;
class wxSpinCtrl;
class wxWindow;

namespace rme::srv {

	// The loaded client reads its items from a server objects.srv (DatSrv).
	[[nodiscard]] bool isSrvClient();

	// An attribute is shown when the item has it, or when the loaded client is
	// a DatSrv one and the item is of a kind that takes it (keys, doors, chests).
	[[nodiscard]] bool showsAttribute(const Item& item, ItemAttribute attribute);

	[[nodiscard]] int attributeValue(const Item& item, ItemAttribute attribute);

	// 0 removes the attribute.
	void setAttributeValue(Item& item, ItemAttribute attribute, int value);

} // namespace rme::srv

// One label + spin row per server attribute shown for an item, for the
// two-column grids of the property dialogs.
class SrvAttributeFields {
public:
	void create(wxFlexGridSizer* grid, wxWindow* parent, const Item& item);
	void apply(Item& item) const;

private:
	std::array<wxSpinCtrl*, rme::srv::kItemAttributes.size()> spins_ {};
};

#endif
