#include "ui/map/map_properties_window.h"

#include "editor/editor.h"
#include "map/map.h"
#include "editor/operations/map_version_changer.h"
#include "ui/gui.h"
#include "app/managers/version_manager.h"
#include "ui/dialog_util.h"
#include "ui/map_tab.h"
#include "util/image_manager.h"

namespace {
	MapVersionID indexToMapVersion(int selection) noexcept {
		switch (selection) {
			case 0: return MAP_OTBM_1;
			case 1: return MAP_OTBM_2;
			case 2: return MAP_OTBM_3;
			case 3: return MAP_OTBM_4;
			case 4: return MAP_OTBM_5;
			default: return MAP_OTBM_UNKNOWN;
		}
	}

	int mapVersionToIndex(MapVersionID ver) noexcept {
		switch (ver) {
			case MAP_OTBM_1: return 0;
			case MAP_OTBM_2: return 1;
			case MAP_OTBM_3: return 2;
			case MAP_OTBM_4: return 3;
			case MAP_OTBM_5: return 4;
			default: return 0;
		}
	}
}

MapPropertiesWindow::MapPropertiesWindow(wxWindow* parent, MapTab* view, Editor& editor) :
	wxDialog(parent, wxID_ANY, "Map Properties", wxDefaultPosition, FROM_DIP(parent, wxSize(300, 200)), wxRESIZE_BORDER | wxCAPTION),
	view(view),
	editor(editor) {
	// Setup data variabels
	Map& map = editor.map;

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer* grid_sizer = newd wxFlexGridSizer(2, 10, 10);
	grid_sizer->AddGrowableCol(1);

	// Description
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Description"));
	description_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getMapDescription()), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	description_ctrl->SetToolTip("Enter a description for the map");
	grid_sizer->Add(description_ctrl, wxSizerFlags(1).Expand());

	// Map version
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Version"));
	version_choice = newd wxChoice(this, MAP_PROPERTIES_VERSION);
	version_choice->SetToolTip("Select the OTBM version (Determines feature support)");
	version_choice->Append("OTBM 1");
	version_choice->Append("OTBM 2");
	version_choice->Append("OTBM 3 (Standard TFS)");
	version_choice->Append("OTBM 4 (Canary)");
	version_choice->Append("OTBM 5 (CrystalServer)");

	version_choice->SetSelection(mapVersionToIndex(map.getVersion().otbm));

	grid_sizer->Add(version_choice, wxSizerFlags(1).Expand());

	// Version
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Client Version"));
	protocol_choice = newd wxChoice(this, wxID_ANY);
	protocol_choice->SetToolTip("Select the target client version");

	protocol_choice->SetStringSelection(wxstr(g_version.GetCurrentVersion().getName()));

	grid_sizer->Add(protocol_choice, wxSizerFlags(1).Expand());

	// Compression
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Compression"));
	compression_choice = newd wxChoice(this, wxID_ANY);
	compression_choice->SetToolTip("Select the map storage compression format");
	compression_choice->Append("None (Standard / The Forgotten Server)");
	compression_choice->Append("GZIP (Crystal Server)");
	compression_choice->SetSelection(map.getCompression() == OtbmCompression::Gzip ? 1 : 0);
	grid_sizer->Add(compression_choice, wxSizerFlags(1).Expand());

	// Dimensions
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Dimensions"));
	{
		wxSizer* subsizer = newd wxBoxSizer(wxHORIZONTAL);
		subsizer->Add(
			width_spin = newd wxSpinCtrl(this, wxID_ANY, wxstr(i2s(map.getWidth())), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 256, MAP_MAX_WIDTH), wxSizerFlags(1).Expand()
		);
		width_spin->SetToolTip("Map width in tiles");
		subsizer->Add(
			height_spin = newd wxSpinCtrl(this, wxID_ANY, wxstr(i2s(map.getHeight())), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 256, MAP_MAX_HEIGHT), wxSizerFlags(1).Expand()
		);
		height_spin->SetToolTip("Map height in tiles");
		grid_sizer->Add(subsizer, 1, wxEXPAND);
	}

	// External files
	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External Housefile")
	);

	grid_sizer->Add(
		house_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getHouseFilename())), 1, wxEXPAND
	);
	house_filename_ctrl->SetToolTip("External house XML file (leave empty for internal)");

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External Spawnfile")
	);

	grid_sizer->Add(
		spawn_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getSpawnFilename())), 1, wxEXPAND
	);
	spawn_filename_ctrl->SetToolTip("External spawn XML file (leave empty for internal)");

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External Waypointfile")
	);

	grid_sizer->Add(
		waypoint_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getWaypointFilename())), 1, wxEXPAND
	);
	waypoint_filename_ctrl->SetToolTip("External waypoint XML file (leave empty for internal)");

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External Zonefile")
	);

	grid_sizer->Add(
		zone_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getZoneFilename())), 1, wxEXPAND
	);
	zone_filename_ctrl->SetToolTip("External zone XML file, OTBM 5 (Crystal Server) maps only");

	topsizer->Add(grid_sizer, wxSizerFlags(1).Expand().Border(wxALL, 20));

	wxSizer* subsizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	okBtn->SetToolTip("Confirm changes");
	subsizer->Add(okBtn, wxSizerFlags(1).Center());

	wxButton* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	cancelBtn->SetToolTip("Discard changes");
	subsizer->Add(cancelBtn, wxSizerFlags(1).Center());
	topsizer->Add(subsizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT | wxBOTTOM, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);
	UpdateProtocolList();

	const ClientVersion* current_version = ClientVersion::getByItemsVersion(map.getVersion().items_major, map.getVersion().client);
	if (!current_version) {
		current_version = ClientVersion::getBestMatch(map.getVersion().client);
	}
	if (!current_version) {
		current_version = &g_version.GetCurrentVersion();
	}
	if (current_version) {
		protocol_choice->SetStringSelection(wxstr(current_version->getName()));
	}

	version_choice->Bind(wxEVT_CHOICE, &MapPropertiesWindow::OnChangeVersion, this);
	okBtn->Bind(wxEVT_BUTTON, &MapPropertiesWindow::OnClickOK, this);
	cancelBtn->Bind(wxEVT_BUTTON, &MapPropertiesWindow::OnClickCancel, this);

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_GEAR));
}

