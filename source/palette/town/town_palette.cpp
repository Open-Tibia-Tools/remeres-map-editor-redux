//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/town/town_palette.h"
#include "palette/town/edit_town_dialog.h"
#include "brushes/town/town_temple_brush.h"
#include "brushes/managers/brush_manager.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/town.h"
#include "game/house.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/theme.h"
#include "ui/dialog_util.h"
#include "ui/map_tab.h"
#include "ui/managers/minimap_manager.h"
#include "util/image_manager.h"

#include <wx/evtloop.h>
#include <wx/msgdlg.h>
#include <algorithm>
#include <unordered_map>
#include <format>
#include <vector>

TownPalettePanel::TownPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	map(nullptr) {
	auto* sidesizer = newd wxBoxSizer(wxVERTICAL);

	// Search control
	search_ctrl = newd PaletteSearchCtrl(this, PALETTE_TOWN_SEARCH, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Search towns...");
	search_ctrl->SetFont(Theme::GetFont(9, false));
	sidesizer->Add(search_ctrl, 0, wxEXPAND | wxALL, 5);

	// Table control (ID, Name, Temple, Houses)
	town_table = newd NanoVGTableCtrl(this, PALETTE_TOWN_LISTBOX);
	std::vector<TableColumn> cols = {
		{ "id", "ID", 45, NVG_ALIGN_RIGHT, true },
		{ "name", "Name", 0, NVG_ALIGN_LEFT, true },
		{ "temple", "Temple", 135, NVG_ALIGN_LEFT, true },
		{ "houses", "Houses", 50, NVG_ALIGN_RIGHT, true }
	};
	town_table->SetColumns(std::move(cols));

	town_table->SetOnSelect([this](int idx, const TableRow& row) {
		OnSelectTown(idx, row);
	});
	town_table->SetOnActivate([this](int idx, const TableRow& row) {
		OnActivateTown(idx, row);
	});
	town_table->SetOnSort([this](int col, bool asc) {
		OnSortTowns(col, asc);
	});
	town_table->SetOnContextMenu([this](int idx, const TableRow* row, const wxPoint& pos) {
		OnTownContextMenu(idx, row, pos);
	});

	sidesizer->Add(town_table, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Buttons
	auto* tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	add_town_button = newd wxButton(this, PALETTE_TOWN_ADD, "Add", wxDefaultPosition, wxSize(45, -1));
	add_town_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	add_town_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(add_town_button, 1, wxEXPAND | wxRIGHT, 2);

	remove_town_button = newd wxButton(this, PALETTE_TOWN_REMOVE, "Remove", wxDefaultPosition, wxSize(55, -1));
	remove_town_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	remove_town_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(remove_town_button, 1, wxEXPAND | wxRIGHT, 2);

	edit_town_button = newd wxButton(this, PALETTE_TOWN_EDIT, "Edit", wxDefaultPosition, wxSize(45, -1));
	edit_town_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	edit_town_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(edit_town_button, 1, wxEXPAND | wxRIGHT, 2);

	change_temple_button = newd wxButton(this, PALETTE_TOWN_CHANGE_TEMPLE, "Set Temple", wxDefaultPosition, wxSize(70, -1));
	change_temple_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CROSSHAIRS, wxSize(16, 16)));
	change_temple_button->SetFont(Theme::GetFont(9, false));
	change_temple_button->SetToolTip("Click to place temple position on the map");
	tmpsizer->Add(change_temple_button, 1, wxEXPAND);

	sidesizer->Add(tmpsizer, 0, wxEXPAND | wxALL, 5);
	SetSizer(sidesizer);

	Bind(wxEVT_TEXT, &TownPalettePanel::OnSearchChange, this, PALETTE_TOWN_SEARCH);

	Bind(wxEVT_BUTTON, &TownPalettePanel::OnClickAddTown, this, PALETTE_TOWN_ADD);
	Bind(wxEVT_BUTTON, &TownPalettePanel::OnClickEditTown, this, PALETTE_TOWN_EDIT);
	Bind(wxEVT_BUTTON, &TownPalettePanel::OnClickRemoveTown, this, PALETTE_TOWN_REMOVE);
	Bind(wxEVT_BUTTON, &TownPalettePanel::OnClickChangeTemple, this, PALETTE_TOWN_CHANGE_TEMPLE);

	Bind(wxEVT_MENU, &TownPalettePanel::OnClickAddTown, this, PALETTE_TOWN_ADD);
	Bind(wxEVT_MENU, &TownPalettePanel::OnClickEditTown, this, PALETTE_TOWN_EDIT);
	Bind(wxEVT_MENU, &TownPalettePanel::OnClickRemoveTown, this, PALETTE_TOWN_REMOVE);
	Bind(wxEVT_MENU, &TownPalettePanel::OnClickChangeTemple, this, PALETTE_TOWN_CHANGE_TEMPLE);
	Bind(wxEVT_MENU, &TownPalettePanel::OnGoToTemple, this, PALETTE_TOWN_GOTO_TEMPLE);
}

void TownPalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
}

void TownPalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void TownPalettePanel::SetMap(Map* m) {
	if (map != m) {
		selected_town_id_ = 0;
		if (g_brush_manager.temple_brush) {
			g_brush_manager.temple_brush->setTown(nullptr);
		}
	}
	map = m;
	OnUpdate();
}

void TownPalettePanel::SelectFirstBrush() {
	if (map && !map->towns.empty()) {
		selected_town_id_ = map->towns.begin()->first;
	}
}

Town* TownPalettePanel::GetSelectedTown() const {
	if (!map || selected_town_id_ == 0) {
		return nullptr;
	}
	return map->towns.getTown(selected_town_id_);
}

Brush* TownPalettePanel::GetSelectedBrush() const {
	// Browsing towns is not drawing. Do not auto-select temple brush or crosshair cursor.
	return nullptr;
}

bool TownPalettePanel::SelectBrush(const Brush* whatbrush) {
	return g_brush_manager.temple_brush && whatbrush == g_brush_manager.temple_brush;
}

int TownPalettePanel::GetSelectedBrushSize() const {
	return 0;
}

wxString TownPalettePanel::GetName() const {
	return "Town";
}

void TownPalettePanel::OnUpdate() {
	if (!map) {
		town_table->ClearRows();
		town_table->Enable(false);
		if (search_ctrl) {
			search_ctrl->Enable(false);
		}
		add_town_button->Enable(false);
		edit_town_button->Enable(false);
		remove_town_button->Enable(false);
		change_temple_button->Enable(false);
		return;
	}

	town_table->Enable(true);
	if (search_ctrl) {
		search_ctrl->Enable(true);
	}
	add_town_button->Enable(true);

	const bool has_selection = (GetSelectedTown() != nullptr);
	edit_town_button->Enable(has_selection);
	remove_town_button->Enable(has_selection);
	change_temple_button->Enable(has_selection);

	const wxString search_query = search_ctrl ? search_ctrl->GetValue().Lower() : wxString();

	// Precompute house counts per town
	std::unordered_map<uint32_t, size_t> house_counts;
	for (const auto& [_, house] : map->houses) {
		if (house) {
			house_counts[house->townid]++;
		}
	}

	std::vector<TableRow> rows;
	for (const auto& [id, town] : map->towns) {
		if (!town) {
			continue;
		}

		const Position& pos = town->getTemplePosition();
		const std::string pos_str = (pos != Position() && pos.isValid())
			? std::format("{}, {}, {}", pos.x, pos.y, pos.z)
			: "—";

		const size_t h_count = house_counts[id];

		const bool search_match = search_query.IsEmpty() ||
			wxstr(town->getName()).Lower().Contains(search_query) ||
			wxstr(std::to_string(id)).Contains(search_query) ||
			wxstr(pos_str).Lower().Contains(search_query);

		if (search_match) {
			TableRow row;
			row.id = static_cast<int>(id);
			row.key = std::to_string(id);
			row.cells.push_back(std::to_string(id));
			row.cells.push_back(town->getName());
			row.cells.push_back(pos_str);
			row.cells.push_back(std::to_string(h_count));
			row.client_data = town.get();
			rows.push_back(std::move(row));
		}
	}

	const int sort_col = town_table->GetSortColumn();
	const bool asc = town_table->IsSortAscending();

	if (sort_col == 0) {
		// Sort by ID
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			return asc ? (a.id < b.id) : (b.id < a.id);
		});
	} else if (sort_col == 1) {
		// Sort by Name
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			int cmp = wxStricmp(wxstr(a.cells[1]), wxstr(b.cells[1]));
			return asc ? (cmp < 0) : (cmp > 0);
		});
	} else if (sort_col == 2) {
		// Sort by Temple Position
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			auto* tA = static_cast<const Town*>(a.client_data);
			auto* tB = static_cast<const Town*>(b.client_data);
			if (!tA || !tB) return false;
			const Position& posA = tA->getTemplePosition();
			const Position& posB = tB->getTemplePosition();
			if (posA != posB) {
				return asc ? (posA < posB) : (posB < posA);
			}
			return asc ? (tA->getID() < tB->getID()) : (tB->getID() < tA->getID());
		});
	} else if (sort_col == 3) {
		// Sort by House count
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			size_t cA = 0, cB = 0;
			try { cA = std::stoull(a.cells[3]); cB = std::stoull(b.cells[3]); } catch (...) {}
			return asc ? (cA < cB) : (cB < cA);
		});
	}

	town_table->SetRows(std::move(rows));

	if (selected_town_id_ != 0) {
		town_table->SelectById(selected_town_id_);
	}
}

