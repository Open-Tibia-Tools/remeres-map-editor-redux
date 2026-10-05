//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/palette_zones.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/zone/zone_brush.h"
#include "map/map.h"
#include "ui/gui.h"
#include "util/image_manager.h"

#include <wx/textdlg.h>

#include <wx/menu.h>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <span>

ZonePalettePanel::ZonePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* sidesizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Zones");
	wxWindow* box = sidesizer->GetStaticBox();

	zone_list = newd wxListCtrl(box, PALETTE_ZONE_LISTBOX, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
	zone_list->InsertColumn(0, "Id", wxLIST_FORMAT_RIGHT, 50);
	zone_list->InsertColumn(1, "Name", wxLIST_FORMAT_LEFT, 150);
	sidesizer->Add(zone_list, 1, wxEXPAND);

	auto* buttons = newd wxBoxSizer(wxHORIZONTAL);
	add_button = newd wxButton(box, PALETTE_ZONE_ADD, "Add");
	add_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	rename_button = newd wxButton(box, PALETTE_ZONE_RENAME, "Rename");
	rename_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	remove_button = newd wxButton(box, PALETTE_ZONE_REMOVE, "Remove");
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	buttons->Add(add_button, 1, wxEXPAND);
	buttons->Add(rename_button, 1, wxEXPAND);
	buttons->Add(remove_button, 1, wxEXPAND);
	sidesizer->Add(buttons, 0, wxEXPAND);

	SetSizerAndFit(sidesizer);

	Bind(wxEVT_LIST_ITEM_SELECTED, &ZonePalettePanel::OnSelectZone, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &ZonePalettePanel::OnActivateZone, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &ZonePalettePanel::OnZoneContextMenu, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickAdd, this, PALETTE_ZONE_ADD);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnGoToZone, this, PALETTE_ZONE_GOTO);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);
}

namespace {

	// Zones can be split or concave, so jump to the real zone tile closest to the centroid of its busiest floor
	std::optional<Position> zoneCenter(std::span<const Position> positions) {
		if (positions.empty()) {
			return std::nullopt;
		}
		std::array<int, MAP_LAYERS> floor_counts {};
		for (const Position& pos : positions) {
			++floor_counts[pos.z];
		}
		const int floor = static_cast<int>(std::ranges::max_element(floor_counts) - floor_counts.begin());

		auto on_floor = positions | std::views::filter([floor](const Position& p) { return p.z == floor; });
		double sum_x = 0.0;
		double sum_y = 0.0;
		for (const Position& pos : on_floor) {
			sum_x += pos.x;
			sum_y += pos.y;
		}
		const double cx = sum_x / floor_counts[floor];
		const double cy = sum_y / floor_counts[floor];

		return *std::ranges::min_element(on_floor, {}, [cx, cy](const Position& p) {
			const double dx = p.x - cx;
			const double dy = p.y - cy;
			return dx * dx + dy * dy;
		});
	}

} // namespace

wxString ZonePalettePanel::GetName() const {
	return "Zone";
}

void ZonePalettePanel::SetMap(Map* m) {
	map = m;
	OnUpdate();
}

bool ZonePalettePanel::supportsZones() const {
	return map && map->getVersion().otbm == MAP_OTBM_5;
}

uint16_t ZonePalettePanel::selectedZoneId() const {
	const long item = zone_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	return item == -1 ? 0 : static_cast<uint16_t>(zone_list->GetItemData(item));
}

Brush* ZonePalettePanel::GetSelectedBrush() const {
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	g_brush_manager.zone_brush->setZone(id);
	return id != 0 ? g_brush_manager.zone_brush : nullptr;
}

bool ZonePalettePanel::SelectBrush(const Brush* whatbrush) {
	return whatbrush == g_brush_manager.zone_brush;
}

