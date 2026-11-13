#ifndef RME_ITEM_DEFINITIONS_LOADER_H_
#define RME_ITEM_DEFINITIONS_LOADER_H_

#include "item_definitions/core/item_definition_store.h"
#include "item_definitions/core/missing_item_report.h"

class ItemDefinitionsLoader {
public:
	bool assemble(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, std::string& error, std::vector<std::string>& warnings, MissingItemReport* missingReport = nullptr) const;
	bool load(const ItemDefinitionLoadInput& input, std::string& error, std::vector<std::string>& warnings) const;
};

#endif