void TownPalettePanel::OnSearchChange(wxCommandEvent& /*event*/) {
	OnUpdate();
}

void TownPalettePanel::OnSelectTown(int /*index*/, const TableRow& row) {
	auto* town = static_cast<Town*>(row.client_data);
	if (town) {
		selected_town_id_ = town->getID();
		edit_town_button->Enable(true);
		remove_town_button->Enable(true);
		change_temple_button->Enable(true);
	}
}

void TownPalettePanel::OnActivateTown(int /*index*/, const TableRow& row) {
	auto* town = static_cast<Town*>(row.client_data);
	if (town && town->getTemplePosition().isValid()) {
		g_gui.SetScreenCenterPosition(town->getTemplePosition());
		g_gui.RefreshView(true);
	}
}

void TownPalettePanel::OnSortTowns(int /*column*/, bool /*ascending*/) {
	OnUpdate();
}

void TownPalettePanel::OnTownContextMenu(int /*index*/, const TableRow* row, const wxPoint& pos) {
	if (!map) {
		return;
	}

	wxMenu menu;
	if (row && row->client_data) {
		auto* town = static_cast<Town*>(row->client_data);
		selected_town_id_ = town->getID();

		auto* goto_item = menu.Append(PALETTE_TOWN_GOTO_TEMPLE, "Go to Temple");
		goto_item->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION_ARROW, wxSize(16, 16)));

		auto* temple_item = menu.Append(PALETTE_TOWN_CHANGE_TEMPLE, "Change Temple Position");
		temple_item->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CROSSHAIRS, wxSize(16, 16)));

		auto* edit_item = menu.Append(PALETTE_TOWN_EDIT, "Edit Town...");
		edit_item->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));

		menu.AppendSeparator();

		auto* remove_item = menu.Append(PALETTE_TOWN_REMOVE, "Remove Town");
		remove_item->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
		menu.AppendSeparator();
	}

	auto* add_item = menu.Append(PALETTE_TOWN_ADD, "Add Town...");
	add_item->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));

	PopupMenu(&menu, pos);
}

void TownPalettePanel::OnGoToTemple(wxCommandEvent& /*event*/) {
	Town* town = GetSelectedTown();
	if (town && town->getTemplePosition().isValid()) {
		g_gui.SetScreenCenterPosition(town->getTemplePosition());
		g_gui.RefreshView(true);
	}
}

bool TownPalettePanel::RunEditTownModal(wxWindow* parent, Map* map, Town* town, bool is_new) {
	EditTownDialog dlg(parent, map, town, is_new);
	const uint32_t saved_town_id = town ? town->getID() : 0;
	while (true) {
		int ret = dlg.ShowModal();
		if (ret == EditTownDialog::DIALOG_PICK_TEMPLE) {
			Position picked_pos;
			bool picked = false;

			if (g_brush_manager.temple_brush) {
				g_brush_manager.temple_brush->setTown(town);

				wxEventLoop loop;
				g_brush_manager.temple_brush->setPlacementCallback([&](const Position& pos) {
					picked_pos = pos;
					picked = true;
					if (loop.IsRunning()) {
						loop.Exit(1);
					}
				});
				g_brush_manager.temple_brush->setCancelCallback([&]() {
					picked = false;
					if (loop.IsRunning()) {
						loop.Exit(0);
					}
				});

				g_gui.SelectBrush(g_brush_manager.temple_brush);
				g_gui.SetStatusText(wxstr(std::format("Click on map to pick temple position for town '{}'... [Esc to cancel]", dlg.GetTownName())));

				loop.Run();

				g_brush_manager.temple_brush->clearCallbacks();
			}

			// Verify map tab and map are still valid
			MapTab* mapTab = g_gui.GetCurrentMapTab();
			if (!mapTab || mapTab->GetMap() != map) {
				return false;
			}

			// If not a new town, re-resolve town using saved ID to ensure it wasn't deleted
			if (!is_new) {
				Town* resolved_town = map->towns.getTown(saved_town_id);
				if (!resolved_town) {
					return false;
				}
				town = resolved_town;
			}

			if (picked && picked_pos.isValid()) {
				dlg.SetTemplePosition(picked_pos);
			}
			continue;
		}
		return ret == wxID_OK;
	}
}

