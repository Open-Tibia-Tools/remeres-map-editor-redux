//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "app/managers/prepared_version_load.h"
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
#include "ui/brush_items/brush_items_window.h"
#include "item_definitions/core/asset_bundle_loader.h"
#include "item_definitions/core/item_definition_store.h"
#include "app/settings.h"
#include "ui/map_tab.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/creature.h"
#include <set>

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
	ClientVersion* target = ClientVersion::get(version);
	if (target == nullptr) {
		error = "Unsupported client version! (8)";
		return false;
	}

	if (version != loaded_version || force) {
		if (!target->hasValidPaths() && !target->loadValidPaths()) {
			error = "Couldn't load relevant asset files";
			return false;
		}

		PreparedVersionLoad prepared;
		if (!PrepareVersionLoad(version, prepared, error, warnings)) {
			return false;
		}

		return CommitPreparedVersion(prepared, error, warnings);
	}
	return true;
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

wxString VersionManager::GetModularDataPath(const ClientVersion& client_version) const {
	return FileSystem::GetDataDirectory() + wxstr(client_version.getDataDirectory()) + FileName::GetPathSeparator();
}

wxString VersionManager::GetModularDataPath() const {
	if (!getLoadedVersion()) {
		return wxEmptyString;
	}
	return GetModularDataPath(*getLoadedVersion());
}

bool VersionManager::CheckModularClientData(const ClientVersion& client_version, wxString& error) const {
	const wxString modular_data_path = GetModularDataPath(client_version);
	if (modular_data_path.empty() || !wxFileName(modular_data_path).DirExists()) {
		error = "Missing modular client data directory: " + modular_data_path;
		return false;
	}

	const FileName materials_manifest(modular_data_path + "materials.xml");
	if (!materials_manifest.FileExists()) {
		error = "Missing materials.xml in modular client data directory: " + modular_data_path;
		return false;
	}

	const ItemDefinitionMode mode = client_version.getItemDefinitionMode();
	if (mode == ItemDefinitionMode::DatSrv) {
		error = wxstr(std::format("Client {} uses unsupported DatSrv item definition mode.", client_version.getName()));
		return false;
	}

	if (mode == ItemDefinitionMode::DatOtb || mode == ItemDefinitionMode::ProtobufOtb) {
		const FileName otb_file(modular_data_path + "items.otb");
		if (!otb_file.FileExists()) {
			error = "Missing items.otb in modular client data directory: " + modular_data_path;
			return false;
		}
	}

	return true;
}

bool VersionManager::PrepareVersionLoad(
	ClientVersionID version, PreparedVersionLoad& prepared, wxString& error, std::vector<std::string>& warnings
) const {
	ClientVersion* target_version = ClientVersion::get(version);
	if (target_version == nullptr) {
		error = "Unsupported client version! (8)";
		return false;
	}

	if (!CheckModularClientData(*target_version, error)) {
		return false;
	}

	struct ScopedLoadBar {
		ScopedLoadBar() {
			g_loading.CreateLoadBar("Loading asset files");
			g_loading.SetLoadDone(0, "Loading canonical asset bundle...");
		}
		~ScopedLoadBar() {
			g_loading.DestroyLoadBar();
		}
	} scoped_load_bar;

	const wxString modular_data_path = GetModularDataPath(*target_version);
	const FileName materials_manifest(modular_data_path + "materials.xml");

	MaterialManifestFiles manifest_files;
	if (!LoadMaterialManifestFiles(materials_manifest, manifest_files, error)) {
		return false;
	}

	AssetLoadRequest asset_request;
	asset_request.mode = target_version->getItemDefinitionMode();
	asset_request.client_version = target_version;
	asset_request.dat_path = target_version->getMetadataPath();
	asset_request.spr_path = target_version->getSpritesPath();
	asset_request.otb_path = wxFileName(modular_data_path + "items.otb");
	asset_request.xml_paths.assign(manifest_files.items.begin(), manifest_files.items.end());

	AssetBundle bundle;
	AssetBundleLoader bundle_loader;
	if (!bundle_loader.load(asset_request, bundle, error, warnings)) {
		error = "Couldn't load canonical asset bundle: " + error;
		return false;
	}

	prepared.version = version;
	prepared.modular_data_path = modular_data_path;
	prepared.manifest_files = std::move(manifest_files);
	prepared.bundle = std::move(bundle);
	prepared.has_otb = (asset_request.mode != ItemDefinitionMode::DatOnly && asset_request.mode != ItemDefinitionMode::ProtobufOnly);
	return true;
}

