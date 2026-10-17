//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/palette_zones.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/zone/zone_brush.h"
#include "map/map.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "util/image_manager.h"
#include "util/common.h"
#include "rendering/indicators/zone_colors.h"
#include "ui/theme.h"

#include <wx/textdlg.h>
#include <wx/menu.h>

#include <algorithm>
#include <format>
#include <ranges>

ZonePalettePanel::ZonePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	wxBoxSizer* sidesizer = newd wxBoxSizer(wxVERTICAL);

	// Search control
	search_ctrl = newd PaletteSearchCtrl(this, PALETTE_ZONE_SEARCH, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Search zones...");
	search_ctrl->SetFont(Theme::GetFont(9, false));
	sidesizer->Add(search_ctrl, 0, wxEXPAND | wxALL, 5);

	// NanoVG Table
	zone_table = newd NanoVGTableCtrl(this, PALETTE_ZONE_LISTBOX);
	std::vector<TableColumn> cols = {
		{ "swatch", "#", 30, NVG_ALIGN_CENTER, false },
		{ "id", "ID", 38, NVG_ALIGN_CENTER, true },
		{ "name", "Name", 0, NVG_ALIGN_LEFT, true },
		{ "areas", "Areas", 46, NVG_ALIGN_CENTER, true },
		{ "tiles", "Tiles", 56, NVG_ALIGN_RIGHT, true }
	};
	zone_table->SetColumns(std::move(cols));

	zone_table->SetOnSelect([this](int idx, const TableRow& row) {
		OnSelectZone(idx, row);
	});
	zone_table->SetOnActivate([this](int idx, const TableRow& row) {
		OnActivateZone(idx, row);
	});
	zone_table->SetOnSort([this](int col, bool asc) {
		OnSortZones(col, asc);
	});
	zone_table->SetOnContextMenu([this](int idx, const TableRow* row, const wxPoint& pos) {
		OnZoneContextMenu(idx, row, pos);
	});
	zone_table->SetOnToggleGroup([this](int idx, const TableRow& row) {
		OnToggleGroup(idx, row);
	});

	sidesizer->Add(zone_table, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Action buttons (Add, Rename, Remove)
	wxBoxSizer* buttons = newd wxBoxSizer(wxHORIZONTAL);
	add_button = newd wxButton(this, PALETTE_ZONE_ADD, "Add", wxDefaultPosition, wxSize(50, -1));
	add_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	add_button->SetFont(Theme::GetFont(9, false));

	rename_button = newd wxButton(this, PALETTE_ZONE_RENAME, "Rename", wxDefaultPosition, wxSize(60, -1));
	rename_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	rename_button->SetFont(Theme::GetFont(9, false));

	remove_button = newd wxButton(this, PALETTE_ZONE_REMOVE, "Remove", wxDefaultPosition, wxSize(60, -1));
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	remove_button->SetFont(Theme::GetFont(9, false));

	buttons->Add(add_button, 1, wxEXPAND | wxRIGHT, 2);
	buttons->Add(rename_button, 1, wxEXPAND | wxRIGHT, 2);
	buttons->Add(remove_button, 1, wxEXPAND);
	sidesizer->Add(buttons, 0, wxEXPAND | wxALL, 5);

	SetSizer(sidesizer);

	Bind(wxEVT_TEXT, &ZonePalettePanel::OnSearchChange, this, PALETTE_ZONE_SEARCH);

	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickAdd, this, PALETTE_ZONE_ADD);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);

	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickAdd, this, PALETTE_ZONE_ADD);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnGoToZone, this, PALETTE_ZONE_GOTO);
}

wxString ZonePalettePanel::GetName() const {
	return "Zone";
}

void ZonePalettePanel::SetMap(Map* m) {
	if (map != m) {
		selected_zone_id_ = 0;
		selected_cluster_ = -1;
		if (g_brush_manager.zone_brush) {
			g_brush_manager.zone_brush->setZone(0);
		}
	}
	map = m;
	OnUpdate();
}

bool ZonePalettePanel::supportsZones() const {
	return map && map->getVersion().otbm == MAP_OTBM_5;
}

uint16_t ZonePalettePanel::selectedZoneId() const {
	return selectedRow().zone;
}

