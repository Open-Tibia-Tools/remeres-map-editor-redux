#include "palette/town/town_palette.h"
#include "palette/town/edit_town_dialog.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/map_tab.h"
#include "util/image_manager.h"
#include "util/common.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "rendering/indicators/technical_item_registry.h"
#include "brushes/town/town_temple_brush.h"
#include "brushes/managers/brush_manager.h"
#include "map/map.h"
#include "game/house.h"

#include <nanovg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/evtloop.h>
#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <unordered_map>


namespace {

constexpr int kCardHeight = 44;
constexpr int kCardSpacing = 4;
constexpr int kIconSize = 32;



struct TownItem {
	uint32_t id = 0;
	std::string name;
	std::string lowercase_name;
	Position pos;
	std::string formatted_pos;
	size_t house_count = 0;
	std::string house_count_str;
	size_t original_index = 0;
};

} // namespace

// ============================================================================
// TownCanvas
// ============================================================================

class TownPalettePanel::TownCanvas final : public NanoVGCanvas {
public:
	explicit TownCanvas(TownPalettePanel* owner) :
		NanoVGCanvas(owner, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
		m_owner(owner) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));

		Bind(wxEVT_SIZE, &TownCanvas::OnSize, this);
		Bind(wxEVT_LEFT_DOWN, &TownCanvas::OnMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &TownCanvas::OnLeftDClick, this);
		Bind(wxEVT_RIGHT_DOWN, &TownCanvas::OnContextMenu, this);
		Bind(wxEVT_MOTION, &TownCanvas::OnMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &TownCanvas::OnMouseLeave, this);
	}

	void SetItems(std::vector<TownItem> items) {
		m_items = std::move(items);
		if (m_has_sort) {
			ApplySort();
		}
		RebuildVisibleRows();
	}

	void SetFilter(const std::string& query) {
		m_filter_query = as_lower_str(query);
		RebuildVisibleRows();
	}

	void SetSort(TilesetSortKey key, TilesetSortDirection dir) {
		m_sort_key = key;
		m_sort_dir = dir;
		m_has_sort = true;
		ApplySort();
		RebuildVisibleRows();
	}

	void ClearSort() {
		m_has_sort = false;
		std::ranges::sort(m_items, [](const auto& a, const auto& b) {
			return a.id < b.id;
		});
		RebuildVisibleRows();
	}

	void SetSelectedId(uint32_t id) {
		m_selected_id = id;
		Refresh();
	}

	[[nodiscard]] uint32_t GetSelectedId() const {
		return m_selected_id;
	}

	void RebuildVisibleRows() {
		m_visible_indices.clear();
		for (size_t i = 0; i < m_items.size(); ++i) {
			const auto& item = m_items[i];
			if (m_filter_query.empty() ||
			    item.lowercase_name.find(m_filter_query) != std::string::npos ||
			    std::to_string(item.id).find(m_filter_query) != std::string::npos ||
			    item.formatted_pos.find(m_filter_query) != std::string::npos) {
				m_visible_indices.push_back(i);
			}
		}

		const int padding = rme::ui::grid::kPadding;
		const int total_h = static_cast<int>(m_visible_indices.size()) * (kCardHeight + kCardSpacing) + padding * 2;
		UpdateScrollbar(total_h);
		Refresh();
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override {
		const int scroll_pos = GetScrollPosition();
		const int padding = rme::ui::grid::kPadding;

		if (m_visible_indices.empty()) {
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 13.0f);
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
			nvgText(vg, width * 0.5f, static_cast<float>(scroll_pos + height / 2),
				m_items.empty() ? "No towns defined in map" : "No matching towns found", nullptr);
			return;
		}

		const int card_w = std::max(1, width - padding * 2);
		const int visible_top = scroll_pos;
		const int visible_bottom = scroll_pos + height;

		const int dip_6 = FromDIP(6);
		const int dip_8 = FromDIP(8);
		const int dip_10 = FromDIP(10);
		const int dip_13 = FromDIP(13);
		const int dip_11 = FromDIP(11);
		const int dip_85 = FromDIP(85);
		const int index_col_w = FromDIP(rme::ui::grid::GetIndexColumnWidth(m_items.size()));

		for (size_t vi = 0; vi < m_visible_indices.size(); ++vi) {
			const size_t item_idx = m_visible_indices[vi];
			const auto& item = m_items[item_idx];
			const int card_y = padding + static_cast<int>(vi) * (kCardHeight + kCardSpacing);

			if (card_y + kCardHeight < visible_top || card_y > visible_bottom) {
				continue;
			}

			const wxRect card_rect(padding, card_y, card_w, kCardHeight);
			const bool is_selected = (item.id == m_selected_id);
			const bool is_hovered = (static_cast<int>(vi) == m_hover_row);

			const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, card_rect, is_selected, is_hovered, 0.0f, false);

			const wxColour meta_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
			const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);

			// 1. Index badge (#1, #2) with 14px font matching Search Results
			std::array<char, 32> id_buf;
			auto res = std::format_to_n(id_buf.data(), id_buf.size() - 1, "#{}", item.original_index + 1);
			*res.out = '\0';

			const int index_x = drawn.x + dip_6;
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, static_cast<float>(index_x), static_cast<float>(drawn.y + drawn.height / 2), id_buf.data(), nullptr);

			// 2. 32x32 Sunken Icon slot with TownTemple sprite indicator tile
			const int icon_x = index_x + index_col_w + dip_6;
			const int icon_y = drawn.y + (drawn.height - kIconSize) / 2;
			const wxRect slot_rect(icon_x, icon_y, kIconSize, kIconSize);
			rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);
			rme::ui::grid::DrawCellIcon(vg, slot_rect, 0, rme::rendering::TileIndicatorType::TownTemple);

			// 3. Right side: House count formatted like Search Results (14px font)
			const float badge_w = static_cast<float>(dip_85);
			const float badge_x = static_cast<float>(drawn.x + drawn.width - dip_8);
			std::array<char, 64> count_buf;
			auto count_res = std::format_to_n(count_buf.data(), count_buf.size() - 1, "({} {})",
				item.house_count, item.house_count == 1 ? "house" : "houses");
			*count_res.out = '\0';

			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, badge_x, static_cast<float>(drawn.y + drawn.height / 2), count_buf.data(), nullptr);

			// 4. Center text: Name in Yellow like tooltip on top, Coordinates underneath
			const float text_x = static_cast<float>(icon_x + kIconSize + dip_8);
			const float top_y = static_cast<float>(drawn.y + dip_13);
			const float bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);
			const float avail_w = std::max(20.0f, badge_x - text_x - badge_w - static_cast<float>(dip_10));

			nvgSave(vg);
			nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));

			// Top line: Name in standard white text
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(text_wx));
			nvgText(vg, text_x, top_y, item.name.c_str(), nullptr);

			// Bottom line: Temple Coordinates
			nvgFontSize(vg, 12.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, text_x, bottom_y, item.formatted_pos.c_str(), nullptr);

			nvgRestore(vg);
		}
	}

