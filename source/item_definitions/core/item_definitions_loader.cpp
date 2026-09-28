#include "item_definitions/core/item_definitions_loader.h"

#include "item_definitions/core/item_definition_parser.h"
#include "item_definitions/core/item_definition_recipe.h"
#include "item_definitions/core/item_definition_resolver.h"
#include "item_definitions/core/item_definition_store_builder.h"
#include "item_definitions/formats/dat/dat_item_parser.h"
#include "item_definitions/formats/otb/otb_item_parser.h"
#include "item_definitions/formats/protobuf/protobuf_item_parser.h"
#include "item_definitions/formats/xml/xml_item_parser.h"
#include "item_definitions/formats/ron/ron_item_parser.h"

#include <memory>

namespace {
	void seedVersionInfoFromClient(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments) {
		if (input.client_version == nullptr) {
			return;
		}

		fragments.version.major_version = input.client_version->getOtbMajor();
		fragments.version.minor_version = input.client_version->getOtbId();
	}
}

bool ItemDefinitionsLoader::assemble(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport) const {
	const ItemDefinitionRecipe& recipe = ItemDefinitionRecipeRegistry::get(input.mode);
	if (!recipe.runnable) {
		error = "Selected item definition mode is not implemented yet.";
		return false;
	}

	const bool requires_xml = input.mode != ItemDefinitionMode::DatRon;
	if (requires_xml && input.xml_path.GetFullPath().IsEmpty() && input.xml_paths.empty()) {
		error = "items.xml path is missing.";
		return false;
	}

	fragments = {};
	rows.clear();
	seedVersionInfoFromClient(input, fragments);

	DatItemParser dat_parser;
	OtbItemParser otb_parser;
	ProtobufItemParser protobuf_parser;
	XmlItemParser xml_parser;
	RonItemParser ron_parser;

	for (size_t i = 0; i < recipe.source_count; ++i) {
		switch (recipe.sources[i]) {
			case ItemDefinitionSourceKind::Dat:
				if (!dat_parser.parse(input, fragments, error, warnings)) {
					return false;
				}
				break;
			case ItemDefinitionSourceKind::Otb:
				if (!otb_parser.parse(input, fragments, error, warnings)) {
					return false;
				}
				break;
			case ItemDefinitionSourceKind::Xml:
				if (!xml_parser.parse(input, fragments, error, warnings)) {
					return false;
				}
				break;
			case ItemDefinitionSourceKind::Protobuf:
				if (!protobuf_parser.parse(input, fragments, error, warnings)) {
					return false;
				}
				break;
			case ItemDefinitionSourceKind::Srv:
				error = "Selected item definition mode is not implemented yet.";
				return false;
			case ItemDefinitionSourceKind::Ron:
				if (!ron_parser.parse(input, fragments, error, warnings)) {
					return false;
				}
				break;
            default:
				error = "Invalid item definition mode selected.";
				return false;
		}
	}

	if (!ItemDefinitionResolver::resolve(input, fragments, rows, error, warnings, missingReport)) {
		return false;
	}

	return true;
}

bool ItemDefinitionsLoader::load(const ItemDefinitionLoadInput& input, wxString& error, std::vector<std::string>& warnings) const {
	ItemDefinitionFragments fragments;
	std::vector<ResolvedItemDefinitionRow> rows;
	if (!assemble(input, fragments, rows, error, warnings)) {
		return false;
	}
	ItemDefinitionStoreBuilder::build(g_item_definitions, fragments.version, rows);
	return true;
}
