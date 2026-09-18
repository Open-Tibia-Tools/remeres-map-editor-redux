#ifndef RME_OTB_ITEM_PARSER_H_
#define RME_OTB_ITEM_PARSER_H_

#include "item_definitions/core/item_definition_parser.h"

class OtbItemParser : public IItemDefinitionParser {
public:
	bool parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, std::string& error, std::vector<std::string>& warnings) const override;
};

#endif
