#include "app/preferences/editor_page.h"

#include "app/main.h"
#include "app/preferences/preferences_layout.h"
#include "app/settings.h"
#include "rendering/core/floor_visibility_mode.h"
#include "ui/gui.h"

EditorPage::EditorPage(wxWindow* parent) : ScrollablePreferencesPage(parent) {
	auto* page_sizer = GetPageSizer();

	auto* modes_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Editor Modes",
		"Choose how editor-wide modes behave while mapping."
	);
	floor_visibility_mode_choice = new wxChoice(modes_section, wxID_ANY);
	floor_visibility_mode_choice->Append("Client visible");
	floor_visibility_mode_choice->Append("All visible");
	floor_visibility_mode_choice->SetSelection(static_cast<int>(SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE))));
	PreferencesLayout::AddControlRow(
		modes_section,
		"Show all floors mode",
		"Choose whether Show all floors uses Tibia client visibility constraints or renders every floor below the active floor.",
		floor_visibility_mode_choice
	);
	page_sizer->Add(modes_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	auto* history_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Action History",
		"These options control how editing operations are grouped and reviewed in the undo system."
	);
	group_actions_chkbox = PreferencesLayout::AddCheckBoxRow(
		history_section,
		"Group same-type actions",
		"Combine consecutive drawing or selection edits into larger undo steps when they happen back to back.",
		g_settings.getBoolean(Config::GROUP_ACTIONS)
	);
	page_sizer->Add(history_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	auto* brush_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Brush Behavior",
		"Make common brushes behave more predictably for large editing passes."
	);
	house_remove_chkbox = PreferencesLayout::AddCheckBoxRow(
		brush_section,
		"House brush removes respawned items",
		"When painting houses, remove items that would reappear every time the map is loaded.",
		g_settings.getBoolean(Config::HOUSE_BRUSH_REMOVE_ITEMS)
	);
	auto_assign_doors_chkbox = PreferencesLayout::AddCheckBoxRow(
		brush_section,
		"Auto-assign door ids",
		"Give placed house doors unique ids automatically. RAW palette placement remains unaffected.",
		g_settings.getBoolean(Config::AUTO_ASSIGN_DOORID)
	);
	doodad_erase_same_chkbox = PreferencesLayout::AddCheckBoxRow(
		brush_section,
		"Doodad brush only erases matching items",
		"Limit doodad erase operations to items that belong to the currently selected doodad brush.",
		g_settings.getBoolean(Config::DOODAD_BRUSH_ERASE_LIKE)
	);
	eraser_leave_unique_chkbox = PreferencesLayout::AddCheckBoxRow(
		brush_section,
		"Eraser leaves unique items",
		"Preserve containers with contents and items carrying unique or action ids during erase passes.",
		g_settings.getBoolean(Config::ERASER_LEAVE_UNIQUE)
	);
	auto_create_spawn_chkbox = PreferencesLayout::AddCheckBoxRow(
		brush_section,
		"Auto-create spawn when placing creatures",
		"Place the required spawn automatically so creature placement works without a separate prep step.",
		g_settings.getBoolean(Config::AUTO_CREATE_SPAWN)
	);
	page_sizer->Add(brush_section, 0, wxEXPAND | wxALL, FromDIP(10));

	auto* safety_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Placement Safety",
		"Warn or prevent mistakes that are easy to miss while painting quickly."
	);
	duplicate_id_warn_chkbox = PreferencesLayout::AddCheckBoxRow(
		safety_section,
		"Warn about duplicate ids",
		"Show warnings when the editor detects duplicate ids that could break houses, quests, or actions.",
		g_settings.getBoolean(Config::WARN_FOR_DUPLICATE_ID)
	);
	missing_items_warn_chkbox = PreferencesLayout::AddCheckBoxRow(
		safety_section,
		"Show missing items warning on load",
		"Display a summary of missing item definitions after loading a map. Items are always tracked and available via File -> Missing Items Report...",
		g_settings.getBoolean(Config::SHOW_MISSING_ITEMS_WARNING)
	);
	allow_multiple_orderitems_chkbox = PreferencesLayout::AddCheckBoxRow(
		safety_section,
		"Prevent toporder conflicts in RAW placement",
		"Block placing multiple RAW items that would occupy the same toporder slot on one tile.",
		g_settings.getBoolean(Config::RAW_LIKE_SIMONE)
	);
	page_sizer->Add(safety_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* merge_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Move and Paste",
		"Choose whether large transforms overwrite target tiles or merge on top of existing content."
	);
	merge_move_chkbox = PreferencesLayout::AddCheckBoxRow(
		merge_section,
		"Use merge move",
		"Moved tiles do not replace already occupied destination tiles.",
		g_settings.getBoolean(Config::MERGE_MOVE)
	);
	merge_paste_chkbox = PreferencesLayout::AddCheckBoxRow(
		merge_section,
		"Use merge paste",
		"Pasted tiles do not replace already occupied destination tiles.",
		g_settings.getBoolean(Config::MERGE_PASTE)
	);
	page_sizer->Add(merge_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* load_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Map Load",
		"Choose the item catalog and spawn placement used when a map opens."
	);
	use_items_ron_chkbox = PreferencesLayout::AddCheckBoxRow(
		load_section,
		"Use items.ron",
		"Load the merged items.ron catalog. Leave this off to load items.xml and items.otb.",
		g_settings.getBoolean(Config::USE_ITEMS_RON)
	);
	spread_randomized_spawns_chkbox = PreferencesLayout::AddCheckBoxRow(
		load_section,
		"Randomized spawns",
		"Place creatures on free tiles inside the spawn radius. Leave this off to keep the spawn file offsets. Saving still writes those offsets.",
		g_settings.getBoolean(Config::SPREAD_RANDOMIZED_SPAWNS)
	);
	page_sizer->Add(load_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	FinishLayout();
}

