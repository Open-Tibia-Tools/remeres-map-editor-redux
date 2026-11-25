#ifndef RME_APP_MANAGERS_PREPARED_VERSION_LOAD_H_
#define RME_APP_MANAGERS_PREPARED_VERSION_LOAD_H_

#include "app/client_version.h"
#include "game/material_manifest.h"
#include "item_definitions/core/asset_bundle.h"

#include <string>
#include <vector>

struct PreparedVersionLoad {
    ClientVersionID version = CLIENT_VERSION_NONE;
    wxString modular_data_path;
    MaterialManifestFiles manifest_files;
    AssetBundle bundle;
    bool has_otb = false;
    std::vector<std::string> warnings;
};

#endif // RME_APP_MANAGERS_PREPARED_VERSION_LOAD_H_
