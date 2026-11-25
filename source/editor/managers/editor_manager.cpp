//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "app/application.h"
#include "app/managers/prepared_version_load.h"
#include "app/managers/version_manager.h"
#include "brushes/managers/brush_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/managers/editor_manager.h"
#include "editor/persistence/map_load_decision.h"
#include "editor/persistence/switch_client_preflight.h"
#include "io/iomap_otbm.h"
#include "live/live_client.h"
#include "live/live_tab.h"
#include "map/map_initial_position_resolver.h"
#include "palette/managers/palette_manager.h"
#include "palette/palette_window.h"
#include "ui/dialog_util.h"
#include "ui/dialogs/client_mismatch_dialog.h"
#include "ui/gui.h"
#include "ui/main_menubar.h"
#include "ui/managers/status_manager.h"
#include "ui/map_tab.h"
#include "ui/welcome_dialog.h"

#include <chrono>
#include <filesystem>
#include <format>
#include <set>
#include <sstream>

EditorManager g_editors;

EditorManager::EditorManager() {
}

EditorManager::~EditorManager() {
}

EditorTab* EditorManager::GetCurrentTab() {
	return g_gui.tabbook->GetCurrentTab();
}

EditorTab* EditorManager::GetTab(int idx) {
	return g_gui.tabbook->GetTab(idx);
}

int EditorManager::GetTabCount() const {
	return g_gui.tabbook->GetTabCount();
}

bool EditorManager::IsAnyEditorOpen() const {
	return g_gui.tabbook && g_gui.tabbook->GetTabCount() > 0;
}

bool EditorManager::IsEditorOpen() const {
	return g_gui.tabbook != nullptr && GetCurrentMapTab();
}

void EditorManager::CloseTab(int idx)
{
    if (!g_gui.tabbook || idx < 0 || idx >= g_gui.tabbook->GetTabCount()) {
        return;
    }

    spdlog::info("EditorManager::CloseTab - Closing tab {}", idx);
    if (idx == g_gui.tabbook->GetSelection()) {
        g_palettes.RefreshPalettes(nullptr, false);
    }

    {
		// Destroying the tab frees the whole map and its GL resources, which
		// can take a moment on large maps; tell the user what is happening.
		ScopedInfoBar closing("Closing map...");
        g_gui.tabbook->DeleteTab(idx);
    }

	// wxAuiNotebook does not emit PAGE_CLOSED when the close event was vetoed
	// and the page was deleted here, so report the result from this path too.
	g_status.SetStatusText("Map closed.");
    if (g_gui.root) {
        g_gui.root->UpdateMenubar();
    }
    spdlog::info("EditorManager::CloseTab - Closed tab {}", idx);
}

void EditorManager::CloseCurrentEditor()
{
    if (!g_gui.tabbook) {
        return;
    }
    CloseTab(g_gui.tabbook->GetSelection());
}

Editor* EditorManager::GetCurrentEditor() {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		return mapTab->GetEditor();
	}
	return nullptr;
}

MapTab* EditorManager::GetCurrentMapTab() const {
	if (g_gui.tabbook && g_gui.tabbook->GetTabCount() > 0) {
		EditorTab* editorTab = g_gui.tabbook->GetCurrentTab();
		auto* mapTab = dynamic_cast<MapTab*>(editorTab);
		return mapTab;
	}
	return nullptr;
}

bool EditorManager::CloseLiveEditors(LiveSocket* sock) {
	for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
		auto* mapTab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i));
		if (mapTab) {
			Editor* editor = mapTab->GetEditor();
			if (editor->live_manager.GetClient() == sock) {
				g_gui.tabbook->DeleteTab(i--);
			}
		}
		auto* liveLogTab = dynamic_cast<LiveLogTab*>(g_gui.tabbook->GetTab(i));
		if (liveLogTab) {
			if (liveLogTab->GetSocket() == sock) {
				liveLogTab->Disconnect();
				g_gui.tabbook->DeleteTab(i--);
			}
		}
	}
	g_gui.root->UpdateMenubar();
	return true;
}

