//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "palette/house/house_palette.h"
#include "palette/house/edit_house_dialog.h"

#include "map/map.h"
#include "game/house.h"
#include "game/town.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/dialog_util.h"
#include "util/image_manager.h"
#include "util/common.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"

#include "ui/theme.h"

#include <wx/stattext.h>
#include <wx/menu.h>
#include <algorithm>
#include <sstream>

HousePalette::HousePalette(wxWindow* parent) :
	PalettePanel(parent, wxID_ANY),
	map(nullptr) {

	wxBoxSizer* main_sizer = newd wxBoxSizer(wxVERTICAL);

	// Header section (Town & Search)
	wxBoxSizer* filter_sizer = newd wxBoxSizer(wxVERTICAL);

	town_choice = newd wxChoice(this, PALETTE_HOUSE_TOWN_CHOICE);
	town_choice->SetFont(Theme::GetFont(9, false));
	filter_sizer->Add(town_choice, 0, wxEXPAND | wxBOTTOM, 4);

	search_ctrl = newd PaletteSearchCtrl(this, PALETTE_HOUSE_SEARCH, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Search houses...");
	search_ctrl->SetFont(Theme::GetFont(9, false));
	filter_sizer->Add(search_ctrl, 0, wxEXPAND);

	main_sizer->Add(filter_sizer, 0, wxEXPAND | wxALL, 5);

	// Table section (NanoVG Table)
	house_table = newd NanoVGTableCtrl(this, PALETTE_HOUSE_LISTBOX);
	std::vector<TableColumn> cols = {
		{ "guild", "Guild", 40, NVG_ALIGN_CENTER, true },
		{ "id", "ID", 45, NVG_ALIGN_LEFT, true },
		{ "name", "Name", 0, NVG_ALIGN_LEFT, true },
		{ "rent", "Rent", 70, NVG_ALIGN_RIGHT, true },
		{ "size", "Size", 50, NVG_ALIGN_RIGHT, true }
	};
	house_table->SetColumns(std::move(cols));

	house_table->SetOnSelect([this](int idx, const TableRow& row) {
		OnHouseSelected(idx, row);
	});
	house_table->SetOnActivate([this](int idx, const TableRow& row) {
		OnHouseActivated(idx, row);
	});
	house_table->SetOnSort([this](int col, bool asc) {
		OnHouseSort(col, asc);
	});
	house_table->SetOnContextMenu([this](int idx, const TableRow* row, const wxPoint& pos) {
		OnHouseContextMenu(idx, row, pos);
	});

	main_sizer->Add(house_table, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Action buttons (Add, Edit, Remove)
	wxBoxSizer* action_sizer = newd wxBoxSizer(wxHORIZONTAL);
	add_button = newd wxButton(this, PALETTE_HOUSE_ADD_HOUSE, "Add", wxDefaultPosition, wxSize(50, -1));
	add_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	add_button->SetFont(Theme::GetFont(9, false));

	edit_button = newd wxButton(this, PALETTE_HOUSE_EDIT_HOUSE, "Edit", wxDefaultPosition, wxSize(50, -1));
	edit_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	edit_button->SetFont(Theme::GetFont(9, false));

	remove_button = newd wxButton(this, PALETTE_HOUSE_REMOVE_HOUSE, "Remove", wxDefaultPosition, wxSize(60, -1));
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	remove_button->SetFont(Theme::GetFont(9, false));

	action_sizer->Add(add_button, 1, wxEXPAND | wxRIGHT, 2);
	action_sizer->Add(edit_button, 1, wxEXPAND | wxRIGHT, 2);
	action_sizer->Add(remove_button, 1, wxEXPAND);

	main_sizer->Add(action_sizer, 0, wxEXPAND | wxALL, 5);

	// Brush buttons (House tiles, Select Exit)
	wxBoxSizer* brush_sizer = newd wxBoxSizer(wxHORIZONTAL);

	house_brush_button = newd wxToggleButton(this, PALETTE_HOUSE_BRUSH_BUTTON, "House tiles");
	house_brush_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PAINTBRUSH, wxSize(16, 16)));
	house_brush_button->SetFont(Theme::GetFont(9, false));

	select_exit_button = newd wxToggleButton(this, PALETTE_HOUSE_SELECT_EXIT_BUTTON, "Select Exit");
	select_exit_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DOOR_OPEN, wxSize(16, 16)));
	select_exit_button->SetFont(Theme::GetFont(9, false));

	brush_sizer->Add(house_brush_button, 1, wxEXPAND | wxRIGHT, 2);
	brush_sizer->Add(select_exit_button, 1, wxEXPAND);

	main_sizer->Add(brush_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizer(main_sizer);

	Bind(wxEVT_CHOICE, &HousePalette::OnTownChange, this, PALETTE_HOUSE_TOWN_CHOICE);
	Bind(wxEVT_TEXT, &HousePalette::OnSearchChange, this, PALETTE_HOUSE_SEARCH);

	Bind(wxEVT_BUTTON, &HousePalette::OnAddHouse, this, PALETTE_HOUSE_ADD_HOUSE);
	Bind(wxEVT_BUTTON, &HousePalette::OnEditHouse, this, PALETTE_HOUSE_EDIT_HOUSE);
	Bind(wxEVT_BUTTON, &HousePalette::OnRemoveHouse, this, PALETTE_HOUSE_REMOVE_HOUSE);

	Bind(wxEVT_MENU, &HousePalette::OnAddHouse, this, PALETTE_HOUSE_ADD_HOUSE);
	Bind(wxEVT_MENU, &HousePalette::OnEditHouse, this, PALETTE_HOUSE_EDIT_HOUSE);
	Bind(wxEVT_MENU, &HousePalette::OnRemoveHouse, this, PALETTE_HOUSE_REMOVE_HOUSE);
	Bind(wxEVT_MENU, &HousePalette::OnGoToExit, this, PALETTE_HOUSE_GOTO_EXIT);
	Bind(wxEVT_MENU, &HousePalette::OnHouseBrushButton, this, PALETTE_HOUSE_BRUSH_BUTTON);
	Bind(wxEVT_MENU, &HousePalette::OnSelectExitButton, this, PALETTE_HOUSE_SELECT_EXIT_BUTTON);

	Bind(wxEVT_TOGGLEBUTTON, &HousePalette::OnHouseBrushButton, this, PALETTE_HOUSE_BRUSH_BUTTON);
	Bind(wxEVT_TOGGLEBUTTON, &HousePalette::OnSelectExitButton, this, PALETTE_HOUSE_SELECT_EXIT_BUTTON);
}

