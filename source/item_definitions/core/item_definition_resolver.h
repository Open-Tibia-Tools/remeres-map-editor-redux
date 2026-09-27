#ifndef RME_ITEM_DEFINITION_RESOLVER_H_
#define RME_ITEM_DEFINITION_RESOLVER_H_

#include "item_definitions/core/item_definition_fragments.h"
#include "item_definitions/core/missing_item_report.h"

struct DatCatalog;

class ItemDefinitionResolver {
public:
	static bool resolve(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport = nullptr);

private:
	static bool resolveDatOtb(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport);
	static bool resolveDatRon(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport);
	static bool resolveDatOnly(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport);
	static void applyXmlOverrides(const XmlItemFragment& xml, ResolvedItemDefinitionRow& row);
};

#endif