private:
	void ApplySort() {
		if (m_sort_key == TilesetSortKey::Name) {
			if (m_sort_dir == TilesetSortDirection::Ascending) {
				std::ranges::sort(m_items, [](const auto& a, const auto& b) {
					return a.name < b.name;
				});
			} else {
				std::ranges::sort(m_items, [](const auto& a, const auto& b) {
					return a.name > b.name;
				});
			}
		} else if (m_sort_key == TilesetSortKey::ID) {
			if (m_sort_dir == TilesetSortDirection::Ascending) {
				std::ranges::sort(m_items, [](const auto& a, const auto& b) {
					return a.id < b.id;
				});
			} else {
				std::ranges::sort(m_items, [](const auto& a, const auto& b) {
					return a.id > b.id;
				});
			}
		}
	}

	void OnSize(wxSizeEvent& event) {
		RebuildVisibleRows();
		event.Skip();
	}

	[[nodiscard]] int HitTest(int mouse_y) const {
		if (m_visible_indices.empty()) {
			return -1;
		}
		const int padding = rme::ui::grid::kPadding;
		const int content_y = mouse_y + GetScrollPosition() - padding;
		if (content_y < 0) {
			return -1;
		}
		const int row = content_y / (kCardHeight + kCardSpacing);
		const int offset_in_row = content_y % (kCardHeight + kCardSpacing);
		if (row >= 0 && row < static_cast<int>(m_visible_indices.size()) && offset_in_row < kCardHeight) {
			return row;
		}
		return -1;
	}

	void OnMouseDown(wxMouseEvent& event) {
		SetFocus();
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			m_selected_id = m_items[item_idx].id;
			Refresh();

			if (m_owner) {
				m_owner->SelectTownById(m_selected_id);
			}
		}
	}

	void OnLeftDClick(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			const auto& item = m_items[item_idx];
			if (item.pos.isValid() && g_gui.IsEditorOpen()) {
				g_gui.SetScreenCenterPosition(item.pos);
			}
		}
	}

	void OnContextMenu(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			m_selected_id = m_items[item_idx].id;
			Refresh();
			if (m_owner) {
				m_owner->SelectTownById(m_selected_id);
			}
		}
		ShowContextMenu(event.GetPosition());
	}

	void ShowContextMenu(const wxPoint& pos) {
		wxMenu menu;
		if (m_selected_id != 0) {
			menu.Append(wxID_FORWARD, "Go to Temple (Double-Click)")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
			menu.Append(wxID_FIND, "Set Temple Position")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CROSSHAIRS));
			menu.Append(wxID_EDIT, "Edit Town...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
			menu.AppendSeparator();
			menu.Append(wxID_DELETE, "Remove Town")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
			menu.AppendSeparator();
		}
		menu.Append(wxID_ADD, "Add Town...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent& ev) {
			if (!m_owner) return;
			switch (ev.GetId()) {
				case wxID_FORWARD:
					m_owner->OnGoToTemple();
					break;
				case wxID_FIND:
					m_owner->OnClickChangeTemple();
					break;
				case wxID_EDIT:
					m_owner->OnClickEditTown();
					break;
				case wxID_DELETE:
					m_owner->OnClickRemoveTown();
					break;
				case wxID_ADD:
					m_owner->OnClickAddTown();
					break;
				default:
					break;
			}
		});

		PopupMenu(&menu, pos);
	}

	void OnMouseMove(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit != m_hover_row) {
			m_hover_row = hit;
			Refresh();
		}
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const auto& item = m_items[m_visible_indices[hit]];
			SetToolTip(wxstr(std::format("[{}] {} | {} | {}", item.id, item.name, item.formatted_pos, item.house_count_str)));
		} else {
			SetToolTip("");
		}
	}

	void OnMouseLeave(wxMouseEvent& WXUNUSED(event)) {
		if (m_hover_row != -1) {
			m_hover_row = -1;
			Refresh();
		}
		SetToolTip("");
	}

	TownPalettePanel* m_owner = nullptr;
	std::vector<TownItem> m_items;
	std::vector<size_t> m_visible_indices;
	std::string m_filter_query;
	uint32_t m_selected_id = 0;
	int m_hover_row = -1;
	bool m_has_sort = false;
	TilesetSortKey m_sort_key = TilesetSortKey::Name;
	TilesetSortDirection m_sort_dir = TilesetSortDirection::Ascending;
};