void MapPropertiesWindow::UpdateProtocolList() {
	wxString client = protocol_choice->GetStringSelection();

	protocol_choice->Clear();

	ClientVersionList protocols;
	if (g_settings.getInteger(Config::USE_OTBM_4_FOR_ALL_MAPS)) {
		protocols = ClientVersion::getAllVisible();
	} else {
		MapVersionID map_version = indexToMapVersion(version_choice->GetSelection());
		if (map_version == MAP_OTBM_UNKNOWN) {
			map_version = MAP_OTBM_1;
		}

		if (map_version >= MAP_OTBM_4) {
			protocols = ClientVersion::getAllForOTBMVersion(MAP_OTBM_3);
			for (auto* c : ClientVersion::getAllForOTBMVersion(MAP_OTBM_4)) {
				if (std::find(protocols.begin(), protocols.end(), c) == protocols.end()) {
					protocols.push_back(c);
				}
			}
			for (auto* c : ClientVersion::getAllForOTBMVersion(MAP_OTBM_5)) {
				if (std::find(protocols.begin(), protocols.end(), c) == protocols.end()) {
					protocols.push_back(c);
				}
			}
		} else {
			protocols = ClientVersion::getAllForOTBMVersion(map_version);
		}
	}

	for (ClientVersionList::const_iterator p = protocols.begin(); p != protocols.end(); ++p) {
		protocol_choice->Append(wxstr((*p)->getName()));
	}
	protocol_choice->SetSelection(0);
	protocol_choice->SetStringSelection(client);
}

void MapPropertiesWindow::OnChangeVersion(wxCommandEvent&) {
	UpdateProtocolList();
}

void MapPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	Map& map = editor.map;

	MapVersion old_ver = map.getVersion();
	MapVersion new_ver;

	ClientVersion* selected_client = ClientVersion::get(nstr(protocol_choice->GetStringSelection()));
	if (selected_client) {
		new_ver.client = selected_client->getProtocolID();
		new_ver.items_major = selected_client->getOTBVersion().format_version;
	} else {
		new_ver.client = old_ver.client;
		new_ver.items_major = old_ver.items_major;
	}
	new_ver.otbm = indexToMapVersion(version_choice->GetSelection());
	if (new_ver.otbm == MAP_OTBM_UNKNOWN) {
		new_ver.otbm = MAP_OTBM_1;
	}

	if (!MapVersionChanger::changeMapVersion(this, editor, new_ver)) {
		return;
	}

	map.setMapDescription(nstr(description_ctrl->GetValue()));
	map.setHouseFilename(nstr(house_filename_ctrl->GetValue()));
	map.setSpawnFilename(nstr(spawn_filename_ctrl->GetValue()));
	map.setWaypointFilename(nstr(waypoint_filename_ctrl->GetValue()));
	map.setZoneFilename(nstr(zone_filename_ctrl->GetValue()));

	const auto new_compression = (compression_choice->GetSelection() == 1) ? OtbmCompression::Gzip : OtbmCompression::None;
	if (new_compression != map.getCompression()) {
		map.setCompression(new_compression);
		map.doChange();
	}

	// Only resize if we have to
	int new_map_width = width_spin->GetValue();
	int new_map_height = height_spin->GetValue();
	if (new_map_width != map.getWidth() || new_map_height != map.getHeight()) {
		map.setWidth(new_map_width);
		map.setHeight(new_map_height);
		g_gui.FitViewToMap(view);
	}
	g_gui.RefreshPalettes();

	EndModal(1);
}

void MapPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	// Just close this window
	EndModal(1);
}

MapPropertiesWindow::~MapPropertiesWindow() = default;
