#include "ui/menubar/map_actions_handler.h"
#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "ui/find_item_window.h"
#include "editor/editor.h"
#include "editor/operations/clean_operations.h"
#include "editor/operations/search_operations.h"
#include "editor/operations/unreachable_cleaner.h"
#include "editor/persistence/map_backup_service.h"
#include "ui/dialogs/remove_unreachable_dialog.h"
#include "map/map.h"
#include "editor/action_queue.h"

MapActionsHandler::MapActionsHandler(MainFrame* frame) :
	frame(frame) {
}

void MapActionsHandler::OnMapRemoveItems(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	FindItemDialog dialog(frame, "Item Type to Remove");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t itemid = dialog.getResultID();

		g_gui.GetCurrentEditor()->selection.clear();
		g_gui.GetCurrentEditor()->actionQueue->clear();

		EditorOperations::RemoveItemCondition condition(itemid, [](uint32_t percent) { g_gui.SetLoadDone(percent); });
		g_gui.CreateLoadBar("Searching map for items to remove...");

		int64_t count = RemoveItemOnMap(g_gui.GetCurrentMap(), condition, false);

		g_gui.DestroyLoadBar();

		wxString msg;
		msg << count << " items deleted.";

		g_gui.SetStatusText(msg);
		g_gui.GetCurrentMap().doChange();
		g_gui.RefreshView();
	}
	dialog.Destroy();
}

void MapActionsHandler::OnMapRemoveUnreachable(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	RemoveUnreachableDialog dialog(frame);
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	const auto settings = dialog.GetSettings();

	g_gui.CreateLoadBar("Analyzing unreachable tiles...");
	uint64_t total_checked = 0;
	std::vector<Position> to_remove = EditorOperations::UnreachableCleaner::FindUnreachableTiles(
		editor->map,
		settings,
		[](uint32_t percent) { g_gui.SetLoadDone(percent); },
		&total_checked
	);
	g_gui.DestroyLoadBar();

	if (to_remove.empty()) {
		DialogUtil::PopupDialog("Remove Unreachable Tiles", "No unreachable tiles were found on the map.", wxOK);
		return;
	}

	wxString prompt;
	prompt << "Found " << to_remove.size() << " unreachable tile(s) that are never visible from any player viewpoint.\n\n";
	if (settings.create_backup) {
		prompt << "A backup will be created before any tiles are removed.\n\n";
	}
	prompt << "Do you want to permanently delete these tiles?";

	int confirm = DialogUtil::PopupDialog("Remove Unreachable Tiles", prompt, wxYES | wxNO);
	if (confirm != wxID_YES) {
		return;
	}

	auto run_settings = settings;
	if (settings.create_backup) {
		std::string backup_path;
		std::string backup_err;
		if (!MapBackupService::CreateSnapshot(*editor, backup_path, backup_err, "unreachable_clean")) {
			int proceed = DialogUtil::PopupDialog(
				"Backup Failed",
				"Failed to create map backup snapshot:\n" + backup_err + "\n\nDo you want to proceed with tile removal anyway?",
				wxYES | wxNO
			);
			if (proceed != wxID_YES) {
				return;
			}
			run_settings.create_backup = false;
		}
	}

	g_gui.CreateLoadBar("Removing unreachable tiles...");
	auto result = EditorOperations::UnreachableCleaner::Apply(
		*editor,
		to_remove,
		run_settings,
		[](uint32_t percent) { g_gui.SetLoadDone(percent); }
	);
	g_gui.DestroyLoadBar();

	g_gui.RefreshView();

	wxString status;
	status << "Removed " << result.tiles_removed << " unreachable tile(s).";
	if (result.backup_created) {
		status << " Backup: " << wxstr(result.backup_path);
	}
	g_gui.SetStatusText(status);

	wxString complete_msg;
	complete_msg << "Successfully removed " << result.tiles_removed << " unreachable tile(s).";
	if (result.backup_created) {
		complete_msg << "\n\nBackup saved to:\n" << wxstr(result.backup_path);
	}
	DialogUtil::PopupDialog("Removal Complete", complete_msg, wxOK);
}

