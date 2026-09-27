#include "app/main.h"
#include "ui/dialog_util.h"
#include "editor/editor_factory.h"
#include "editor/editor.h"
#include "editor/persistence/editor_persistence.h"
#include "app/managers/version_manager.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "io/iomap.h"
#include "live/live_client.h"

#include "ui/tile_properties/tile_properties_panel.h"
#include "map/tile.h"
#include "game/creature.h"
#include "game/spawn.h"

void SetupCallbacks(Editor* editor) {
	editor->onStateChange = [editor]() {
		g_gui.UpdateTitle();
		g_gui.UpdateMenus();

		if (g_gui.tile_properties_panel) {
			g_gui.tile_properties_panel->UpdateFromEditor(editor);
		}
	};

	editor->selection.onSelectionChange = [editor](size_t count) {
		if (count > 0) {
			wxString ss;
			if (count == 1) {
				ss << "One tile selected.";
			} else {
				ss << count << " tiles selected.";
			}
			g_gui.SetStatusText(ss);
		} else {
			// Optional: Clear status text if nothing selected, or keep previous behavior
			// Previous behavior didn't clear it explicitly in updateSelectionCount if size <= 0
		}

		if (g_gui.tile_properties_panel) {
			g_gui.tile_properties_panel->UpdateFromEditor(editor);
		}
	};
}

std::unique_ptr<Editor> EditorFactory::CreateEmpty(CopyBuffer& copybuffer, ClientVersionID preferred) {
	ClientVersionID defaultVersion = preferred;
	if (defaultVersion.empty()) {
		ClientVersion* latest = ClientVersion::getLatestVersion();
		if (latest) {
			defaultVersion = latest->getID();
		}
	}

	if (defaultVersion == CLIENT_VERSION_NONE) {
		OtbVersionID protocolId = g_settings.getInteger(Config::DEFAULT_CLIENT_VERSION);
		if (protocolId != 0) {
			ClientVersion* match = ClientVersion::getBestMatch(protocolId);
			if (match) {
				defaultVersion = match->getID();
			}
		}
	}

	if (ClientVersion* client = ClientVersion::get(defaultVersion)) {
		wxString error;
		std::vector<std::string> warnings;
		if (!g_version.setStartupCatalog(client, g_settings.getBoolean(Config::USE_ITEMS_RON), error, warnings)) {
			throw std::runtime_error(nstr(error));
		}
		if (!warnings.empty()) {
			DialogUtil::ListDialog("Warnings", warnings);
		}
	}

	if (!EnsureVersion(defaultVersion)) {
		throw std::runtime_error("Couldn't load client version");
	}

	MapVersion mapVersion;
	mapVersion.otbm = g_version.GetCurrentVersion().getPrefferedMapVersionID();
	mapVersion.client = g_version.GetCurrentVersion().getProtocolID();

	std::unique_ptr<Editor> editor = std::make_unique<Editor>(copybuffer, mapVersion);
	SetupCallbacks(editor.get());
	return editor;
}

std::unique_ptr<Editor> EditorFactory::LoadFromFile(CopyBuffer& copybuffer, const FileName& fn, const MapLoadOptions& load_options) {
	MapVersion mapVersion;
	mapVersion.otbm = g_version.GetCurrentVersion().getPrefferedMapVersionID();
	mapVersion.client = g_version.GetCurrentVersion().getProtocolID();

	std::unique_ptr<Editor> editor = std::make_unique<Editor>(copybuffer, mapVersion, fn, load_options);
	SetupCallbacks(editor.get());
	return editor;
}

std::unique_ptr<Editor> EditorFactory::JoinLive(CopyBuffer& copybuffer, std::unique_ptr<LiveClient> client) {
	MapVersion mapVersion;
	mapVersion.otbm = g_version.GetCurrentVersion().getPrefferedMapVersionID();
	mapVersion.client = g_version.GetCurrentVersion().getProtocolID();

	std::unique_ptr<Editor> editor = std::make_unique<Editor>(copybuffer, mapVersion, std::move(client));
	SetupCallbacks(editor.get());
	return editor;
}

bool EditorFactory::EnsureVersion(ClientVersionID version) {
	if (g_version.GetCurrentVersionID() == version) {
		return true;
	}

	if (!g_gui.CloseAllEditors()) {
		return false;
	}

	wxString error;
	std::vector<std::string> warnings;
	if (g_version.LoadVersion(version, error, warnings)) {
		if (!warnings.empty()) {
			DialogUtil::ListDialog("Warnings", warnings);
		}
		return true;
	} else {
		DialogUtil::PopupDialog("Error", error, wxOK);
		return false;
	}
}
