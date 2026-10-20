#include "ui/dialogs/spawn_validator_window.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "editor/action.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/map_window.h"
#include "ui/properties/creature_properties_window.h"
#include "ui/properties/spawn_properties_window.h"
#include "map/tile_operations.h"
#include "map/tile.h"
#include "game/creature.h"
#include "game/spawn.h"
#include "util/image_manager.h"

#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>
#include <format>
#include <algorithm>

namespace {
	constexpr int ID_SCAN_MAP = 10001;
	constexpr int ID_JUMP_POS = 10002;
	constexpr int ID_EDIT_ISSUE = 10003;
	constexpr int ID_REMOVE_ISSUE = 10004;
	constexpr int ID_REMOVE_ALL = 10005;
	constexpr int ID_SEARCH_FILTER = 10006;
	constexpr int ID_KIND_FILTER = 10007;
}

SpawnValidatorWindow::SpawnValidatorWindow(wxWindow* parent, Editor* editor) :
	wxDialog(parent, wxID_ANY, "Spawn Validator", wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
	editor(editor) {
	SetSize(FromDIP(wxSize(980, 620)));
	SetMinSize(FromDIP(wxSize(750, 480)));
	CenterOnParent();
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	InitLayout();

	Bind(wxEVT_CLOSE_WINDOW, &SpawnValidatorWindow::OnClose, this);
	Bind(wxEVT_CHAR_HOOK, &SpawnValidatorWindow::OnCharHook, this);
}

SpawnValidatorWindow::~SpawnValidatorWindow() {
	DetachEditor();
	Unbind(wxEVT_CLOSE_WINDOW, &SpawnValidatorWindow::OnClose, this);
	Unbind(wxEVT_CHAR_HOOK, &SpawnValidatorWindow::OnCharHook, this);
}

void SpawnValidatorWindow::DetachEditor() {
	editor = nullptr;
}

void SpawnValidatorWindow::OnClose(wxCloseEvent& event) {
	if (auto* owner = wxDynamicCast(GetParent(), MapWindow); owner != nullptr) {
		owner->ForgetSpawnValidatorDialog();
	}
	Destroy();
}

void SpawnValidatorWindow::OnCharHook(wxKeyEvent& event) {
	const int key = event.GetKeyCode();
	if (key == WXK_ESCAPE) {
		Close();
		return;
	}
	if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
		if (selected_issue_id >= 0 && remove_button && remove_button->IsEnabled()) {
			OnRemoveSelected();
			return;
		}
	}
	if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
		if (selected_issue_id >= 0 && jump_button && jump_button->IsEnabled()) {
			OnJumpToSelected();
			return;
		}
	}
	event.Skip();
}

