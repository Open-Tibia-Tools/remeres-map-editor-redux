#ifndef RME_ITEM_DEFINITIONS_CORE_ASSET_BUNDLE_H_
#define RME_ITEM_DEFINITIONS_CORE_ASSET_BUNDLE_H_

#include "item_definitions/core/item_definition_store.h"
#include "item_definitions/core/missing_item_report.h"
#include "item_definitions/formats/dat/dat_catalog.h"

#include <filesystem>
#include <memory>
#include <vector>

class ClientVersion;
class SpriteArchive;

struct AssetLoadRequest {
	ItemDefinitionMode mode = ItemDefinitionMode::DatOtb;
	std::filesystem::path dat_path;
	std::filesystem::path spr_path;
	std::filesystem::path otb_path;
	// Legacy single XML path kept for migration; xmlInputPaths() falls back to it when xml_paths is empty.
	std::filesystem::path xml_path;
	// Preferred modular XML inputs. xmlInputPaths() uses these first and xml_path can be removed after migration.
	std::vector<std::filesystem::path> xml_paths;
	ClientVersion* client_version = nullptr;
};

struct AssetBundle {
	DatCatalog dat_catalog;
	std::shared_ptr<SpriteArchive> sprite_archive;
	ItemDefinitionFragments fragments;
	std::vector<ResolvedItemDefinitionRow> rows;
	MissingItemReport missing_items;
};

#endif