HousePalette::~HousePalette() = default;

wxString HousePalette::GetName() const {
	return "House";
}

void HousePalette::SetMap(Map* m) {
	if (map != m) {
		selected_house_id_ = 0;
		if (g_brush_manager.house_brush) {
			g_brush_manager.house_brush->setHouse(nullptr);
		}
		if (g_brush_manager.house_exit_brush) {
			g_brush_manager.house_exit_brush->setHouse(nullptr);
		}
	}
	map = m;
	UpdateHouses();
}

void HousePalette::UpdateHouses() {
	town_choice->Clear();

	if (!map) {
		house_table->ClearRows();
		return;
	}

	town_choice->Append("All Towns", static_cast<void*>(nullptr));
	for (auto& it : map->towns) {
		town_choice->Append(wxstr(it.second->getName()), it.second.get());
	}
	town_choice->Append("No Town", static_cast<void*>(nullptr));
	town_choice->SetSelection(0);

	FilterHouses();
}

void HousePalette::FilterHouses() {
	if (!map) {
		house_table->ClearRows();
		return;
	}

	const wxString search_query = search_ctrl->GetValue().Lower();
	const int town_idx = town_choice->GetSelection();
	Town* selected_town = nullptr;
	bool filter_no_town = false;

	if (town_idx != wxNOT_FOUND) {
		if (town_idx > 0 && town_idx < static_cast<int>(town_choice->GetCount()) - 1) {
			selected_town = static_cast<Town*>(town_choice->GetClientData(town_idx));
		} else if (town_idx == static_cast<int>(town_choice->GetCount()) - 1) {
			filter_no_town = true;
		}
	}

	std::vector<TableRow> rows;

	for (auto& it : map->houses) {
		House* house = it.second.get();
		if (!house) {
			continue;
		}

		const bool search_match = search_query.IsEmpty() ||
			wxstr(house->name).Lower().Contains(search_query) ||
			wxstr(std::to_string(house->getID())).Contains(search_query);

		bool town_match = true;
		if (search_query.IsEmpty()) {
			if (selected_town) {
				town_match = (house->townid == selected_town->getID());
			} else if (filter_no_town) {
				town_match = (map->towns.getTown(house->townid) == nullptr);
			}
		}

		if (search_match && town_match) {
			TableRow row;
			row.id = house->getID();
			row.key = std::to_string(house->getID());
			row.client_data = house;

			if (house->guildhall) {
				row.icon_col = 0;
				row.icon_name = std::string(ICON_HOUSE_USER);
			}

			row.cells.push_back(""); // Guild column - renders ICON_HOUSE_USER if guildhall, blank if not
			row.cells.push_back(std::to_string(house->getID()));
			row.cells.push_back(house->name);
			row.cells.push_back(format_thousands_separator(house->rent));
			row.cells.push_back(std::to_string(house->size()));

			rows.push_back(std::move(row));
		}
	}

	// Sort according to active sort column
	const int sort_col = house_table->GetSortColumn();
	const bool asc = house_table->IsSortAscending();

	if (sort_col >= 0) {
		std::stable_sort(rows.begin(), rows.end(), [sort_col, asc](const TableRow& a, const TableRow& b) {
			auto* ha = static_cast<const House*>(a.client_data);
			auto* hb = static_cast<const House*>(b.client_data);
			if (!ha || !hb) {
				return false;
			}

			switch (sort_col) {
				case 0: // Guild
					if (ha->guildhall != hb->guildhall) {
						return asc ? ha->guildhall : hb->guildhall;
					}
					return asc ? (ha->getID() < hb->getID()) : (hb->getID() < ha->getID());
				case 1: // ID
					return asc ? (ha->getID() < hb->getID()) : (hb->getID() < ha->getID());
				case 2: { // Name
					int cmp = wxStricmp(wxstr(ha->name), wxstr(hb->name));
					if (cmp != 0) {
						return asc ? (cmp < 0) : (cmp > 0);
					}
					return asc ? (ha->getID() < hb->getID()) : (hb->getID() < ha->getID());
				}
				case 3: // Rent
					if (ha->rent != hb->rent) {
						return asc ? (ha->rent < hb->rent) : (hb->rent < ha->rent);
					}
					return asc ? (ha->getID() < hb->getID()) : (hb->getID() < ha->getID());
				case 4: // Size
					if (ha->size() != hb->size()) {
						return asc ? (ha->size() < hb->size()) : (hb->size() < ha->size());
					}
					return asc ? (ha->getID() < hb->getID()) : (hb->getID() < ha->getID());
				default:
					return false;
			}
		});
	}

	house_table->SetRows(std::move(rows));

	if (selected_house_id_ != 0) {
		house_table->SelectById(selected_house_id_);
	}
}