void SpawnValidatorWindow::InitLayout() {
	auto* rootSizer = newd wxBoxSizer(wxHORIZONTAL);

	// =========================================================================
	// LEFT COLUMN: Rules, Filters & Actions
	// =========================================================================
	auto* leftPanel = newd wxPanel(this, wxID_ANY);
	auto* leftSizer = newd wxBoxSizer(wxVERTICAL);

	// 1. Validation Rules Group (native wxStaticBoxSizer matching RemoveUnreachableDialog)
	auto* rules_box = newd wxStaticBoxSizer(wxVERTICAL, leftPanel, "Validation Rules");

	chk_invalid_time = newd wxCheckBox(rules_box->GetStaticBox(), wxID_ANY, "Invalid spawn time (< 1s, > 24h)");
	chk_unknown_creature = newd wxCheckBox(rules_box->GetStaticBox(), wxID_ANY, "Unknown creature / kind mismatch");
	chk_blocked_tile = newd wxCheckBox(rules_box->GetStaticBox(), wxID_ANY, "Creature on wall / blocked tile");
	chk_empty_tile = newd wxCheckBox(rules_box->GetStaticBox(), wxID_ANY, "Missing / empty tile (server drop)");
	chk_unwalkable_zone = newd wxCheckBox(rules_box->GetStaticBox(), wxID_ANY, "Spawn zone with 0 walkable tiles");

	chk_invalid_time->SetValue(true);
	chk_unknown_creature->SetValue(true);
	chk_blocked_tile->SetValue(true);
	chk_empty_tile->SetValue(true);
	chk_unwalkable_zone->SetValue(true);

	rules_box->Add(chk_invalid_time, 0, wxALL, 5);
	rules_box->Add(chk_unknown_creature, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	rules_box->Add(chk_blocked_tile, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	rules_box->Add(chk_empty_tile, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	rules_box->Add(chk_unwalkable_zone, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	leftSizer->Add(rules_box, 0, wxEXPAND | wxBOTTOM, 10);

	// 2. Filter & Search Group (native wxStaticBoxSizer)
	auto* filter_box = newd wxStaticBoxSizer(wxVERTICAL, leftPanel, "Filter & Search");

	wxArrayString kinds;
	kinds.Add("All");
	kinds.Add("Monsters");
	kinds.Add("NPCs");
	radio_kind_filter = newd wxRadioBox(filter_box->GetStaticBox(), ID_KIND_FILTER, "Kind Filter",
		wxDefaultPosition, wxDefaultSize, kinds, 3, wxRA_SPECIFY_COLS);
	radio_kind_filter->Bind(wxEVT_RADIOBOX, &SpawnValidatorWindow::OnKindFilterChanged, this);
	filter_box->Add(radio_kind_filter, 0, wxEXPAND | wxALL, 5);

	search_ctrl = newd PaletteSearchCtrl(filter_box->GetStaticBox(), ID_SEARCH_FILTER, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Filter results by name or coords...");
	search_ctrl->Bind(wxEVT_TEXT, &SpawnValidatorWindow::OnSearchFilterChanged, this);
	filter_box->Add(search_ctrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	leftSizer->Add(filter_box, 0, wxEXPAND | wxBOTTOM, 10);

	// 3. Scan Map Button
	scan_button = newd wxButton(leftPanel, ID_SCAN_MAP, "Scan Map");
	scan_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION_CROSSHAIRS, wxSize(16, 16)));
	scan_button->SetDefault();
	scan_button->Bind(wxEVT_BUTTON, &SpawnValidatorWindow::OnScanMap, this);
	leftSizer->Add(scan_button, 0, wxEXPAND | wxBOTTOM, 10);

	// 4. Actions Group (native wxStaticBoxSizer)
	auto* actions_box = newd wxStaticBoxSizer(wxVERTICAL, leftPanel, "Actions");
	auto* action_grid = newd wxGridSizer(2, 2, 6, 6);

	jump_button = newd wxButton(actions_box->GetStaticBox(), ID_JUMP_POS, "Jump to Pos");
	jump_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION_ARROW, wxSize(16, 16)));
	jump_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnJumpToSelected(); });
	action_grid->Add(jump_button, 1, wxEXPAND);

	edit_button = newd wxButton(actions_box->GetStaticBox(), ID_EDIT_ISSUE, "Edit...");
	edit_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	edit_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnEditSelected(); });
	action_grid->Add(edit_button, 1, wxEXPAND);

	remove_button = newd wxButton(actions_box->GetStaticBox(), ID_REMOVE_ISSUE, "Remove");
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH, wxSize(16, 16)));
	remove_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRemoveSelected(); });
	action_grid->Add(remove_button, 1, wxEXPAND);

	remove_all_button = newd wxButton(actions_box->GetStaticBox(), ID_REMOVE_ALL, "Remove All");
	remove_all_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH, wxSize(16, 16)));
	remove_all_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRemoveAll(); });
	action_grid->Add(remove_all_button, 1, wxEXPAND);

	actions_box->Add(action_grid, 0, wxEXPAND | wxALL, 5);
	leftSizer->Add(actions_box, 0, wxEXPAND | wxBOTTOM, 10);

	leftSizer->AddStretchSpacer(1);

	status_label = newd wxStaticText(leftPanel, wxID_ANY, "Ready. Configure rules and click 'Scan Map'.");
	status_label->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	leftSizer->Add(status_label, 0, wxEXPAND | wxBOTTOM, 5);

	leftPanel->SetSizer(leftSizer);
	rootSizer->Add(leftPanel, 0, wxEXPAND | wxALL, 10);

	// =========================================================================
	// RIGHT COLUMN: Results List
	// =========================================================================
	auto* rightPanel = newd wxPanel(this, wxID_ANY);
	auto* rightSizer = newd wxBoxSizer(wxVERTICAL);

	auto* results_box = newd wxStaticBoxSizer(wxVERTICAL, rightPanel, "Results");

	issues_table = newd NanoVGTableCtrl(results_box->GetStaticBox(), wxID_ANY);
	issues_table->SetRowHeight(34);

	std::vector<TableColumn> cols = {
		{ "id", "#", 45, NVG_ALIGN_RIGHT, true },
		{ "name", "Name", 130, NVG_ALIGN_LEFT, true },
		{ "kind", "Kind", 75, NVG_ALIGN_CENTER, true },
		{ "pos", "Position", 115, NVG_ALIGN_CENTER, true },
		{ "issue", "Issue", 135, NVG_ALIGN_LEFT, true },
		{ "details", "Details", 0, NVG_ALIGN_LEFT, true }
	};
	issues_table->SetColumns(std::move(cols));

	issues_table->SetOnSelect([this](int idx, const TableRow& row) {
		selected_issue_id = static_cast<int>(row.id);
		UpdateButtonStates();
	});

	issues_table->SetOnActivate([this](int idx, const TableRow& row) {
		selected_issue_id = static_cast<int>(row.id);
		UpdateButtonStates();
		OnJumpToSelected();
	});

	issues_table->SetOnContextMenu([this](int idx, const TableRow* row, const wxPoint& pos) {
		if (!row) {
			return;
		}
		selected_issue_id = static_cast<int>(row->id);
		UpdateButtonStates();

		wxMenu menu;
		menu.Append(ID_JUMP_POS, "Jump to Position\tEnter");
		menu.Append(ID_EDIT_ISSUE, "Edit / Correct...");
		menu.Append(ID_REMOVE_ISSUE, "Remove Selected\tDel");

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnJumpToSelected(); }, ID_JUMP_POS);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnEditSelected(); }, ID_EDIT_ISSUE);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnRemoveSelected(); }, ID_REMOVE_ISSUE);

		PopupMenu(&menu, pos);
	});

	issues_table->SetOnSort([this](int col, bool asc) {
		auto less = [](const auto& a, const auto& b, int c) {
			switch (c) {
				case 0: return a.id < b.id;
				case 1: return a.name != b.name ? a.name < b.name : a.id < b.id;
				case 2: return a.is_npc != b.is_npc ? a.is_npc < b.is_npc : a.id < b.id;
				case 3: return a.pos != b.pos ? a.pos < b.pos : a.id < b.id;
				case 4: return a.GetTypeString() != b.GetTypeString() ? a.GetTypeString() < b.GetTypeString() : a.id < b.id;
				case 5: return a.description != b.description ? a.description < b.description : a.id < b.id;
				default: return a.id < b.id;
			}
		};
		std::sort(current_issues.begin(), current_issues.end(), [&](const auto& a, const auto& b) {
			return asc ? less(a, b, col) : less(b, a, col);
		});
		RefreshTable();
	});

	results_box->Add(issues_table, 1, wxEXPAND | wxALL, 5);
	rightSizer->Add(results_box, 1, wxEXPAND);
	rightPanel->SetSizer(rightSizer);

	rootSizer->Add(rightPanel, 1, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 10);

	SetSizer(rootSizer);
	Layout();

	UpdateButtonStates();
}

