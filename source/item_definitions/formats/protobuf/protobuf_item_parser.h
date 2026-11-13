#ifndef RME_PROTOBUF_ITEM_PARSER_H_
#define RME_PROTOBUF_ITEM_PARSER_H_

#include "item_definitions/core/item_definition_parser.h"
#include "item_definitions/formats/dat/dat_catalog.h"

class ProtobufItemParser : public IItemDefinitionParser {
public:
	bool parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, std::string& error, std::vector<std::string>& warnings) const override;
	bool parseCatalog(const ItemDefinitionLoadInput& input, DatCatalog& catalog, std::string& error, std::vector<std::string>& warnings) const;

	static void appendFragments(const DatCatalog& catalog, ItemDefinitionFragments& fragments);
};

#endif