bool EditorManager::CloseAllEditors() {
	spdlog::info("EditorManager::CloseAllEditors - Closing all tabs");
	if (!g_gui.tabbook) {
		return true;
	}
	if (g_gui.root && !g_gui.root->DoQuerySaveTileset()) {
		spdlog::info("EditorManager::CloseAllEditors - Cancelled by user");
		return false;
	}

	std::vector<wxWindow*> map_tab_windows;
	map_tab_windows.reserve(g_gui.tabbook->GetTabCount());
	for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
		if (auto* mapTab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i))) {
			map_tab_windows.push_back(mapTab->GetWindow());
		}
	}

	// Pass 1: Prompt once per unique open editor before deleting any tab.
	std::set<Editor*> visited_editors;
	for (wxWindow* tab_window : map_tab_windows) {
		const int tab_idx = g_gui.tabbook->GetTabIndex(tab_window);
		if (tab_idx == wxNOT_FOUND) {
			continue;
		}
		auto* mapTab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(tab_idx));
		if (!mapTab) {
			continue;
		}
		Editor* editor = mapTab->GetEditor();
		if (!editor || !visited_editors.insert(editor).second) {
			continue;
		}
		if (ShouldSave(*editor) || editor->live_manager.IsLive()) {
			g_gui.tabbook->SetFocusedTab(tab_idx);
			if (!g_gui.root || !g_gui.root->DoQuerySave(*editor, mapTab, false)) {
				spdlog::info("EditorManager::CloseAllEditors - Cancelled by user");
				return false;
			}
		}
	}

	// Pass 2: All prompts succeeded; remove every MapTab cleanly.
	for (int i = g_gui.tabbook->GetTabCount() - 1; i >= 0; --i) {
		if (dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i)) != nullptr) {
			g_gui.tabbook->DeleteTab(i);
		}
	}

	g_palettes.RefreshPalettes(nullptr, false);
	if (g_gui.root) {
		g_gui.root->UpdateMenubar();
	}
	spdlog::info("EditorManager::CloseAllEditors - All tabs closed");
	return true;
}

void EditorManager::NewMapView() {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		auto* newMapTab = new MapTab(mapTab);
		newMapTab->OnSwitchEditorMode(mapTab->GetMode());

		g_status.SetStatusText("Created new view");
		g_status.UpdateTitle();
		g_palettes.RefreshPalettes();
		g_gui.root->UpdateMenubar();
		g_gui.root->Refresh();
	}
}

Map& EditorManager::GetCurrentMap() {
	Editor* editor = GetCurrentEditor();
	ASSERT(editor);
	return editor->map;
}

int EditorManager::GetOpenMapCount() {
	std::set<Map*> open_maps;

	for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
		auto* tab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i));
		if (tab) {
			open_maps.insert(tab->GetMap());
		}
	}

	return static_cast<int>(open_maps.size());
}

int EditorManager::CountRealOpenMaps() const
{
    if (!g_gui.tabbook) {
        return 0;
    }
    std::set<const Map*> real_maps;
    for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
        const auto* tab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(i));
        if (tab && tab->GetMap()) {
            const Map* map = tab->GetMap();
            if (map->hasChanged() || map->hasFile()) {
                real_maps.insert(map);
            }
        }
    }
    return static_cast<int>(real_maps.size());
}

bool EditorManager::SwitchClientVersionForLoad(ClientVersion& target)
{
    wxString error;
    if (!g_version.CheckModularClientData(target, error)) {
        g_status.SetStatusText("Failed to switch client version.");
        DialogUtil::PopupDialog(g_gui.root, "Cannot Switch Client", error, wxOK | wxICON_WARNING);
        return false;
    }

    if (!target.hasConfiguredClientPath()) {
        g_status.SetStatusText("Target client is not configured.");
        DialogUtil::PopupDialog(
            g_gui.root,
            "Client Not Configured",
            wxstr(std::format(
                "Client {} folder is not configured.\n\nPlease configure it in File -> Preferences -> Client Version and try again.",
                target.getName()
            )),
            wxOK | wxICON_WARNING
        );
        return false;
    }

    if (!target.hasValidPaths()) {
        g_status.SetStatusText("Target client files are invalid.");
        DialogUtil::PopupDialog(
            g_gui.root,
            "Invalid Client Files",
            wxstr(std::format(
                "Client {} files could not be validated at the configured folder.\n\nPlease check the path in File -> Preferences -> Client Version and try again.",
                target.getName()
            )),
            wxOK | wxICON_WARNING
        );
        return false;
    }

    std::vector<std::string> warnings;
    PreparedVersionLoad prepared;
    if (!g_version.PrepareVersionLoad(target.getID(), prepared, error, warnings)) {
        g_status.SetStatusText("Failed to switch client version.");
        DialogUtil::PopupDialog(g_gui.root, "Error", error, wxOK | wxICON_ERROR);
        return false;
    }

    if (!CloseAllEditors()) {
        g_status.SetStatusText("Map load cancelled.");
        return false;
    }

    if (!g_version.CommitPreparedVersion(prepared, error, warnings)) {
        g_status.SetStatusText("Failed to load map.");
        DialogUtil::PopupDialog(g_gui.root, "Error", error, wxOK | wxICON_ERROR);
        return false;
    }
    DialogUtil::LoadWarningsDialog(g_gui.root, "Warnings", warnings);
    return true;
}