void SpawnValidatorWindow::OnScanMap(wxCommandEvent&) {
	if (!editor) {
		return;
	}

	EditorOperations::SpawnValidationSettings settings;
	settings.check_invalid_spawntime = chk_invalid_time->GetValue();
	settings.check_unknown_creature = chk_unknown_creature->GetValue();
	settings.check_blocked_tiles = chk_blocked_tile->GetValue();
	settings.check_missing_or_empty_tiles = chk_empty_tile->GetValue();
	settings.check_unwalkable_zone = chk_unwalkable_zone->GetValue();

	last_settings = settings;
	has_scanned = true;
	current_issues = EditorOperations::SpawnValidator::ScanMap(editor->map, settings);
	selected_issue_id = -1;
	RefreshTable();
}

void SpawnValidatorWindow::RescanMap() {
	if (!editor || !last_settings) {
		return;
	}
	current_issues = EditorOperations::SpawnValidator::ScanMap(editor->map, *last_settings);
	selected_issue_id = -1;
	RefreshTable();
}

void SpawnValidatorWindow::OnSearchFilterChanged(wxCommandEvent&) {
	RefreshTable();
}

void SpawnValidatorWindow::OnKindFilterChanged(wxCommandEvent&) {
	RefreshTable();
}

std::vector<const EditorOperations::SpawnIssue*> SpawnValidatorWindow::GetFilteredIssues() const {
	std::vector<const EditorOperations::SpawnIssue*> filtered;
	const int kindFilter = radio_kind_filter ? radio_kind_filter->GetSelection() : 0;
	const std::string search = search_ctrl ? as_lower_str(search_ctrl->GetValue().ToStdString()) : "";

	for (const auto& issue : current_issues) {
		if (kindFilter == 1 && issue.is_npc) {
			continue;
		}
		if (kindFilter == 2 && !issue.is_npc) {
			continue;
		}

		if (!search.empty()) {
			const std::string nameLower = as_lower_str(issue.name);
			const std::string posStr = std::format("{},{},{}", issue.pos.x, issue.pos.y, issue.pos.z);
			const std::string typeLower = as_lower_str(std::string(issue.GetTypeString()));
			if (nameLower.find(search) == std::string::npos &&
			    posStr.find(search) == std::string::npos &&
			    typeLower.find(search) == std::string::npos) {
				continue;
			}
		}

		filtered.push_back(&issue);
	}
	return filtered;
}

