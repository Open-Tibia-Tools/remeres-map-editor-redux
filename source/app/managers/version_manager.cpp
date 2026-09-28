//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "app/managers/version_manager.h"

#include <spdlog/spdlog.h>

#include "ui/gui.h"
#include "util/file_system.h"
#include "game/sprites.h"
#include "game/materials.h"
#include "game/material_manifest.h"
#include "brushes/brush.h"
#include "brushes/managers/brush_manager.h"
#include "ui/managers/loading_manager.h"
#include "ui/tool_options_window.h"
#include "item_definitions/core/asset_bundle_loader.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/material_server_ids.h"
#include "app/settings.h"

#include <format>
#include <ranges>

VersionManager g_version;

namespace {

constexpr size_t MAX_MISSING_ITEMS_TO_SHOW = 50;

void appendServerClientMissingItems(std::vector<std::string>& warnings, std::string_view title, const std::vector<MissingItemEntry>& entries) {
	warnings.push_back(std::format("--- {} ({}) ---", title, entries.size()));
	for (const auto& entry : entries | std::views::take(MAX_MISSING_ITEMS_TO_SHOW)) {
		warnings.push_back(std::format("  Server ID: {}, Client ID: {}, Name: '{}'",
			entry.server_id, entry.client_id, entry.name.empty() ? "unknown" : entry.name));
	}
	if (entries.size() > MAX_MISSING_ITEMS_TO_SHOW) {
		warnings.push_back(std::format("  ...and {} more", entries.size() - MAX_MISSING_ITEMS_TO_SHOW));
	}
	warnings.push_back("");
}

void appendClientMissingItems(std::vector<std::string>& warnings, std::string_view title, const std::vector<MissingItemEntry>& entries) {
	warnings.push_back(std::format("--- {} ({}) ---", title, entries.size()));
	for (const auto& entry : entries | std::views::take(MAX_MISSING_ITEMS_TO_SHOW)) {
		warnings.push_back(std::format("  Client ID: {}", entry.client_id));
	}
	if (entries.size() > MAX_MISSING_ITEMS_TO_SHOW) {
		warnings.push_back(std::format("  ...and {} more", entries.size() - MAX_MISSING_ITEMS_TO_SHOW));
	}
	warnings.push_back("");
}

} // namespace

VersionManager::VersionManager() :
	loaded_version(CLIENT_VERSION_NONE) {
}

VersionManager::~VersionManager() {
	// UnloadVersion is handled explicitly by Application::Unload() during graceful shutdown.
	// Calling it here during static deinitialization causes use-after-free for singletons
	// like SpritePreloader that may have already been destructed.
}

bool VersionManager::LoadVersion(ClientVersionID version, wxString& error, std::vector<std::string>& warnings, bool force) {
	if (ClientVersion::get(version) == nullptr) {
		error = "Unsupported client version! (8)";
		return false;
	}

	if (version != loaded_version || force) {
		if (getLoadedVersion() != nullptr) {
			// There is another version loaded right now, save window layout
			g_gui.SavePerspective();
		}

		// Disable all rendering so the data is not accessed while reloading
		UnnamedRenderingLock();
		g_gui.DestroyPalettes();
		g_gui.DestroyMinimap();

		// Destroy the previous version
		UnloadVersion();

		loaded_version = version;
		if (!getLoadedVersion()->hasValidPaths()) {
			if (!getLoadedVersion()->loadValidPaths()) {
				error = "Couldn't load relevant asset files";
				loaded_version = CLIENT_VERSION_NONE;
				return false;
			}
		}

		bool ret = LoadDataFiles(error, warnings);
		if (ret) {
			g_gui.LoadPerspective();
		} else {
			loaded_version = CLIENT_VERSION_NONE;
		}

		return ret;
	}
	return true;
}

bool VersionManager::setStartupCatalog(ClientVersion* client, bool use_items_ron, wxString& error, std::vector<std::string>& warnings) {
	if (client == nullptr) {
		return true;
	}

	const ItemDefinitionMode configured = client->getItemDefinitionMode();
	if (configured != ItemDefinitionMode::DatOtb && configured != ItemDefinitionMode::DatRon) {
		return true;
	}

	const ItemDefinitionMode wanted = use_items_ron ? ItemDefinitionMode::DatRon : ItemDefinitionMode::DatOtb;
	const bool override_applies = catalog_override.has_value() && catalog_override_version == client->getID();
	const ItemDefinitionMode previous = override_applies ? *catalog_override : configured;
	const auto saved_override = catalog_override;
	const auto saved_override_version = catalog_override_version;
	catalog_override = wanted;
	catalog_override_version = client->getID();
	if (getLoadedVersion() != client || previous == wanted) {
		return true;
	}

	if (g_gui.tabbook && !g_gui.CloseAllEditors()) {
		catalog_override = saved_override;
		catalog_override_version = saved_override_version;
		error = "Changing the item catalog requires closing all open maps.";
		return false;
	}

	return LoadVersion(client->getID(), error, warnings, true);
}