bool EditorManager::ShouldSave(const Editor& editor) const
{
    const Map& map = editor.map;
    if (map.hasChanged()) {
		if (map.getTileCount() == 0) {
            return editor.actionQueue->canUndo();
        }
		return true;
	}
	return false;
}

bool EditorManager::ShouldSave()
{
    Editor* editor = GetCurrentEditor();
    return editor ? ShouldSave(*editor) : false;
}

bool EditorManager::SaveMap(Editor& editor, MapTab* mapTab, FileName fileName, bool showdialog)
{
    g_status.SetStatusText("Saving map...");
    if (g_gui.root) {
        g_gui.root->Update();
    }

    const auto save_started = std::chrono::steady_clock::now();
    const bool success = EditorPersistence::saveMap(editor, fileName, showdialog);
    const double save_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - save_started).count();

    if (success && !editor.map.hasChanged()) {
        // Report what actually landed on disk, not just that it worked.
        std::error_code size_ec;
        const auto file_size = std::filesystem::file_size(std::filesystem::path(editor.map.getFilename()), size_ec);
        if (size_ec) {
            g_status.SetStatusText("Map saved successfully.");
            spdlog::info("Map saved: {} in {:.1f} s", editor.map.getName(), save_seconds);
        } else {
            const double megabytes = static_cast<double>(file_size) / (1024.0 * 1024.0);
            g_status.SetStatusText(std::format("Map saved: {} ({:.1f} MB) in {:.1f} s.", editor.map.getName(), megabytes, save_seconds));
            spdlog::info("Map saved: {} ({:.1f} MB) in {:.1f} s", editor.map.getName(), megabytes, save_seconds);
        }
    }

    if (success && mapTab) {
        const std::string& path = editor.map.getFilename();
        const Position& position = mapTab->GetScreenCenterPosition();
        std::ostringstream stream;
        stream << position;
        g_settings.setString(Config::RECENT_EDITED_MAP_PATH, path);
        g_settings.setString(Config::RECENT_EDITED_MAP_POSITION, stream.str());
    }

    g_status.UpdateTitle();
    if (success && g_gui.root) {
        // A cancelled or failed save left the document untouched; skip
        // the full frame/canvas repaint and the menubar refresh.
        g_gui.root->UpdateMenubar();
        g_gui.root->Refresh();
    }
    return success;
}

bool EditorManager::SaveCurrentMap(FileName fileName, bool showdialog) {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		Editor* editor = mapTab->GetEditor();
		if (editor) {
            return SaveMap(*editor, mapTab, fileName, showdialog);
        }
	}

	return false;
}

bool EditorManager::NewMap() {
	spdlog::info("EditorManager::NewMap - Creating new map");
	g_gui.FinishWelcomeDialog();

	std::unique_ptr<Editor> editor;
	try {
		editor = EditorFactory::CreateEmpty(g_gui.copybuffer);
	} catch (std::runtime_error& e) {
		DialogUtil::PopupDialog(g_gui.root, "Error!", wxString(e.what(), wxConvUTF8), wxOK);
		return false;
	}

	auto* mapTab = new MapTab(g_gui.tabbook, editor.release());
	mapTab->OnSwitchEditorMode(SELECTION_MODE);
	mapTab->GetMap()->clearChanges();
	mapTab->GetView()->ResetMinimapViewportToCurrentView();
	g_minimap.InvalidateAll(*mapTab->GetMap());

	g_status.SetStatusText("Created new map");
	g_status.UpdateTitle();
	g_palettes.RefreshPalettes();
	g_gui.root->UpdateMenubar();
	g_gui.root->Refresh();
	g_gui.UpdateMinimap(true);

	return true;
}