bool VersionManager::CommitPreparedVersion(PreparedVersionLoad& prepared, wxString& error, std::vector<std::string>& warnings) {
	if (ClientVersion::get(prepared.version) == nullptr) {
		error = "Unsupported client version! (8)";
		return false;
	}

	if (getLoadedVersion() != nullptr) {
		// Save active window layout before unloading previous client palettes.
		g_gui.SavePerspective();
	}

	UnnamedRenderingLock();
	g_gui.DestroyPalettes();
	g_gui.DestroyMinimap();
	UnloadVersion();

	loaded_version = prepared.version;
	g_gui.gfx.client_version = getLoadedVersion();
	last_load_has_otb = prepared.has_otb;

	g_loading.CreateLoadBar("Loading asset files");
	g_loading.SetLoadDone(20, "Installing graphics...");

	AssetBundleLoader bundle_loader;
	if (!bundle_loader.install(prepared.bundle, g_gui.gfx, g_item_definitions, error, warnings)) {
		error = "Couldn't install canonical asset bundle: " + error;
		FailDataLoad();
		return false;
	}

	last_missing_items = std::move(prepared.bundle.missing_items);
	if (g_settings.getBoolean(Config::SHOW_MISSING_ITEMS_WARNING)) {
		AppendMissingItemWarnings(warnings);
	}

	if (!LoadCreatureFiles(prepared.manifest_files, error, warnings)) {
		return false;
	}

	const FileName materials_manifest(prepared.modular_data_path + "materials.xml");
	if (!LoadModularMaterials(materials_manifest, error, warnings)) {
		return false;
	}

	g_loading.SetLoadDone(70, "Finishing...");
	g_brushes.init();
	g_materials.ensureAllRawBrushes();

	RegisterCreaturesFromOpenMaps();

	g_gui.ReleaseAllChunkCaches();
	g_palettes.ReloadDynamicPalettes();

	g_loading.DestroyLoadBar();
	g_gui.LoadPerspective();
	return true;
}

void VersionManager::FailDataLoad() {
	last_missing_items = {};
	last_load_has_otb = false;
	g_gui.gfx.client_version = nullptr;
	g_loading.DestroyLoadBar();
	UnloadVersion();
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
		const std::string missing_in_otb_label = last_load_has_otb ? "tibia.dat items not in items.otb" : "tibia.dat items not referenced by items.xml";
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

void VersionManager::RegisterCreaturesFromOpenMaps() {
	if (!g_gui.tabbook) {
		return;
	}

	std::set<Map*> visited_maps;
	for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
		auto* tab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i));
		if (!tab || !tab->GetEditor()) {
			continue;
		}
		Map* map = &tab->GetEditor()->map;
		if (!visited_maps.insert(map).second) {
			continue;
		}

		for (const auto& tile_loc : map->tiles()) {
			const Tile* tile = tile_loc.get();
			if (tile && tile->creature) {
				const std::string& name = tile->creature->getName();
				if (!name.empty() && g_creatures[name] == nullptr) {
					g_creatures.addMissingCreatureType(name, tile->creature->isNpc());
				}
				for (const auto& alt : tile->creature->getAlternatives()) {
					if (!alt.name.empty() && g_creatures[alt.name] == nullptr) {
						g_creatures.addMissingCreatureType(alt.name, false);
					}
				}
			}
		}
	}
}

void VersionManager::UnloadVersion(bool full_clear) {
	UnnamedRenderingLock();
	g_gui.ReleaseAllChunkCaches();
	g_gui.gfx.clear();
	if (g_gui.tool_options) {
		g_gui.tool_options->Clear();
	}
	if (g_gui.brush_items_window) {
		g_gui.brush_items_window->SetActiveBrush(nullptr);
	}
	g_brush_manager.Clear();

	if (!loaded_version.empty()) {
		g_materials.clear();
		g_brushes.clear();
		g_item_definitions.clear();
		g_gui.gfx.clear();

		// FileName cdb = getLoadedVersion()->getLocalDataPath();
		// cdb.SetFullName("creatures.xml");
		// g_creatures.saveToXML(cdb); // Disabled to prevent dual source of truth
		g_creatures.clear(!full_clear);

		loaded_version = CLIENT_VERSION_NONE;
	}
}