ZonePalettePanel::RowRef ZonePalettePanel::selectedRow() const {
	if (!zone_table) {
		return {};
	}
	const TableRow* r = zone_table->GetSelectedRow();
	if (!r || r->key.empty()) {
		if (selected_zone_id_ != 0) {
			return RowRef{ selected_zone_id_, selected_cluster_ };
		}
		return {};
	}
	const auto colon = r->key.find(':');
	if (colon != std::string::npos) {
		const uint16_t zid = static_cast<uint16_t>(std::strtoul(r->key.substr(0, colon).c_str(), nullptr, 10));
		const int cidx = static_cast<int>(std::strtol(r->key.substr(colon + 1).c_str(), nullptr, 10));
		return RowRef{ zid, cidx };
	} else {
		const uint16_t zid = static_cast<uint16_t>(std::strtoul(r->key.c_str(), nullptr, 10));
		return RowRef{ zid, -1 };
	}
}

Brush* ZonePalettePanel::GetSelectedBrush() const {
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	return id != 0 ? g_brush_manager.zone_brush : nullptr;
}

bool ZonePalettePanel::SelectBrush(const Brush* whatbrush) {
	return whatbrush == g_brush_manager.zone_brush;
}

void ZonePalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(id);
	}
}

void ZonePalettePanel::OnUpdate() {
	const bool enabled = supportsZones();

	if (!enabled || !map) {
		zone_table->ClearRows();
		zone_table->Enable(false);
		if (search_ctrl) {
			search_ctrl->Enable(false);
		}
		add_button->Enable(false);
		rename_button->Enable(false);
		remove_button->Enable(false);
		zone_table->SetToolTip(enabled || !map ? wxString() : wxString("Zones require an OTBM 5 map (Canary / Crystal Server)."));
		return;
	}

	zone_table->Enable(true);
	if (search_ctrl) {
		search_ctrl->Enable(true);
	}
	add_button->Enable(true);
	rename_button->Enable(true);
	remove_button->Enable(true);
	zone_table->SetToolTip(wxString());

	const wxString search_query = search_ctrl ? search_ctrl->GetValue().Lower() : wxString();
	const bool has_search = !search_query.IsEmpty();

	struct ZoneEntry {
		const Map::Zone* zone = nullptr;
		size_t total_tiles = 0;
		std::vector<ZoneCluster> clusters;
	};

	std::vector<ZoneEntry> zone_entries;
	for (const auto& z : map->zones) {
		if (z.id == 0) {
			continue;
		}
		ZoneEntry entry;
		entry.zone = &z;
		entry.clusters = map->getZoneClusters(z.id);
		entry.total_tiles = map->getZoneTilePositions(z.id).size();

		if (has_search) {
			bool match = wxstr(z.name).Lower().Contains(search_query) ||
			             wxstr(std::to_string(z.id)).Contains(search_query);
			if (!match) {
				for (size_t ci = 0; ci < entry.clusters.size(); ++ci) {
					const auto& c = entry.clusters[ci];
					if (wxstr(std::format("{}, {}, {}", c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z)).Lower().Contains(search_query) ||
					    wxstr(std::format("area {}", ci + 1)).Lower().Contains(search_query)) {
						match = true;
						break;
					}
				}
			}
			if (!match) {
				continue;
			}
		}
		zone_entries.push_back(std::move(entry));
	}

	// Sort zones according to active table sort column
	const int sort_col = zone_table->GetSortColumn();
	const bool asc = zone_table->IsSortAscending();

	if (sort_col >= 0) {
		std::stable_sort(zone_entries.begin(), zone_entries.end(), [sort_col, asc](const ZoneEntry& a, const ZoneEntry& b) {
			switch (sort_col) {
				case 1: // ID
					return asc ? (a.zone->id < b.zone->id) : (b.zone->id < a.zone->id);
				case 2: { // Name
					int cmp = wxStricmp(wxstr(a.zone->name), wxstr(b.zone->name));
					if (cmp != 0) {
						return asc ? (cmp < 0) : (cmp > 0);
					}
					return asc ? (a.zone->id < b.zone->id) : (b.zone->id < a.zone->id);
				}
				case 3: // Areas
					if (a.clusters.size() != b.clusters.size()) {
						return asc ? (a.clusters.size() < b.clusters.size()) : (b.clusters.size() < a.clusters.size());
					}
					return asc ? (a.zone->id < b.zone->id) : (b.zone->id < a.zone->id);
				case 4: // Tiles
					if (a.total_tiles != b.total_tiles) {
						return asc ? (a.total_tiles < b.total_tiles) : (b.total_tiles < a.total_tiles);
					}
					return asc ? (a.zone->id < b.zone->id) : (b.zone->id < a.zone->id);
				default:
					return asc ? (a.zone->id < b.zone->id) : (b.zone->id < a.zone->id);
			}
		});
	}

	// Flatten into TableRows
	std::vector<TableRow> rows;
	for (const auto& entry : zone_entries) {
		const auto& z = *entry.zone;
		const glm::vec3 rgb = rme::rendering::getZoneColor(z.id);
		const bool is_multi_area = (entry.clusters.size() > 1);
		const bool is_expanded = (is_multi_area && (expanded_zones_.contains(z.id) || has_search));

		TableRow row;
		row.id = (static_cast<uint64_t>(z.id) << 32);
		row.key = std::to_string(z.id);
		row.client_data = const_cast<Map::Zone*>(&z);

		row.has_color_swatch = true;
		row.color_swatch_col = 0;
		row.swatch_r = static_cast<uint8_t>(std::clamp(rgb.r * 255.0f, 0.0f, 255.0f));
		row.swatch_g = static_cast<uint8_t>(std::clamp(rgb.g * 255.0f, 0.0f, 255.0f));
		row.swatch_b = static_cast<uint8_t>(std::clamp(rgb.b * 255.0f, 0.0f, 255.0f));

		row.indent_level = 0;
		row.group_col = 2; // Name column contains the expander arrow
		row.is_group = is_multi_area;
		row.is_expanded = is_expanded;

		row.cells.push_back(""); // Swatch column
		row.cells.push_back(std::to_string(z.id));
		row.cells.push_back(z.name);
		row.cells.push_back(is_multi_area ? std::to_string(entry.clusters.size()) : "1");
		row.cells.push_back(format_thousands_separator(entry.total_tiles));

		rows.push_back(std::move(row));

		if (is_expanded) {
			for (size_t ci = 0; ci < entry.clusters.size(); ++ci) {
				const auto& c = entry.clusters[ci];
				TableRow child_row;
				child_row.id = (static_cast<uint64_t>(z.id) << 32) | static_cast<uint32_t>(ci + 1);
				child_row.key = std::format("{}:{}", z.id, ci);
				child_row.client_data = const_cast<Map::Zone*>(&z);

				child_row.indent_level = 1;
				child_row.group_col = 2;
				child_row.is_group = false;
				child_row.is_expanded = false;

				child_row.cells.push_back("");
				child_row.cells.push_back("");
				child_row.cells.push_back(std::format("Area {} @ ({}, {}, z{})", ci + 1, c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z));
				child_row.cells.push_back("");
				child_row.cells.push_back(format_thousands_separator(c.tile_count));

				rows.push_back(std::move(child_row));
			}
		}
	}

	zone_table->SetRows(std::move(rows));

	// Reselect active zone/cluster
	if (selected_zone_id_ != 0) {
		const std::string target_key = (selected_cluster_ >= 0)
			? std::format("{}:{}", selected_zone_id_, selected_cluster_)
			: std::to_string(selected_zone_id_);
		zone_table->SelectByKey(target_key);
	}
}

