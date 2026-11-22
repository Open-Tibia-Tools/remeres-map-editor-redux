#ifndef RME_ITEM_DEFINITION_PARSER_H_
#define RME_ITEM_DEFINITION_PARSER_H_

#include "item_definitions/core/item_definition_fragments.h"

class IItemDefinitionParser {
public:
	virtual ~IItemDefinitionParser() = default;
	virtual bool parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, std::string& error, std::vector<std::string>& warnings) const = 0;
};

#endif