ItemDefinitionMode VersionManager::activeItemDefinitionMode() const {
	const ClientVersion* loaded = getLoadedVersion();
	if (loaded == nullptr) {
		return ItemDefinitionMode::DatOtb;
	}
	if (catalog_override.has_value() && catalog_override_version == loaded->getID()) {
		return *catalog_override;
	}
	return loaded->getItemDefinitionMode();
}

ClientVersionID VersionManager::GetCurrentVersionID() const {
	if (!loaded_version.empty()) {
		return getLoadedVersion()->getID();
	}
	return CLIENT_VERSION_NONE;
}

const ClientVersion& VersionManager::GetCurrentVersion() const {
	assert(!loaded_version.empty());
	return *getLoadedVersion();
}

bool VersionManager::LoadDataFiles(wxString& error, std::vector<std::string>& warnings) {
	g_gui.gfx.client_version = getLoadedVersion();

	// OTFI loading removed. Metadata and sprite files are configured via clients.toml or defaults in ClientVersion.

	g_loading.CreateLoadBar("Loading asset files");
	g_loading.SetLoadDone(0, "Loading canonical asset bundle...");

	const wxString modular_data_path = GetModularDataPath();
	if (!wxFileName(modular_data_path).DirExists()) {
		error = "Missing modular client data directory: " + modular_data_path;
		FailDataLoad();
		return false;
	}

	const FileName materials_manifest(modular_data_path + "materials.xml");
	MaterialManifestFiles manifest_files;
	if (!LoadMaterialManifest(materials_manifest, manifest_files, error)) {
		return false;
	}

	if (!LoadCanonicalAssets(modular_data_path, manifest_files, error, warnings)) {
		return false;
	}

	if (activeItemDefinitionMode() == ItemDefinitionMode::DatRon) {
		if (!loadMaterialServerIds(wxFileName(modular_data_path + "items.otb"), warnings)) {
			error = warnings.empty() ? wxString("items.otb is required to translate brush and tileset ids for items.ron.") : wxString::FromUTF8(warnings.back());
			FailDataLoad();
			return false;
		}
	} else {
		clearMaterialServerIds();
	}

	if (!LoadCreatureFiles(manifest_files, error, warnings)) {
		return false;
	}

	if (!LoadModularMaterials(materials_manifest, error, warnings)) {
		return false;
	}

	g_loading.SetLoadDone(70, "Finishing...");
	g_brushes.init();

	g_loading.DestroyLoadBar();
	return true;
}

wxString VersionManager::GetModularDataPath() const {
	return FileSystem::GetDataDirectory() + wxstr(getLoadedVersion()->getDataDirectory()) + FileName::GetPathSeparator();
}

void VersionManager::FailDataLoad() {
	last_missing_items = {};
	last_load_has_otb = false;
	clearMaterialServerIds();
	g_loading.DestroyLoadBar();
	UnloadVersion();
}

bool VersionManager::LoadMaterialManifest(const FileName& materials_manifest, MaterialManifestFiles& manifest_files, wxString& error) {
	if (LoadMaterialManifestFiles(materials_manifest, manifest_files, error)) {
		return true;
	}
	FailDataLoad();
	return false;
}

