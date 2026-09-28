#ifndef RME_ITEM_DEFINITIONS_CORE_ASSET_BUNDLE_H_
#define RME_ITEM_DEFINITIONS_CORE_ASSET_BUNDLE_H_

#include "item_definitions/core/item_definition_store.h"
#include "item_definitions/core/missing_item_report.h"
#include "item_definitions/formats/dat/dat_catalog.h"

#include <memory>
#include <vector>
#include <wx/filename.h>

class ClientVersion;
class SpriteArchive;

struct AssetLoadRequest {
	ItemDefinitionMode mode = ItemDefinitionMode::DatOtb;
	wxFileName dat_path;
	wxFileName spr_path;
	wxFileName otb_path;
	// Legacy single XML path kept for migration; xmlInputPaths() falls back to it when xml_paths is empty.
	wxFileName xml_path;
	// Preferred modular XML inputs. xmlInputPaths() uses these first and xml_path can be removed after migration.
	std::vector<wxFileName> xml_paths;
	ClientVersion* client_version = nullptr;
	wxFileName ron_path;
};

struct AssetBundle {
	DatCatalog dat_catalog;
	std::shared_ptr<SpriteArchive> sprite_archive;
	ItemDefinitionFragments fragments;
	std::vector<ResolvedItemDefinitionRow> rows;
	MissingItemReport missing_items;
};

#endif