void ZonePalettePanel::OnSearchChange(wxCommandEvent& /*event*/) {
	OnUpdate();
}

void ZonePalettePanel::OnSelectZone(int /*index*/, const TableRow& /*row*/) {
	const auto ref = selectedRow();
	selected_zone_id_ = ref.zone;
	selected_cluster_ = ref.cluster;

	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(ref.zone);
	}
	g_gui.SelectBrush();

	if (supportsZones() && map && ref.zone != 0) {
		if (ref.cluster >= 0) {
			const auto& clusters = map->getZoneClusters(ref.zone);
			if (static_cast<size_t>(ref.cluster) < clusters.size()) {
				const auto& region = clusters[static_cast<size_t>(ref.cluster)];
				g_gui.SetStatusText(wxstr(std::format("Zone {} Area {}/{}: {} tiles @ ({}, {}, z{})",
					ref.zone, ref.cluster + 1, clusters.size(), region.tile_count,
					region.centroid_pos.x, region.centroid_pos.y, region.centroid_pos.z)));
			}
		} else {
			const auto& clusters = map->getZoneClusters(ref.zone);
			const size_t total_tiles = map->getZoneTilePositions(ref.zone).size();
			if (clusters.size() > 1) {
				g_gui.SetStatusText(wxstr(std::format("Zone {}: {} tiles across {} area(s)",
					ref.zone, total_tiles, clusters.size())));
			} else {
				g_gui.SetStatusText(wxstr(std::format("Zone {}: {} tiles",
					ref.zone, total_tiles)));
			}
		}
	}
}