void EditorPage::Apply() {
	const int previous_floor_visibility_mode = g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE);
	const int selected_floor_visibility_mode = floor_visibility_mode_choice ? floor_visibility_mode_choice->GetSelection() : 0;
	g_settings.setInteger(Config::FLOOR_VISIBILITY_MODE, static_cast<int>(SanitizeFloorVisibilityMode(selected_floor_visibility_mode)));
	g_settings.setInteger(Config::GROUP_ACTIONS, group_actions_chkbox->GetValue());
	g_settings.setInteger(Config::WARN_FOR_DUPLICATE_ID, duplicate_id_warn_chkbox->GetValue());
	g_settings.setInteger(Config::SHOW_MISSING_ITEMS_WARNING, missing_items_warn_chkbox->GetValue());
	g_settings.setInteger(Config::HOUSE_BRUSH_REMOVE_ITEMS, house_remove_chkbox->GetValue());
	g_settings.setInteger(Config::AUTO_ASSIGN_DOORID, auto_assign_doors_chkbox->GetValue());
	g_settings.setInteger(Config::ERASER_LEAVE_UNIQUE, eraser_leave_unique_chkbox->GetValue());
	g_settings.setInteger(Config::DOODAD_BRUSH_ERASE_LIKE, doodad_erase_same_chkbox->GetValue());
	g_settings.setInteger(Config::AUTO_CREATE_SPAWN, auto_create_spawn_chkbox->GetValue());
	g_settings.setInteger(Config::RAW_LIKE_SIMONE, allow_multiple_orderitems_chkbox->GetValue());
	g_settings.setInteger(Config::MERGE_MOVE, merge_move_chkbox->GetValue());
	g_settings.setInteger(Config::MERGE_PASTE, merge_paste_chkbox->GetValue());
	g_settings.setInteger(Config::USE_ITEMS_RON, use_items_ron_chkbox->GetValue());
	g_settings.setInteger(Config::SPREAD_RANDOMIZED_SPAWNS, spread_randomized_spawns_chkbox->GetValue());

	if (previous_floor_visibility_mode != g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE)) {
		g_gui.RefreshView();
		g_gui.UpdateMinimap(true);
	}
}
