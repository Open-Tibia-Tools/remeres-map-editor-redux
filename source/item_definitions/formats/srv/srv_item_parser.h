#ifndef RME_SRV_ITEM_PARSER_H_
#define RME_SRV_ITEM_PARSER_H_

#include "item_definitions/core/item_definition_parser.h"

// Reads item definitions from a server objects.srv. Server IDs are the
// server type IDs; the client ID is the same, or the DisguiseTarget for
// disguised types, matching what the game server sends to the client.
// The rows go into the server item table (`fragments.otb`) so the DAT + OTB
// resolver can pair them with Tibia.dat.
class SrvItemParser : public IItemDefinitionParser {
public:
	bool parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const override;
};

#endif
