#ifndef RME_ITEM_DEFINITION_RECIPE_H_
#define RME_ITEM_DEFINITION_RECIPE_H_

#include "item_definitions/core/item_definition_types.h"

#include <array>
#include <span>

enum class ItemDefinitionSourceKind : uint8_t {
	Dat,
	Otb,
	Xml,
	Srv,
	Protobuf,
	Ron,
};

struct ItemDefinitionRecipe {
	ItemDefinitionMode mode;
	std::array<ItemDefinitionSourceKind, 3> sources {};
	size_t source_count = 0;
	bool runnable = false;
};

class ItemDefinitionRecipeRegistry {
public:
	static const ItemDefinitionRecipe& get(ItemDefinitionMode mode);
};

#endif