// ============================================================================
// TownPalettePanel
// ============================================================================

TownPalettePanel::TownPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Towns");

	// 1. TilesetGridToolbar (Filter & Sort)
	TilesetGridToolbar::Actions actions;
	actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		if (canvas_) canvas_->SetSort(key, dir);
	};
	actions.clear_sort = [this]() {
		if (canvas_) canvas_->ClearSort();
	};
	actions.filter = [this](const std::string& q) {
		if (canvas_) canvas_->SetFilter(q);
	};
	actions.filter_cleared = [this]() {
		if (canvas_) canvas_->SetFilter("");
	};

	toolbar_ = new TilesetGridToolbar(this, std::move(actions), TilesetGridToolbar::Part_Sort | TilesetGridToolbar::Part_Search);
	box_sizer->Add(toolbar_, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	// 2. Modern Town Card Canvas
	canvas_ = new TownCanvas(this);
	box_sizer->Add(canvas_, 1, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 3. Action Button Bar (2x2 Grid)
	auto* button_grid = new wxGridSizer(2, 2, FromDIP(4), FromDIP(4));

	add_button_ = new wxButton(this, wxID_ANY, "Add", wxDefaultPosition, wxDefaultSize);
	add_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	add_button_->SetFont(Theme::GetFont(9, false));
	add_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickAddTown(); });
	button_grid->Add(add_button_, 1, wxEXPAND);

	edit_button_ = new wxButton(this, wxID_ANY, "Edit", wxDefaultPosition, wxDefaultSize);
	edit_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
	edit_button_->SetFont(Theme::GetFont(9, false));
	edit_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickEditTown(); });
	button_grid->Add(edit_button_, 1, wxEXPAND);

	remove_button_ = new wxButton(this, wxID_ANY, "Remove", wxDefaultPosition, wxSize(FromDIP(60), -1));
	remove_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	remove_button_->SetFont(Theme::GetFont(9, false));
	remove_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRemoveTown(); });
	button_grid->Add(remove_button_, 1, wxEXPAND);

	change_temple_button_ = new wxButton(this, wxID_ANY, "Set Temple", wxDefaultPosition, wxDefaultSize);
	change_temple_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CROSSHAIRS));
	change_temple_button_->SetFont(Theme::GetFont(9, false));
	change_temple_button_->SetToolTip("Click on map to place temple position for selected town");
	change_temple_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickChangeTemple(); });
	button_grid->Add(change_temple_button_, 1, wxEXPAND);

	box_sizer->Add(button_grid, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	root_sizer->Add(box_sizer, 1, wxEXPAND);
	SetSizer(root_sizer);
	Layout();
}