House* HousePalette::GetSelectedHouse() const {
	if (!map) {
		return nullptr;
	}
	if (house_table) {
		const TableRow* row = house_table->GetSelectedRow();
		if (row && row->client_data) {
			return static_cast<House*>(row->client_data);
		}
	}
	if (selected_house_id_ != 0) {
		return map->houses.getHouse(selected_house_id_);
	}
	return nullptr;
}

void HousePalette::UpdateHouseSize(uint32_t house_id) {
	if (!map || !house_table) {
		return;
	}

	const size_t count = house_table->GetRowCount();
	for (size_t i = 0; i < count; ++i) {
		const TableRow* r = house_table->GetRow(i);
		if (!r) {
			continue;
		}
		auto* h = static_cast<House*>(r->client_data);
		if (h && (house_id == 0 || h->getID() == house_id)) {
			std::vector<std::string> cells = r->cells;
			if (cells.size() > 4) {
				cells[4] = std::to_string(h->size());
				house_table->UpdateRowCells(i, std::move(cells));
			}
			if (house_id != 0) {
				break;
			}
		}
	}
}

Brush* HousePalette::GetSelectedBrush() const {
	House* house = GetSelectedHouse();
	if (select_exit_button->GetValue()) {
		if (g_brush_manager.house_exit_brush) {
			g_brush_manager.house_exit_brush->setHouse(house);
			return (g_brush_manager.house_exit_brush->getHouseID() != 0 ? g_brush_manager.house_exit_brush : nullptr);
		}
	} else if (house_brush_button->GetValue()) {
		if (g_brush_manager.house_brush) {
			g_brush_manager.house_brush->setHouse(house);
			return (g_brush_manager.house_brush->getHouseID() != 0 ? g_brush_manager.house_brush : nullptr);
		}
	}
	return nullptr;
}

bool HousePalette::SelectBrush(const Brush* whatbrush) {
	if (whatbrush && whatbrush == g_brush_manager.house_brush) {
		SelectHouseBrush();
		return true;
	}
	if (whatbrush && whatbrush == g_brush_manager.house_exit_brush) {
		SelectExitBrush();
		return true;
	}
	return false;
}

void HousePalette::OnUpdate() {
	UpdateHouses();
}

void HousePalette::SelectHouseBrush() {
	house_brush_button->SetValue(true);
	select_exit_button->SetValue(false);
	House* house = GetSelectedHouse();
	if (g_brush_manager.house_brush) {
		g_brush_manager.house_brush->setHouse(house);
	}
	g_gui.SelectBrush();
}

void HousePalette::SelectExitBrush() {
	house_brush_button->SetValue(false);
	select_exit_button->SetValue(true);
	House* house = GetSelectedHouse();
	if (g_brush_manager.house_exit_brush) {
		g_brush_manager.house_exit_brush->setHouse(house);
	}
	g_gui.SelectBrush();
}

void HousePalette::OnTownChange(wxCommandEvent& /*event*/) {
	FilterHouses();
}