void ZonePalettePanel::OnActivateZone(int /*index*/, const TableRow& /*row*/) {
	wxCommandEvent unused;
	OnGoToZone(unused);
}

void ZonePalettePanel::OnSortZones(int /*column*/, bool /*ascending*/) {
	OnUpdate();
}

void ZonePalettePanel::OnToggleGroup(int /*index*/, const TableRow& row) {
	const auto colon = row.key.find(':');
	const uint16_t zid = (colon != std::string::npos)
		? static_cast<uint16_t>(std::strtoul(row.key.substr(0, colon).c_str(), nullptr, 10))
		: static_cast<uint16_t>(std::strtoul(row.key.c_str(), nullptr, 10));

	if (zid != 0) {
		if (expanded_zones_.contains(zid)) {
			expanded_zones_.erase(zid);
		} else {
			expanded_zones_.insert(zid);
		}
		OnUpdate();
	}
}

void ZonePalettePanel::OnZoneContextMenu(int /*index*/, const TableRow* row, const wxPoint& /*pos*/) {
	if (!supportsZones()) {
		return;
	}

	wxMenu menu;
	if (row && !row->key.empty()) {
		const auto ref = selectedRow();
		if (ref.zone != 0) {
			menu.Append(PALETTE_ZONE_GOTO, "Go to area (double-click)")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MAP_PIN, wxSize(16, 16)));
			menu.AppendSeparator();
			menu.Append(PALETTE_ZONE_RENAME, "Rename...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
			menu.Append(PALETTE_ZONE_REMOVE, "Remove...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
			menu.AppendSeparator();
		}
	}

	menu.Append(PALETTE_ZONE_ADD, "Add zone...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	PopupMenu(&menu);
}

void ZonePalettePanel::OnGoToZone(wxCommandEvent& /*event*/) {
	const RowRef row = supportsZones() ? selectedRow() : RowRef{};
	if (row.zone == 0 || !map) {
		return;
	}
	wxBusyCursor busy;
	const auto& clusters = map->getZoneClusters(row.zone);
	if (!clusters.empty()) {
		if (row.cluster >= 0 && static_cast<size_t>(row.cluster) < clusters.size()) {
			const auto& region = clusters[static_cast<size_t>(row.cluster)];
			g_gui.SetScreenCenterPosition(region.centroid_pos);
			g_gui.SetStatusText(wxstr(std::format("Zone {} Area {}/{}: {} tiles @ ({}, {}, z{})",
				row.zone, row.cluster + 1, clusters.size(), region.tile_count,
				region.centroid_pos.x, region.centroid_pos.y, region.centroid_pos.z)));
		} else {
			const auto it = std::ranges::max_element(clusters, {}, &ZoneCluster::tile_count);
			g_gui.SetScreenCenterPosition(it->centroid_pos);
			const size_t total_tiles = map->getZoneTilePositions(row.zone).size();
			g_gui.SetStatusText(wxstr(std::format("Zone {}: {} tiles across {} area(s) @ ({}, {}, z{})",
				row.zone, total_tiles, clusters.size(), it->centroid_pos.x, it->centroid_pos.y, it->centroid_pos.z)));
		}
	} else {
		g_gui.SetStatusText("This zone has no tiles yet.");
	}
}

void ZonePalettePanel::OnClickAdd(wxCommandEvent& /*event*/) {
	if (!supportsZones() || !map) {
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
	selected_zone_id_ = id;
	selected_cluster_ = -1;
	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(id);
	}
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
	if (Editor* editor = g_gui.GetCurrentEditor()) {
		if (editor->actionQueue) {
			editor->actionQueue->clear();
		}
	}
	map->removeZone(id);
	if (selected_zone_id_ == id) {
		selected_zone_id_ = 0;
		selected_cluster_ = -1;
	}
	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(0);
	}
	OnUpdate();
	g_gui.SelectBrush();
	g_gui.RefreshView();
}

void ZonePalettePanel::UpdateZone(uint16_t /*zone_id*/) {
	OnUpdate();
}

void ZonePalettePanel::UpdateTileCounts() {
	OnUpdate();
}