TownPalettePanel::~TownPalettePanel() = default;

wxString TownPalettePanel::GetName() const {
	return "Town";
}

void TownPalettePanel::SelectFirstBrush() {
	if (map_ && !map_->towns.empty()) {
		SelectTownById(map_->towns.begin()->first);
	}
}

Brush* TownPalettePanel::GetSelectedBrush() const {
	return nullptr;
}

int TownPalettePanel::GetSelectedBrushSize() const {
	return 0;
}

bool TownPalettePanel::SelectBrush(const Brush* whatbrush) {
	return g_brush_manager.temple_brush && whatbrush == g_brush_manager.temple_brush;
}

void TownPalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
}

void TownPalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void TownPalettePanel::SetMap(Map* map) {
	if (map_ != map) {
		selected_town_id_ = 0;
		if (g_brush_manager.temple_brush) {
			g_brush_manager.temple_brush->setTown(nullptr);
		}
	}
	map_ = map;
	Enable(map_ != nullptr);
	OnUpdate();
}

void TownPalettePanel::OnUpdate() {
	if (!map_) {
		if (canvas_) canvas_->SetItems({});
		if (add_button_) add_button_->Enable(false);
		if (edit_button_) edit_button_->Enable(false);
		if (remove_button_) remove_button_->Enable(false);
		if (change_temple_button_) change_temple_button_->Enable(false);
		return;
	}

	if (add_button_) add_button_->Enable(true);

	// Precompute house counts per town
	std::unordered_map<uint32_t, size_t> house_counts;
	for (const auto& [_, house] : map_->houses) {
		if (house) {
			house_counts[house->townid]++;
		}
	}

	std::vector<TownItem> items;
	size_t orig_idx = 0;
	for (const auto& [id, town] : map_->towns) {
		if (!town) continue;
		const Position& pos = town->getTemplePosition();
		const size_t hc = house_counts[id];

		items.push_back(TownItem {
			.id = id,
			.name = town->getName(),
			.lowercase_name = as_lower_str(town->getName()),
			.pos = pos,
			.formatted_pos = (pos != Position() && pos.isValid())
				? std::format("Temple: [{}, {}, {}]", pos.x, pos.y, pos.z)
				: "Temple: Unplaced",
			.house_count = hc,
			.house_count_str = std::format("{} {}", hc, hc == 1 ? "house" : "houses"),
			.original_index = orig_idx++
		});
	}

	if (canvas_) {
		canvas_->SetItems(std::move(items));
		canvas_->SetSelectedId(selected_town_id_);
	}

	const bool has_selection = (GetSelectedTown() != nullptr);
	if (edit_button_) edit_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
	if (change_temple_button_) change_temple_button_->Enable(has_selection);
}

void TownPalettePanel::SelectTownById(uint32_t town_id) {
	selected_town_id_ = town_id;
	const bool has_selection = (GetSelectedTown() != nullptr);
	if (edit_button_) edit_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
	if (change_temple_button_) change_temple_button_->Enable(has_selection);
}