void EditorManager::OpenMap() {
	wxString wildcard = MAP_LOAD_FILE_WILDCARD;
	wxFileDialog dialog(g_gui.root, "Open map file", wxEmptyString, wxEmptyString, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dialog.ShowModal() == wxID_OK) {
		LoadMap(dialog.GetPath());
	}
}

void EditorManager::SaveMap() {
	if (!IsEditorOpen()) {
		return;
	}

	if (GetCurrentMap().hasFile()) {
		SaveCurrentMap(FileName(""), true);
	} else {
		wxString wildcard = MAP_SAVE_FILE_WILDCARD;
		wxFileDialog dialog(g_gui.root, "Save...", wxEmptyString, wxEmptyString, wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

		if (dialog.ShowModal() == wxID_OK) {
			SaveCurrentMap(dialog.GetPath(), true);
		}
	}
}

void EditorManager::SaveMapAs() {
	if (!IsEditorOpen()) {
		return;
	}

	wxString wildcard = "OpenTibia Binary Map (*.otbm)|*.otbm|GZIP Compressed Map (*.otbm)|*.otbm";
	wxFileDialog dialog(g_gui.root, "Save As...", "", "", wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	dialog.SetFilterIndex(GetCurrentMap().getCompression() == OtbmCompression::Gzip ? 1 : 0);

	if (dialog.ShowModal() == wxID_OK) {
		const OtbmCompression prev_compression = GetCurrentMap().getCompression();
		if (dialog.GetFilterIndex() == 1) {
			GetCurrentMap().setCompression(OtbmCompression::Gzip);
		} else if (dialog.GetFilterIndex() == 0) {
			GetCurrentMap().setCompression(OtbmCompression::None);
		}

		if (!SaveCurrentMap(dialog.GetPath(), true)) {
			GetCurrentMap().setCompression(prev_compression);
			return;
		}

		g_status.UpdateTitle();
		g_gui.root->AddRecentFile(dialog.GetPath());
		g_gui.root->UpdateMenubar();
	}
}

void EditorManager::SaveMapAsGzip() {
	if (!IsEditorOpen()) {
		return;
	}

	wxString wildcard = "GZIP Compressed Map (*.otbm)|*.otbm";
	wxFileDialog dialog(g_gui.root, "Save as GZIP...", "", "", wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if (dialog.ShowModal() == wxID_OK) {
		const OtbmCompression prev_compression = GetCurrentMap().getCompression();
		GetCurrentMap().setCompression(OtbmCompression::Gzip);

		if (!SaveCurrentMap(dialog.GetPath(), true)) {
			GetCurrentMap().setCompression(prev_compression);
			return;
		}

		g_status.UpdateTitle();
		g_gui.root->AddRecentFile(dialog.GetPath());
		g_gui.root->UpdateMenubar();
	}
}

bool EditorManager::LoadMap(const FileName& fileName, const MapLoadOptions& initial_load_options)
{
    spdlog::info("EditorManager::LoadMap - Loading map: {}", nstr(fileName.GetFullPath()));
    const auto load_started = std::chrono::steady_clock::now();
    g_status.SetStatusText("Loading map...");
	if (g_gui.root) {
		g_gui.root->Update();
	}
	g_gui.FinishWelcomeDialog();

    MapLoadOptions load_options = initial_load_options;
    MapTab* placeholder_tab = nullptr;
    std::unique_ptr<Editor> editor;
    try {
        MapVersion ver;
		if (!IOMapOTBM::getVersionInfo(fileName, ver)) {
			throw std::runtime_error(std::format("Could not open file \"{}\".\nThis is not a valid OTBM file or it does not exist.", nstr(fileName.GetFullPath())));
		}

		ClientVersion* target = nullptr;
		if (load_options.hasSelectedClient()) {
            target = ClientVersion::get(load_options.selected_client_id);
        } else {
            target = ClientVersion::getByItemsVersion(ver.items_major, ver.client);
			if (!target) {
				target = ClientVersion::getBestMatch(ver.client);
            }
        }

        ClientVersion* current = g_version.getLoadedVersion();
        const MapLoadDecisionInput decision_input {
            .loaded_client_id = current ? current->getID() : "",
            .resolved_target_id = target ? target->getID() : "",
            .loaded_protocol = current ? current->getProtocolID() : 0,
            .loaded_items_major = current ? static_cast<uint32_t>(current->getOTBVersion().format_version) : 0,
            .map_protocol = ver.client,
            .map_items_major = ver.items_major,
            .options = load_options,
        };

        const auto decision = DecideMapLoadAction(decision_input);
        if (decision.resolution == MapLoadResolution::UnsupportedVersion) {
            if (load_options.hasSelectedClient()) {
                throw std::runtime_error(std::format("Unsupported client selection: {}", load_options.selected_client_id));
            }
            throw std::runtime_error(std::format("Unsupported client version (OtbId: {})", static_cast<int>(ver.client)));
        }

        if (decision.resolution == MapLoadResolution::PromptMismatch) {
            wxString modular_error;
            const bool has_modular_data = g_version.CheckModularClientData(*target, modular_error);
            const bool has_configured_paths = target->hasConfiguredClientPath() && target->getClientPath().DirExists();
            const SwitchPreconditionStatus switch_status = EvaluateSwitchPrecondition(SwitchPreconditionInput {
                .is_same_client = current->getID() == target->getID(),
                .has_modular_data = has_modular_data,
                .has_configured_client_paths = has_configured_paths,
            });
            ClientMismatchDialog dlg(
                g_gui.root, *current, *target, ver, fileName.GetFullName().utf8_string(), CountRealOpenMaps(), switch_status
            );
            dlg.ShowModal();

            const ClientMismatchChoice choice = dlg.GetChoice();
            if (choice == ClientMismatchChoice::ForceLoadActive) {
                // Re-read loaded version in case another load ran while the modal dialog was open.
                ClientVersion* active_current = g_version.getLoadedVersion();
                if (!active_current) {
                    if (!SwitchClientVersionForLoad(*target)) {
                        return false;
                    }
                } else {
                    load_options.selected_client_id = active_current->getID();
                    load_options.force_client_mismatch = true;
                }
            } else if (choice == ClientMismatchChoice::SwitchVersion) {
                if (!SwitchClientVersionForLoad(*target)) {
                    return false;
                }
            } else {
                g_status.SetStatusText("Map load cancelled.");
                return false;
            }
        } else if (decision.requires_switch) {
            if (!SwitchClientVersionForLoad(*target)) {
                return false;
            }
        }

        if (MapTab* active_tab = GetCurrentMapTab(); active_tab && active_tab->IsUniqueReference() && active_tab->GetMap()
            && !active_tab->GetMap()->hasChanged() && !active_tab->GetMap()->hasFile()) {
            placeholder_tab = active_tab;
        }

        editor = EditorFactory::LoadFromFile(g_gui.copybuffer, fileName, load_options);
    } catch (std::runtime_error& e) {
        g_status.SetStatusText("Failed to load map.");
		DialogUtil::PopupDialog(g_gui.root, "Error!", wxString(e.what(), wxConvUTF8), wxOK);
		return false;
    }

    auto* mapTab = new MapTab(g_gui.tabbook, editor.release());
	mapTab->OnSwitchEditorMode(SELECTION_MODE);

    if (placeholder_tab && g_gui.tabbook) {
        for (int i = 0; i < g_gui.tabbook->GetTabCount(); ++i) {
            if (g_gui.tabbook->GetTab(i) == placeholder_tab && placeholder_tab->GetMap() && !placeholder_tab->GetMap()->hasChanged()
                && !placeholder_tab->GetMap()->hasFile()) {
                g_gui.tabbook->DeleteTab(i);
                break;
            }
        }
    }

    g_gui.root->AddRecentFile(fileName);

    mapTab->GetView()->FitToMap();
	g_status.UpdateTitle();
	DialogUtil::LoadWarningsDialog(g_gui.root, "Map loader errors", mapTab->GetMap()->getWarnings());
	g_gui.root->DoQueryImportCreatures();

	g_gui.FitViewToMap(mapTab);
	g_gui.root->UpdateMenubar();

	const Position initial_pos = MapInitialPositionResolver::Resolve(*mapTab->GetMap(), fileName.GetFullPath().ToStdString());
	mapTab->SetScreenCenterPosition(initial_pos);

	mapTab->GetView()->ResetMinimapViewportToCurrentView();
	g_minimap.InvalidateAll(*mapTab->GetMap());
	g_gui.UpdateMinimap(true);

	const uint64_t loaded_tiles = mapTab->GetMap()->getTileCount();
	const double load_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - load_started).count();
	if (loaded_tiles > 0) {
		g_status.SetStatusText(std::format("Map loaded: {} ({} tiles) in {:.1f} s.", nstr(fileName.GetFullName()), loaded_tiles, load_seconds));
		spdlog::info("Map loaded: {} ({} tiles) in {:.1f} s", nstr(fileName.GetFullName()), loaded_tiles, load_seconds);
	} else {
		std::error_code size_ec;
		const auto bytes = std::filesystem::file_size(std::filesystem::path(fileName.GetFullPath().utf8_string()), size_ec);
		if (size_ec) {
			g_status.SetStatusText("Map loaded successfully.");
		} else {
			const double megabytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
			g_status.SetStatusText(std::format("Map loaded: {} ({:.1f} MB) in {:.1f} s.", nstr(fileName.GetFullName()), megabytes, load_seconds));
			spdlog::info("Map loaded: {} ({:.1f} MB) in {:.1f} s", nstr(fileName.GetFullName()), megabytes, load_seconds);
		}
	}
	return true;
}

void EditorManager::DoCut() {
	if (!g_gui.IsSelectionMode()) {
		return;
	}

	Editor* editor = GetCurrentEditor();
	if (!editor) {
		return;
	}

	editor->copybuffer.cut(*editor, g_gui.GetCurrentFloor());
	g_gui.RefreshView();
	g_gui.root->UpdateMenubar();
}

void EditorManager::DoCopy() {
	if (!g_gui.IsSelectionMode()) {
		return;
	}

	Editor* editor = GetCurrentEditor();
	if (!editor) {
		return;
	}

	editor->copybuffer.copy(*editor, g_gui.GetCurrentFloor());
	g_gui.RefreshView();
	g_gui.root->UpdateMenubar();
}

void EditorManager::DoPaste() {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		g_gui.copybuffer.paste(*mapTab->GetEditor(), mapTab->GetCanvas()->GetCursorPosition());
	}
}

void EditorManager::PreparePaste() {
	Editor* editor = GetCurrentEditor();
	if (editor) {
		g_gui.SetSelectionMode();
		editor->selection.start();
		editor->selection.clear();
		editor->selection.finish();
		StartPasting();
		g_gui.RefreshView();
	}
}

void EditorManager::StartPasting() {
	MapTab* mapTab = g_gui.GetCurrentMapTab();
	if (mapTab) {
		g_gui.pasting = true;
		g_gui.SetCurrentMapSecondaryMap(&g_gui.copybuffer.getBufferMap());
	}
}

void EditorManager::EndPasting() {
	if (g_gui.pasting) {
		g_gui.pasting = false;
		g_gui.SetCurrentMapSecondaryMap(nullptr);
	}
}

bool EditorManager::CanUndo() {
	Editor* editor = GetCurrentEditor();
	return (editor && editor->actionQueue->canUndo());
}

bool EditorManager::CanRedo() {
	Editor* editor = GetCurrentEditor();
	return (editor && editor->actionQueue->canRedo());
}

bool EditorManager::DoUndo() {
	Editor* editor = GetCurrentEditor();
	if (editor && editor->actionQueue->canUndo()) {
		editor->actionQueue->undo();
		if (!editor->selection.empty()) {
			g_gui.SetSelectionMode();
		}
		g_status.SetStatusText("Undo action");
		g_gui.UpdateMinimap();
		g_gui.UpdateZonePalette(0);
		g_gui.UpdateHousePalette(0);
		g_gui.root->UpdateMenubar();
		g_gui.root->Refresh();
		return true;
	}
	return false;
}

bool EditorManager::DoRedo() {
	Editor* editor = GetCurrentEditor();
	if (editor && editor->actionQueue->canRedo()) {
		editor->actionQueue->redo();
		if (!editor->selection.empty()) {
			g_gui.SetSelectionMode();
		}
		g_status.SetStatusText("Redo action");
		g_gui.UpdateMinimap();
		g_gui.UpdateZonePalette(0);
		g_gui.UpdateHousePalette(0);
		g_gui.root->UpdateMenubar();
		g_gui.root->Refresh();
		return true;
	}
	return false;
}