bool VersionManager::LoadCanonicalAssets(const wxString& modular_data_path, const MaterialManifestFiles& manifest_files, wxString& error, std::vector<std::string>& warnings) {
	AssetLoadRequest asset_request;
	asset_request.mode = activeItemDefinitionMode();
	asset_request.client_version = getLoadedVersion();
	asset_request.dat_path = getLoadedVersion()->getMetadataPath();
	asset_request.spr_path = getLoadedVersion()->getSpritesPath();
	asset_request.otb_path = wxFileName(modular_data_path + "items.otb");
	asset_request.ron_path = wxFileName(modular_data_path + "items.ron");
	if (asset_request.mode != ItemDefinitionMode::DatRon) {
		asset_request.xml_paths.assign(manifest_files.items.begin(), manifest_files.items.end());
	}

	// Track whether this mode uses OTB as the item catalog
	last_load_has_otb = asset_request.mode == ItemDefinitionMode::DatOtb || asset_request.mode == ItemDefinitionMode::ProtobufOtb;

	AssetBundle bundle;
	AssetBundleLoader bundle_loader;
	if (!bundle_loader.load(asset_request, bundle, error, warnings)) {
		error = "Couldn't load canonical asset bundle: " + error;
		FailDataLoad();
		return false;
	}

	g_loading.SetLoadDone(20, "Installing graphics...");
	if (!bundle_loader.install(bundle, g_gui.gfx, g_item_definitions, error, warnings)) {
		error = "Couldn't install canonical asset bundle: " + error;
		FailDataLoad();
		return false;
	}

	// Store the missing items report for later dialog display (always collected regardless of warning setting)
	last_missing_items = std::move(bundle.missing_items);

	// Only add detailed missing items to warnings list if the user has enabled this option
	if (g_settings.getBoolean(Config::SHOW_MISSING_ITEMS_WARNING)) {
		AppendMissingItemWarnings(warnings);
	}
	return true;
}

void VersionManager::AppendMissingItemWarnings(std::vector<std::string>& warnings) const {
	const size_t total_missing = last_missing_items.missing_in_dat.size() +
	                             last_missing_items.missing_in_otb.size() +
	                             last_missing_items.xml_no_otb.size() +
	                             last_missing_items.otb_no_xml.size();
	if (total_missing == 0) {
		return;
	}

	warnings.push_back(std::format("Missing item definitions detected ({} entries total).", total_missing));
	warnings.push_back("Go to File -> Missing Items Report... to view details.");
	warnings.push_back("");

	if (!last_missing_items.missing_in_dat.empty()) {
		appendServerClientMissingItems(warnings, "Items missing from tibia.dat", last_missing_items.missing_in_dat);
	}
	if (!last_missing_items.missing_in_otb.empty()) {
		const ItemDefinitionMode mode = loaded_version.empty() ? ItemDefinitionMode::DatOtb : activeItemDefinitionMode();
		const std::string missing_in_otb_label = mode == ItemDefinitionMode::DatRon ? "tibia.dat items not in items.ron" : last_load_has_otb ? "tibia.dat items not in items.otb" : "tibia.dat items not referenced by items.xml";
		appendClientMissingItems(warnings, missing_in_otb_label, last_missing_items.missing_in_otb);
	}
	if (!last_missing_items.xml_no_otb.empty()) {
		appendServerClientMissingItems(warnings, "items.xml entries missing from items.otb", last_missing_items.xml_no_otb);
	}
	if (!last_missing_items.otb_no_xml.empty()) {
		appendServerClientMissingItems(warnings, "items.otb entries missing from items.xml", last_missing_items.otb_no_xml);
	}
}

bool VersionManager::LoadCreatureFiles(const MaterialManifestFiles& manifest_files, wxString& error, std::vector<std::string>& warnings) {
	g_loading.SetLoadDone(35, "Loading creatures.xml ...");
	for (const FileName& creatureFile : manifest_files.creatures) {
		if (!g_creatures.loadFromXML(creatureFile, true, error, warnings)) {
			error = "Couldn't load creatures XML file " + creatureFile.GetFullPath() + ": " + error;
			FailDataLoad();
			return false;
		}
	}
	return true;
}

bool VersionManager::LoadModularMaterials(const FileName& materials_manifest, wxString& error, std::vector<std::string>& warnings) {
	g_loading.SetLoadDone(50, "Loading materials.xml ...");
	if (!g_materials.loadMaterials(materials_manifest, error, warnings)) {
		error = "Couldn't load materials.xml: " + error;
		FailDataLoad();
		return false;
	}
	return true;
}

void VersionManager::UnloadVersion() {
	UnnamedRenderingLock();
	g_gui.gfx.clear();
	if (g_gui.tool_options) {
		g_gui.tool_options->Clear();
	}
	g_brush_manager.Clear();
	clearMaterialServerIds();

	if (!loaded_version.empty()) {
		g_materials.clear();
		g_brushes.clear();
		g_item_definitions.clear();
		g_gui.gfx.clear();

		// FileName cdb = getLoadedVersion()->getLocalDataPath();
		// cdb.SetFullName("creatures.xml");
		// g_creatures.saveToXML(cdb); // Disabled to prevent dual source of truth
		g_creatures.clear();

		loaded_version = CLIENT_VERSION_NONE;
	}
}