Town* TownPalettePanel::GetSelectedTown() const {
	if (!map_ || selected_town_id_ == 0) {
		return nullptr;
	}
	return map_->towns.getTown(selected_town_id_);
}

bool TownPalettePanel::RunEditTownModal(wxWindow* parent, Map* map, Town* town, bool is_new) {
	if (!map || !town) {
		return false;
	}

	const uint32_t saved_town_id = town->getID();
	EditTownDialog dlg(parent, map, town, is_new);

	while (true) {
		const int ret = dlg.ShowModal();
		if (ret == EditTownDialog::DIALOG_PICK_TEMPLE) {
			dlg.Hide();

			bool picked = false;
			Position picked_pos;
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

			MapTab* mapTab = g_gui.GetCurrentMapTab();
			if (!mapTab || mapTab->GetMap() != map) {
				return false;
			}

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

void TownPalettePanel::OnClickAddTown() {
	if (!map_) return;

	uint32_t next_id = map_->towns.getEmptyID();
	MapTab* mapTab = g_gui.GetCurrentMapTab();
	Position default_pos = mapTab ? mapTab->GetScreenCenterPosition() : Position();
	if (!default_pos.isValid()) {
		default_pos = Position(map_->getWidth() / 2, map_->getHeight() / 2, 7);
	}

	auto new_town = std::make_unique<Town>(next_id);
	new_town->setName(std::format("Town {}", next_id));
	new_town->setTemplePosition(default_pos);

	if (RunEditTownModal(this, map_, new_town.get(), true)) {
		Town* raw_ptr = new_town.get();
		map_->towns.addTown(std::move(new_town));
		SelectTownById(raw_ptr->getID());
		OnUpdate();
	}
}

void TownPalettePanel::OnClickEditTown() {
	Town* town = GetSelectedTown();
	if (!map_ || !town) return;

	if (RunEditTownModal(this, map_, town, false)) {
		OnUpdate();
	}
}

void TownPalettePanel::OnClickRemoveTown() {
	Town* town = GetSelectedTown();
	if (!map_ || !town) return;

	const uint32_t town_id = town->getID();
	const std::string town_name = town->getName();

	int house_count = 0;
	for (const auto& [_, house] : map_->houses) {
		if (house && house->townid == town_id) {
			house_count++;
		}
	}

	wxString confirm_msg = wxstr(std::format("Are you sure you want to delete town '{}' (ID: {})?", town_name, town_id));
	if (house_count > 0) {
		confirm_msg += wxstr(std::format("\nWarning: {} house(s) belong to this town and will lose their town assignment.", house_count));
	}

	if (wxMessageBox(confirm_msg, "Delete Town", wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}

	for (auto& [_, house] : map_->houses) {
		if (house && house->townid == town_id) {
			house->townid = 0;
		}
	}

	Position temple_pos = town->getTemplePosition();
	if (temple_pos != Position()) {
		if (auto* tile = map_->getTile(temple_pos)) {
			if (auto* loc = tile->getLocation()) {
				loc->decreaseTownCount();
			}
		}
		if (temple_pos.isValid()) {
			map_->getChangeTracker().markTileDirty(temple_pos);
			g_minimap.MarkTileDirty(*map_, temple_pos);
		}
	}

	map_->towns.removeTown(town);
	if (selected_town_id_ == town_id) {
		selected_town_id_ = 0;
	}
	map_->doChange();
	OnUpdate();
	g_gui.RefreshPalettes();
	g_gui.RefreshView();
}

void TownPalettePanel::OnClickChangeTemple() {
	Town* town = GetSelectedTown();
	if (!map_ || !town || !g_brush_manager.temple_brush) return;

	g_brush_manager.temple_brush->setTown(town);
	g_gui.SelectBrush(g_brush_manager.temple_brush);
	g_gui.SetStatusText(wxstr(std::format("Click on the map to set temple position for town '{}' (ID: {})...", town->getName(), town->getID())));
}

void TownPalettePanel::OnGoToTemple() {
	Town* town = GetSelectedTown();
	if (town && town->getTemplePosition().isValid() && g_gui.IsEditorOpen()) {
		g_gui.SetScreenCenterPosition(town->getTemplePosition());
	}
}

