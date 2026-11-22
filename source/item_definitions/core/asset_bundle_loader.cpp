#include "item_definitions/core/asset_bundle_loader.h"

#include "item_definitions/core/item_definitions_loader.h"
#include "item_definitions/core/item_definition_store_builder.h"
#include "item_definitions/formats/dat/dat_item_parser.h"
#include "item_definitions/formats/protobuf/protobuf_item_parser.h"
#include "rendering/core/graphics_assembler.h"
#include "rendering/core/sprite_archive.h"
#include "item_definition_types.h"

namespace {
	ItemDefinitionLoadInput toDefinitionInput(const AssetLoadRequest& request, const DatCatalog& dat_catalog) {
		return ItemDefinitionLoadInput {
			.mode = request.mode,
			.dat_path = request.dat_path,
			.otb_path = request.otb_path,
			.xml_path = request.xml_path,
			.xml_paths = request.xml_paths,
			.client_version = request.client_version,
			.graphics = nullptr,
			.dat_catalog = &dat_catalog,
		};
	}
}

bool AssetBundleLoader::load(const AssetLoadRequest& request, AssetBundle& bundle, std::string& error, std::vector<std::string>& warnings) const {
	bundle = {};

	DatItemParser dat_parser;
	const auto definition_input = ItemDefinitionLoadInput {
		.mode = request.mode,
		.dat_path = request.dat_path,
		.otb_path = request.otb_path,
		.xml_path = request.xml_path,
		.xml_paths = request.xml_paths,
		.client_version = request.client_version,
		.graphics = nullptr,
		.dat_catalog = nullptr,
	};

	switch (request.mode) {
		case ItemDefinitionMode::ProtobufOtb:
		case ItemDefinitionMode::ProtobufOnly: {
			ProtobufItemParser protobuf_parser;
			if (!protobuf_parser.parseCatalog(definition_input, bundle.dat_catalog, error, warnings)) {
				return false;
			}

			bundle.sprite_archive = SpriteArchive::loadProtobuf(request.spr_path, error, warnings);
			if (!bundle.sprite_archive) {
				return false;
			}
			break;
		}
		case ItemDefinitionMode::DatOtb:
		case ItemDefinitionMode::DatOnly:
		case ItemDefinitionMode::DatSrv: {
			DatItemParser dat_parser;
			if (!dat_parser.parseCatalog(definition_input, bundle.dat_catalog, error, warnings)) {
				return false;
			}

			bundle.sprite_archive = SpriteArchive::load(request.spr_path, bundle.dat_catalog.is_extended, error, warnings);
			if (!bundle.sprite_archive) {
				return false;
			}
			break;
		}
		default:
			error = std::format("Unsupported item definition mode {}.", static_cast<int>(request.mode));
			return false;
	}

	ItemDefinitionsLoader definitions_loader;
	if (!definitions_loader.assemble(toDefinitionInput(request, bundle.dat_catalog), bundle.fragments, bundle.rows, error, warnings, &bundle.missing_items)) {
		return false;
	}

	return true;
}

bool AssetBundleLoader::install(AssetBundle& bundle, GraphicManager& graphics, ItemDefinitionStore& store, std::string& error, std::vector<std::string>& warnings) const {
	if (!GraphicsAssembler::install(graphics, bundle.dat_catalog, bundle.sprite_archive, error, warnings)) {
		return false;
	}

	ItemDefinitionStoreBuilder::build(store, bundle.fragments.version, bundle.rows);
	return true;
}
