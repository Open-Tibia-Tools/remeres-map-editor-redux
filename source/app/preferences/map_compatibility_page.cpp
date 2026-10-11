#include "app/preferences/map_compatibility_page.h"

#include "app/main.h"
#include "app/preferences/preferences_layout.h"
#include "app/settings.h"
#include "ui/gui.h"

MapCompatibilityPage::MapCompatibilityPage(wxWindow* parent) : ScrollablePreferencesPage(parent) {
	auto* page_sizer = GetPageSizer();

	// Section 1: Waypoints
	auto* waypoints_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Waypoints",
		"Configure where map waypoints are stored and how cross-server formats are handled."
	);

	waypoint_storage_choice = new wxChoice(waypoints_section, wxID_ANY);
	waypoint_storage_choice->Append("In OTBM (Recommended / Standard)");
	waypoint_storage_choice->Append("External XML (<map>-waypoint.xml)");
	waypoint_storage_choice->Append("Both (OTBM and XML)");

	int current_format = g_settings.getInteger(Config::WAYPOINT_STORAGE_FORMAT);
	if (current_format < 0 || current_format > 2) {
		current_format = static_cast<int>(Config::WaypointStorageFormat::OTBM);
	}
	waypoint_storage_choice->SetSelection(current_format);

	PreferencesLayout::AddControlRow(
		waypoints_section,
		"Waypoint storage location",
		"Choose where waypoints are persisted. OTBM is the native format expected by TFS, Canary, and Crystal Server.",
		waypoint_storage_choice
	);

	PreferencesLayout::AddNotice(
		waypoints_section,
		"When opening a map, waypoints present in OTBM and/or external XML files will automatically load and merge without prompts. On save, waypoints will be migrated to your chosen format."
	);

	page_sizer->Add(waypoints_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	// Section 2: Zones
	auto* zones_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Zones (Placeholder)",
		"Placeholder"
	);
	page_sizer->Add(zones_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	// Section 3: Spawns & NPCs
	auto* spawns_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Spawns & NPCs (Placeholder)",
		"Placeholder"
	);
	page_sizer->Add(spawns_section, 0, wxEXPAND | wxALL, FromDIP(10));
}

void MapCompatibilityPage::Apply() {
	if (waypoint_storage_choice) {
		g_settings.setInteger(Config::WAYPOINT_STORAGE_FORMAT, waypoint_storage_choice->GetSelection());
	}
}
