#ifndef RME_RON_ITEM_PARSER_H_
#define RME_RON_ITEM_PARSER_H_

#include "item_definitions/core/item_definition_parser.h"

class RonItemParser : public IItemDefinitionParser {
public:
	bool parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const override;
};

#endif
