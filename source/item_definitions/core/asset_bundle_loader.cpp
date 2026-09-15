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

bool AssetBundleLoader::load(const AssetLoadRequest& request, AssetBundle& bundle, wxString& error, std::vector<std::string>& warnings) const {
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

			std::string spr_error;
			bundle.sprite_archive = SpriteArchive::loadProtobuf(request.spr_path.GetFullPath().ToStdWstring(), spr_error, warnings);
			if (!bundle.sprite_archive) {
				error = wxString::FromUTF8(spr_error);
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

			std::string spr_error;
			bundle.sprite_archive = SpriteArchive::load(request.spr_path.GetFullPath().ToStdWstring(), bundle.dat_catalog.is_extended, spr_error, warnings);
			if (!bundle.sprite_archive) {
				error = wxString::FromUTF8(spr_error);
				return false;
			}
			break;
		}
		default:
			error = wxString::FromUTF8(std::format("Unsupported item definition mode {}.", static_cast<int>(request.mode)));
			return false;
	}

	ItemDefinitionsLoader definitions_loader;
	if (!definitions_loader.assemble(toDefinitionInput(request, bundle.dat_catalog), bundle.fragments, bundle.rows, error, warnings, &bundle.missing_items)) {
		return false;
	}

	return true;
}

bool AssetBundleLoader::install(AssetBundle& bundle, GraphicManager& graphics, ItemDefinitionStore& store, wxString& error, std::vector<std::string>& warnings) const {
	std::string install_error;
	if (!GraphicsAssembler::install(graphics, bundle.dat_catalog, bundle.sprite_archive, install_error, warnings)) {
		error = wxString::FromUTF8(install_error);
		return false;
	}

	ItemDefinitionStoreBuilder::build(store, bundle.fragments.version, bundle.rows);
	return true;
}