void ZonePalettePanel::OnUpdate() {
	const uint16_t keep = g_brush_manager.zone_brush ? g_brush_manager.zone_brush->getZone() : 0;
	const bool enabled = supportsZones();

	refreshing = true;
	zone_list->Freeze();
	zone_list->DeleteAllItems();
	if (enabled) {
		for (const auto& zone : map->zones) {
			const long item = zone_list->InsertItem(zone_list->GetItemCount(), wxString::Format("%u", zone.id));
			zone_list->SetItem(item, 1, wxstr(zone.name));
			zone_list->SetItemData(item, zone.id);
			if (zone.id == keep) {
				zone_list->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			}
		}
	}
	zone_list->Thaw();
	refreshing = false;

	zone_list->Enable(enabled);
	add_button->Enable(enabled);
	rename_button->Enable(enabled);
	remove_button->Enable(enabled);
	zone_list->SetToolTip(enabled || !map ? wxString() : wxString("Zones require an OTBM 5 (Crystal Server) map."));
}

void ZonePalettePanel::OnSelectZone(wxListEvent& /*event*/) {
	if (!refreshing) {
		g_gui.SelectBrush();
	}
}

void ZonePalettePanel::OnActivateZone(wxListEvent& /*event*/) {
	wxCommandEvent unused;
	OnGoToZone(unused);
}

void ZonePalettePanel::OnZoneContextMenu(wxListEvent& event) {
	if (!supportsZones() || event.GetIndex() < 0) {
		return;
	}
	zone_list->SetItemState(event.GetIndex(), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);

	wxMenu menu;
	menu.Append(PALETTE_ZONE_GOTO, "Go to zone (double-click)")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MAP_PIN, wxSize(16, 16)));
	menu.AppendSeparator();
	menu.Append(PALETTE_ZONE_RENAME, "Rename...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	menu.Append(PALETTE_ZONE_REMOVE, "Remove...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	PopupMenu(&menu);
}

void ZonePalettePanel::OnGoToZone(wxCommandEvent& /*event*/) {
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	if (id == 0) {
		return;
	}
	wxBusyCursor busy;
	const std::vector<Position> positions = map->getZoneTilePositions(id);
	if (const auto center = zoneCenter(positions)) {
		g_gui.SetScreenCenterPosition(*center);
		g_gui.SetStatusText(wxstr(std::format("Zone {}: {} tiles", id, positions.size())));
	} else {
		g_gui.SetStatusText("This zone has no tiles yet.");
	}
}

void ZonePalettePanel::OnClickAdd(wxCommandEvent& /*event*/) {
	if (!supportsZones()) {
		return;
	}
	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Add Zone", wxEmptyString, this).Trim().Trim(false));
	if (name.empty()) {
		return;
	}
	const uint16_t id = map->addZone(name);
	if (id == 0) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}
	g_brush_manager.zone_brush->setZone(id);
	OnUpdate();
	g_gui.SelectBrush();
}

void ZonePalettePanel::OnClickRename(wxCommandEvent& /*event*/) {
	const uint16_t id = selectedZoneId();
	const Map::Zone* zone = supportsZones() && id != 0 ? map->findZone(id) : nullptr;
	if (!zone) {
		return;
	}
	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Rename Zone", wxstr(zone->name), this).Trim().Trim(false));
	if (name.empty() || name == zone->name) {
		return;
	}
	if (!map->renameZone(id, name)) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}
	OnUpdate();
}

void ZonePalettePanel::OnClickRemove(wxCommandEvent& /*event*/) {
	const uint16_t id = selectedZoneId();
	const Map::Zone* zone = supportsZones() && id != 0 ? map->findZone(id) : nullptr;
	if (!zone) {
		return;
	}
	const wxString question = wxstr(std::format("Remove zone \"{}\" (id {}) and clear it from every tile?\nThis cannot be undone.", zone->name, id));
	if (wxMessageBox(question, "Remove Zone", wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}
	map->removeZone(id);
	g_brush_manager.zone_brush->setZone(0);
	OnUpdate();
	g_gui.SelectBrush();
	g_gui.RefreshView();
}
