//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/map/sector_map_properties_window.h"

#include "app/managers/version_manager.h"
#include "io/iomap_sec.h"
#include "map/map.h"
#include "ui/theme.h"
#include "util/file_system.h"
#include "util/image_manager.h"

#include <array>
#include <filesystem>
#include <format>
#include <string_view>

namespace {

	struct ServerFile {
		std::string_view path; // relative to the server data folder
		std::string_view holds;
		std::string_view when_missing;
	};

	// The files around a sector map folder, as io/sec/data/ reads them.
	constexpr std::array<ServerFile, 8> kServerFiles { {
		{ "dat/objects.srv", "item types", "copied there on save" },
		{ "dat/map.dat", "towns, waypoints, start positions, sector range", "written on save" },
		{ "dat/monster.db", "monster homes", "written once the map has monsters" },
		{ "dat/houses.dat", "houses", "written once the map has houses" },
		{ "dat/houseareas.dat", "house areas", "written once the map has houses" },
		{ "dat/moveuse.dat", "script markers (read only)", "no script markers" },
		{ "mon/", "monster races (*.mon) and raids (*.evt)", "no monster races or raids" },
		{ "npc/", "NPCs and their homes (*.npc)", "no NPCs" },
	} };

} // namespace

SectorMapPropertiesWindow::SectorMapPropertiesWindow(wxWindow* parent, const Map& map) :
	wxDialog(parent, wxID_ANY, "Map Properties", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
	auto* top_sizer = new wxBoxSizer(wxVERTICAL);
	auto* grid_sizer = new wxFlexGridSizer(2, Theme::Grid(2), Theme::Grid(3));
	grid_sizer->AddGrowableCol(1);

	const auto add_row = [this, grid_sizer](std::string_view label, const std::string& value, std::string_view tooltip = {}) {
		grid_sizer->Add(new wxStaticText(this, wxID_ANY, wxstr(label)));
		auto* text = new wxStaticText(this, wxID_ANY, wxstr(value));
		if (!tooltip.empty()) {
			text->SetToolTip(wxstr(tooltip));
		}
		grid_sizer->Add(text, wxSizerFlags().Expand());
	};

	const std::string file_name = map.getFilename();
	const bool saved = !file_name.empty();
	const std::filesystem::path sector_folder = saved ? IOMapSec::sectorFolder(wxstr(file_name)) : std::filesystem::path();

	add_row("Map Format", "Sector files (XXXX-YYYY-ZZ.sec, 32x32 tiles each)");
	add_row("Client Version", g_version.IsVersionLoaded() ? g_version.GetCurrentVersion().getName() : "None");
	add_row("Map Folder", saved ? FileSystem::PathToUtf8(sector_folder) : "Not saved yet");
	add_row("Tiles", std::format("{}", map.getTileCount()));
	add_row(
		"Map Dimensions", std::format("{} x {}", map.getWidth(), map.getHeight()),
		"Up to the furthest tile. Positions are the server's game coordinates, the same ones map.dat, monster.db and the NPC files use, so the map does not start at 0,0."
	);
	top_sizer->Add(grid_sizer, wxSizerFlags().Expand().Border(wxALL, Theme::Grid(4)));

	auto* files_box = new wxStaticBoxSizer(wxVERTICAL, this, "Server Files");
	auto* files_grid = new wxFlexGridSizer(3, Theme::Grid(1), Theme::Grid(3));
	files_grid->AddGrowableCol(2);
	const std::filesystem::path data_folder = sector_folder.parent_path();
	for (const ServerFile& file : kServerFiles) {
		std::error_code ec;
		const bool exists = saved && std::filesystem::exists(data_folder / std::filesystem::path(file.path), ec);
		wxWindow* box = files_box->GetStaticBox();
		files_grid->Add(new wxStaticText(box, wxID_ANY, wxstr(file.path)));
		auto* status = new wxStaticText(box, wxID_ANY, wxstr(exists ? std::string_view("found") : file.when_missing));
		if (!exists) {
			status->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
		}
		files_grid->Add(status);
		files_grid->Add(new wxStaticText(box, wxID_ANY, wxstr(file.holds)));
	}
	files_box->Add(files_grid, wxSizerFlags().Expand().Border(wxALL, Theme::Grid(2)));
	top_sizer->Add(files_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, Theme::Grid(4)));

	auto* close_button = new wxButton(this, wxID_OK, "Close");
	close_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	close_button->SetDefault();
	top_sizer->Add(close_button, wxSizerFlags().Right().Border(wxLEFT | wxRIGHT | wxBOTTOM, Theme::Grid(4)));

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);
	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_GEAR));
}