void HousePalette::OnSearchChange(wxCommandEvent& /*event*/) {
	FilterHouses();
}

void HousePalette::OnHouseSelected(int /*index*/, const TableRow& row) {
	auto* house = static_cast<House*>(row.client_data);
	if (house) {
		selected_house_id_ = house->getID();
		if (house_brush_button->GetValue()) {
			g_brush_manager.house_brush->setHouse(house);
		} else if (select_exit_button->GetValue()) {
			g_brush_manager.house_exit_brush->setHouse(house);
		}
		g_gui.SelectBrush();
	}
}

void HousePalette::OnHouseActivated(int /*index*/, const TableRow& row) {
	auto* house = static_cast<House*>(row.client_data);
	if (house) {
		selected_house_id_ = house->getID();
		SelectHouseBrush();
		g_gui.SelectBrush();
		if (house->getExit() != Position(0, 0, 0) && house->getExit().isValid()) {
			g_gui.SetScreenCenterPosition(house->getExit());
		}
	}
}

void HousePalette::OnHouseSort(int /*column*/, bool /*ascending*/) {
	FilterHouses();
}

void HousePalette::OnHouseContextMenu(int /*index*/, const TableRow* row, const wxPoint& /*pos*/) {
	if (!map) {
		return;
	}

	wxMenu menu;
	if (row && row->client_data) {
		auto* house = static_cast<House*>(row->client_data);
		selected_house_id_ = house->getID();

		if (house->getExit() != Position(0, 0, 0) && house->getExit().isValid()) {
			menu.Append(PALETTE_HOUSE_GOTO_EXIT, "Go to exit (double-click)")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MAP_PIN, wxSize(16, 16)));
			menu.AppendSeparator();
		}
		menu.Append(PALETTE_HOUSE_BRUSH_BUTTON, "Paint house tiles")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PAINTBRUSH, wxSize(16, 16)));
		menu.Append(PALETTE_HOUSE_SELECT_EXIT_BUTTON, "Select house exit")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DOOR_OPEN, wxSize(16, 16)));
		menu.AppendSeparator();
		menu.Append(PALETTE_HOUSE_EDIT_HOUSE, "Edit house...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
		menu.Append(PALETTE_HOUSE_REMOVE_HOUSE, "Remove house...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
		menu.AppendSeparator();
	}

	menu.Append(PALETTE_HOUSE_ADD_HOUSE, "Add house...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	PopupMenu(&menu);
}

void HousePalette::OnGoToExit(wxCommandEvent& /*event*/) {
	House* house = GetSelectedHouse();
	if (house && house->getExit() != Position(0, 0, 0) && house->getExit().isValid()) {
		g_gui.SetScreenCenterPosition(house->getExit());
	}
}

void HousePalette::OnAddHouse(wxCommandEvent& /*event*/) {
	if (!map) {
		return;
	}

	auto new_house = std::make_unique<House>(*map);
	new_house->setID(map->houses.getEmptyID());

	std::ostringstream os;
	os << "Unnamed House #" << new_house->getID();
	new_house->name = os.str();

	const int town_idx = town_choice->GetSelection();
	if (town_idx > 0 && town_idx < static_cast<int>(town_choice->GetCount()) - 1) {
		auto* town = static_cast<Town*>(town_choice->GetClientData(town_idx));
		new_house->townid = town ? town->getID() : 0;
	}

	House* house_ptr = new_house.get();
	selected_house_id_ = house_ptr->getID();
	map->houses.addHouse(std::move(new_house));
	FilterHouses();

	g_gui.SelectBrush();
}

void HousePalette::OnEditHouse(wxCommandEvent& /*event*/) {
	House* house = GetSelectedHouse();
	if (house && map) {
		selected_house_id_ = house->getID();
		auto* d = newd EditHouseDialog(g_gui.root, map, house);
		if (d->ShowModal() == 1) {
			FilterHouses();
			g_gui.SelectBrush();
		}
		d->Destroy();
	}
}

void HousePalette::OnRemoveHouse(wxCommandEvent& /*event*/) {
	House* house = GetSelectedHouse();
	if (house && map) {
		const int ret = wxMessageBox("Are you sure you want to remove this house? This cannot be undone.", "Remove House", wxYES_NO | wxICON_WARNING | wxCENTER, this);
		if (ret == wxYES) {
			if (selected_house_id_ == house->getID()) {
				selected_house_id_ = 0;
			}
			map->houses.removeHouse(house);
			FilterHouses();
			g_gui.SelectBrush();
			g_gui.RefreshView();
		}
	}
}

void HousePalette::OnHouseBrushButton(wxCommandEvent& /*event*/) {
	SelectHouseBrush();
}

void HousePalette::OnSelectExitButton(wxCommandEvent& /*event*/) {
	SelectExitBrush();
}