void TownPalettePanel::OnClickAddTown(wxCommandEvent& /*event*/) {
	if (!map) {
		return;
	}

	uint32_t next_id = map->towns.getEmptyID();
	MapTab* mapTab = g_gui.GetCurrentMapTab();
	Position default_pos = mapTab ? mapTab->GetScreenCenterPosition() : Position();
	if (!default_pos.isValid()) {
		default_pos = Position(map->getWidth() / 2, map->getHeight() / 2, 7);
	}

	auto new_town = std::make_unique<Town>(next_id);
	new_town->setName(std::format("Town {}", next_id));
	new_town->setTemplePosition(default_pos);

	Town* town_ptr = new_town.get();
	if (RunEditTownModal(this, map, town_ptr, true)) {
		Position temple = new_town->getTemplePosition();
		if (auto* tile = map->getOrCreateTile(temple)) {
			if (auto* loc = tile->getLocation()) {
				loc->increaseTownCount();
			}
		}
		if (temple.isValid()) {
			map->getChangeTracker().markTileDirty(temple);
			g_minimap.MarkTileDirty(*map, temple);
		}
		selected_town_id_ = new_town->getID();
		map->towns.addTown(std::move(new_town));
		map->doChange();
		g_gui.RefreshPalettes();
		g_gui.RefreshView();
	}
}

void TownPalettePanel::OnClickEditTown(wxCommandEvent& /*event*/) {
	Town* town = GetSelectedTown();
	if (!town || !map) {
		return;
	}

	const uint32_t old_id = town->getID();
	if (RunEditTownModal(this, map, town, false)) {
		MapTab* mapTab = g_gui.GetCurrentMapTab();
		if (mapTab && mapTab->GetMap() == map) {
			if (map->towns.getTown(town->getID())) {
				selected_town_id_ = town->getID();
			} else if (map->towns.getTown(old_id)) {
				selected_town_id_ = old_id;
			}
			OnUpdate();
		}
	}
}

void TownPalettePanel::OnClickRemoveTown(wxCommandEvent& /*event*/) {
	Town* town = GetSelectedTown();
	if (!town || !map) {
		return;
	}

	size_t house_count = 0;
	std::vector<House*> houses_to_wipe;
	for (const auto& [_, house] : map->houses) {
		if (house && house->townid == town->getID()) {
			houses_to_wipe.push_back(house.get());
			++house_count;
		}
	}

	if (house_count > 0) {
		TownDeleteDialog del_dlg(this, town->getName(), town->getID(), house_count);
		if (del_dlg.ShowModal() != wxID_YES) {
			return;
		}

		// User confirmed wiping all associated houses!
		for (House* h : houses_to_wipe) {
			for (const auto& pos : h->getTiles()) {
				map->getChangeTracker().markTileDirty(pos);
				g_minimap.MarkTileDirty(*map, pos);
			}
			if (h->getExit().isValid()) {
				map->getChangeTracker().markTileDirty(h->getExit());
				g_minimap.MarkTileDirty(*map, h->getExit());
			}
			map->houses.removeHouse(h);
		}
	} else {
		const int ret = wxMessageBox(
			wxString::Format("Are you sure you want to remove town '%s' (ID: %u)?", wxstr(town->getName()), town->getID()),
			"Confirm Removal",
			wxYES_NO | wxICON_QUESTION | wxCENTER,
			this
		);
		if (ret != wxYES) {
			return;
		}
	}

	// Remove town flag from tile
	Position temple_pos = town->getTemplePosition();
	if (temple_pos != Position()) {
		if (auto* tile = map->getTile(temple_pos)) {
			if (auto* loc = tile->getLocation()) {
				loc->decreaseTownCount();
			}
		}
		if (temple_pos.isValid()) {
			map->getChangeTracker().markTileDirty(temple_pos);
			g_minimap.MarkTileDirty(*map, temple_pos);
		}
	}

	map->towns.removeTown(town);
	selected_town_id_ = 0;
	map->doChange();
	g_gui.RefreshPalettes();
	g_gui.RefreshView();
}

void TownPalettePanel::OnClickChangeTemple(wxCommandEvent& /*event*/) {
	Town* town = GetSelectedTown();
	if (!town) {
		g_gui.SetStatusText("Please select a town first.");
		return;
	}

	if (g_brush_manager.temple_brush) {
		g_brush_manager.temple_brush->invokeCancelCallback();
		g_brush_manager.temple_brush->setTown(town);
		g_gui.SelectBrush(g_brush_manager.temple_brush);
		g_gui.SetStatusText(wxstr(std::format("Click on map to set temple position for town '{}' (ID: {})... [Esc to cancel]", town->getName(), town->getID())));
	}
}