void MapActionsHandler::OnClearHouseTiles(wxCommandEvent& WXUNUSED(event)) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	int ret = DialogUtil::PopupDialog(
		"Clear Invalid House Tiles",
		"Are you sure you want to remove all house tiles that do not belong to a house (this action cannot be undone)?",
		wxYES | wxNO
	);

	if (ret == wxID_YES) {
		ScopedLoadingBar loading("Clearing invalid house tiles...");
		editor->clearInvalidHouseTiles([&](int percent) {
			loading.SetLoadDone(percent);
		});
	}

	g_gui.RefreshView();
}

void MapActionsHandler::OnClearModifiedState(wxCommandEvent& WXUNUSED(event)) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	int ret = DialogUtil::PopupDialog(
		"Clear Modified State",
		"This will have the same effect as closing the map and opening it again. Do you want to proceed?",
		wxYES | wxNO
	);

	if (ret == wxID_YES) {
		ScopedLoadingBar loading("Clearing modified state from all tiles...");
		editor->clearModifiedTileState([&](int percent) {
			loading.SetLoadDone(percent);
		});
	}

	g_gui.RefreshView();
}

void MapActionsHandler::OnMapCleanHouseItems(wxCommandEvent& WXUNUSED(event)) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	int ret = DialogUtil::PopupDialog(
		"Clear Moveable House Items",
		"Are you sure you want to remove all items inside houses that can be moved (this action cannot be undone)?",
		wxYES | wxNO
	);

	if (ret == wxID_YES) {
		// Editor will do the work
		// editor->removeHouseItems(true);
	}

	g_gui.RefreshView();
}

void MapActionsHandler::OnBorderizeSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	if (!g_gui.GetCurrentEditor()->borderizeSelection()) {
		g_gui.SetStatusText("No items selected. Can't borderize.");
	}
	g_gui.RefreshView();
}

void MapActionsHandler::OnBorderizeMap(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	int ret = DialogUtil::PopupDialog("Borderize Map", "Are you sure you want to borderize the entire map (this action cannot be undone)?", wxYES | wxNO);
	if (ret == wxID_YES) {
		ScopedLoadingBar loading("Borderizing map...");
		g_gui.GetCurrentEditor()->borderizeMap([&](int percent) {
			loading.SetLoadDone(percent);
		});
	}

	g_gui.RefreshView();
}

void MapActionsHandler::OnRandomizeSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	if (!g_gui.GetCurrentEditor()->randomizeSelection()) {
		g_gui.SetStatusText("No items selected. Can't randomize.");
	}
	g_gui.RefreshView();
}

void MapActionsHandler::OnRandomizeMap(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	int ret = DialogUtil::PopupDialog("Randomize Map", "Are you sure you want to randomize the entire map (this action cannot be undone)?", wxYES | wxNO);
	if (ret == wxID_YES) {
		ScopedLoadingBar loading("Randomizing map...");
		g_gui.GetCurrentEditor()->randomizeMap([&](int percent) {
			loading.SetLoadDone(percent);
		});
	}

	g_gui.RefreshView();
}

void MapActionsHandler::OnMapCleanup(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	int ok = DialogUtil::PopupDialog("Cleanup invalid tiles", "Do you want to remove all invalid or unresolved items from the map?", wxYES | wxNO);

	if (ok == wxID_YES) {
		g_gui.CreateLoadBar("Removing invalid tiles...");
		g_gui.GetCurrentMap().cleanInvalidTiles([](int percent, std::string_view) {
			g_gui.SetLoadDone(percent);
		});
		g_gui.DestroyLoadBar();
		g_gui.RefreshView();
	}
}

void MapActionsHandler::OnMapCleanInvalidZones(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	int ok = DialogUtil::PopupDialog("Cleanup invalid zones", "Do you want to remove all invalid tile flags and opaque OTBM tile fragments from the map?", wxYES | wxNO);

	if (ok == wxID_YES) {
		g_gui.CreateLoadBar("Removing invalid zones...");
		g_gui.GetCurrentMap().cleanInvalidZones([](int percent, std::string_view) {
			g_gui.SetLoadDone(percent);
		});
		g_gui.DestroyLoadBar();
		g_gui.RefreshView();
	}
}
