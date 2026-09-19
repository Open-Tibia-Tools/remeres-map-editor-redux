//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_VERSION_MANAGER_H_
#define RME_VERSION_MANAGER_H_

#include "app/definitions.h"
#include "app/client_version.h"
#include "item_definitions/core/missing_item_report.h"

struct MaterialManifestFiles;

class VersionManager {
public:
	VersionManager();
	~VersionManager();

	void UnloadVersion();
	bool LoadVersion(ClientVersionID ver, std::string& error, std::vector<std::string>& warnings, bool force = false);

	// The current version loaded (returns CLIENT_VERSION_NONE if no version is loaded)
	const ClientVersion& GetCurrentVersion() const;
	ClientVersionID GetCurrentVersionID() const;

	// If any version is loaded at all
	bool IsVersionLoaded() const {
		return !loaded_version.empty();
	}

	ClientVersion* getLoadedVersion() const {
		return loaded_version.empty() ? nullptr : ClientVersion::get(loaded_version);
	}

	// Access the missing items report from the last version load
	const MissingItemReport& getLastMissingItems() const {
		return last_missing_items;
	}

	// Check if the last loaded version uses OTB
	bool lastLoadHasOtb() const {
		return last_load_has_otb;
	}

private:
	bool PromptForClientPaths(ClientVersion* version);
	bool LoadDataFiles(std::string& error, std::vector<std::string>& warnings);
	[[nodiscard]] std::filesystem::path GetModularDataPath() const;
	void FailDataLoad();
	bool LoadMaterialManifest(const std::filesystem::path& materials_manifest, MaterialManifestFiles& manifest_files, std::string& error);
	bool LoadCanonicalAssets(const std::filesystem::path& modular_data_path, const MaterialManifestFiles& manifest_files, std::string& error, std::vector<std::string>& warnings);
	void AppendMissingItemWarnings(std::vector<std::string>& warnings) const;
	bool LoadCreatureFiles(const MaterialManifestFiles& manifest_files, std::string& error, std::vector<std::string>& warnings);
	bool LoadModularMaterials(const std::filesystem::path& materials_manifest, std::string& error, std::vector<std::string>& warnings);

	ClientVersionID loaded_version;
	MissingItemReport last_missing_items;
	bool last_load_has_otb = true;
};

extern VersionManager g_version;

#endif