void SpawnValidatorWindow::RefreshTable() {
	const auto filtered = GetFilteredIssues();

	std::vector<TableRow> rows;
	rows.reserve(filtered.size());

	for (const auto* issue : filtered) {
		TableRow row;
		row.id = issue->id;
		row.key = std::to_string(issue->id);
		row.cells = {
			std::to_string(issue->id),
			issue->name,
			std::string(issue->GetKindString()),
			std::format("{}, {}, {}", issue->pos.x, issue->pos.y, issue->pos.z),
			std::string(issue->GetTypeString()),
			issue->description
		};
		rows.push_back(std::move(row));
	}

	issues_table->SetRows(std::move(rows));

	if (status_label) {
		if (!has_scanned) {
			status_label->SetLabel("Ready. Configure rules and click 'Scan Map'.");
		} else if (current_issues.empty()) {
			status_label->SetLabel("Scan complete: No issues found.");
		} else {
			status_label->SetLabel(wxString::Format("Found %zu issues (%zu shown).", current_issues.size(), filtered.size()));
		}
	}

	UpdateButtonStates();
}

void SpawnValidatorWindow::UpdateButtonStates() {
	const bool hasSelection = (selected_issue_id >= 0);
	const auto filtered = GetFilteredIssues();

	if (jump_button) {
		jump_button->Enable(hasSelection);
	}
	if (edit_button) {
		edit_button->Enable(hasSelection);
	}
	if (remove_button) {
		remove_button->Enable(hasSelection);
	}
	if (remove_all_button) {
		remove_all_button->Enable(!filtered.empty());
	}
}

void SpawnValidatorWindow::OnJumpToSelected() {
	if (selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end()) {
		return;
	}

	g_gui.SetScreenCenterPosition(it->pos);
}

void SpawnValidatorWindow::OnEditSelected() {
	if (!editor || selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end()) {
		return;
	}

	if (it->type == EditorOperations::SpawnIssueType::UnwalkableZone) {
		Tile* centerTile = editor->map.getTile(it->spawn_center);
		if (!centerTile || !centerTile->spawn) {
			return;
		}

		std::unique_ptr<Tile> newTile = TileOperations::deepCopy(centerTile, editor->map);
		SpawnPropertiesWindow dlg(this, &editor->map, newTile.get(), newTile->spawn.get());
		if (dlg.ShowModal() != 0) {
			auto action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(newTile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
			RescanMap();
		}
		return;
	}

	Tile* tile = editor->map.getTile(it->pos);
	if (!tile || !tile->creature) {
		return;
	}

	std::unique_ptr<Tile> newTile = TileOperations::deepCopy(tile, editor->map);
	if (it->is_alternative) {
		auto& alts = newTile->creature->getAlternatives();
		if (it->alternative_index >= alts.size()) {
			return;
		}
		auto& alt = alts[it->alternative_index];
		Creature tempCreature(alt.name);
		tempCreature.setSpawnTime(alt.spawntime);
		tempCreature.setDirection(alt.direction);

		CreaturePropertiesWindow dlg(this, &editor->map, newTile.get(), &tempCreature);
		if (dlg.ShowModal() != 0) {
			alt.spawntime = tempCreature.getSpawnTime();
			alt.direction = tempCreature.getDirection();

			auto action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(newTile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
			RescanMap();
		}
	} else {
		CreaturePropertiesWindow dlg(this, &editor->map, newTile.get(), newTile->creature.get());
		if (dlg.ShowModal() != 0) {
			auto action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(newTile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
			RescanMap();
		}
	}
}

void SpawnValidatorWindow::OnRemoveSelected() {
	if (!editor || selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end()) {
		return;
	}

	if (EditorOperations::SpawnValidator::RemoveIssue(*editor, *it)) {
		selected_issue_id = -1;
		g_gui.RefreshView();
		RescanMap();
	}
}

void SpawnValidatorWindow::OnRemoveAll() {
	if (!editor) {
		return;
	}

	const auto filtered = GetFilteredIssues();
	if (filtered.empty()) {
		return;
	}

	const int answer = wxMessageBox(
		wxString::Format("Are you sure you want to remove all %zu listed broken spawn entries?\n\nThis action can be undone with Ctrl+Z.", filtered.size()),
		"Confirm Remove All",
		wxYES_NO | wxICON_WARNING,
		this
	);
	if (answer != wxYES) {
		return;
	}

	std::vector<EditorOperations::SpawnIssue> toRemove;
	toRemove.reserve(filtered.size());
	for (const auto* iss : filtered) {
		toRemove.push_back(*iss);
	}

	const size_t removedCount = EditorOperations::SpawnValidator::RemoveAllIssues(*editor, toRemove);
	if (removedCount > 0) {
		selected_issue_id = -1;
		g_gui.RefreshView();
		RescanMap();
		g_gui.SetStatusText(wxString::Format("Removed %zu broken spawn entries.", removedCount));
	}
}
