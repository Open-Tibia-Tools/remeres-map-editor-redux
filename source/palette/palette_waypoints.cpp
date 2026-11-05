#include "palette/palette_waypoints.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "util/image_manager.h"
#include "util/common.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "rendering/indicators/technical_item_registry.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/managers/brush_manager.h"
#include "map/map.h"
#include "map/tile.h"

#include <nanovg.h>
#include <wx/menu.h>
#include <wx/textdlg.h>
#include <wx/clipbrd.h>
#include <algorithm>
#include <array>
#include <format>
#include <string_view>


namespace {

constexpr int kCardHeight = 44;
constexpr int kCardSpacing = 4;
constexpr int kIconSize = 32;



struct WaypointItem {
	std::string name;
	std::string lowercase_name;
	Position pos;
	std::string formatted_pos;
	size_t original_index = 0;
};

} // namespace

// ============================================================================
// WaypointCanvas
// ============================================================================

class WaypointPalettePanel::WaypointCanvas final : public NanoVGCanvas {
public:
	explicit WaypointCanvas(WaypointPalettePanel* owner) :
		NanoVGCanvas(owner, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
		m_owner(owner) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));

		Bind(wxEVT_SIZE, &WaypointCanvas::OnSize, this);
		Bind(wxEVT_LEFT_DOWN, &WaypointCanvas::OnMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &WaypointCanvas::OnLeftDClick, this);
		Bind(wxEVT_RIGHT_DOWN, &WaypointCanvas::OnContextMenu, this);
		Bind(wxEVT_MOTION, &WaypointCanvas::OnMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &WaypointCanvas::OnMouseLeave, this);
	}

	void SetItems(std::vector<WaypointItem> items) {
		m_items = std::move(items);
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
			return a.original_index < b.original_index;
		});
		RebuildVisibleRows();
	}

	void SetSelectedName(const std::string& name) {
		m_selected_name = name;
		Refresh();
	}

	[[nodiscard]] const std::string& GetSelectedName() const {
		return m_selected_name;
	}

	[[nodiscard]] size_t GetTotalCount() const {
		return m_items.size();
	}

	void RebuildVisibleRows() {
		m_visible_indices.clear();
		for (size_t i = 0; i < m_items.size(); ++i) {
			const auto& item = m_items[i];
			if (m_filter_query.empty() ||
			    item.lowercase_name.find(m_filter_query) != std::string::npos ||
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

		const int dip_6 = FromDIP(6);
		const int dip_8 = FromDIP(8);
		const int dip_11 = FromDIP(11);
		const int dip_13 = FromDIP(13);
		const int index_col_w = FromDIP(rme::ui::grid::GetIndexColumnWidth(m_items.size()));

		if (m_visible_indices.empty()) {
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 13.0f);
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
			nvgText(vg, width * 0.5f, static_cast<float>(scroll_pos + height / 2),
				m_items.empty() ? "No waypoints in map" : "No matching waypoints", nullptr);
			return;
		}

		const int card_w = std::max(1, width - padding * 2);
		const int visible_top = scroll_pos;
		const int visible_bottom = scroll_pos + height;

		for (size_t vi = 0; vi < m_visible_indices.size(); ++vi) {
			const size_t item_idx = m_visible_indices[vi];
			const auto& item = m_items[item_idx];
			const int card_y = padding + static_cast<int>(vi) * (kCardHeight + kCardSpacing);

			if (card_y + kCardHeight < visible_top || card_y > visible_bottom) {
				continue;
			}

			const wxRect card_rect(padding, card_y, card_w, kCardHeight);
			const bool is_selected = (item.name == m_selected_name);
			const bool is_hovered = (static_cast<int>(vi) == m_hover_row);

			const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, card_rect, is_selected, is_hovered, 0.0f, false);
			const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

			const wxColour meta_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
			const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);

			// 1. Index badge (#1, #2) with 14px font matching Search Results
			std::array<char, 32> index_buf;
			auto res = std::format_to_n(index_buf.data(), index_buf.size() - 1, "#{}", item.original_index + 1);
			*res.out = '\0';

			const int index_x = drawn.x + dip_6;
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, static_cast<float>(index_x), center_y, index_buf.data(), nullptr);

			// 2. 32x32 Sunken Icon slot with Waypoint sprite tile badge
			const int icon_x = index_x + index_col_w + dip_6;
			const int icon_y = drawn.y + (drawn.height - kIconSize) / 2;
			const wxRect slot_rect(icon_x, icon_y, kIconSize, kIconSize);
			rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);
			rme::ui::grid::DrawCellIcon(vg, slot_rect, 0, rme::rendering::TileIndicatorType::Waypoint);

			// 3. Name on top (standard white text) and Position underneath
			const float text_x = static_cast<float>(icon_x + kIconSize + dip_8);
			const float top_y = static_cast<float>(drawn.y + dip_13);
			const float bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);
			const float avail_w = std::max(20.0f, static_cast<float>(drawn.x + drawn.width - dip_8) - text_x);

			nvgSave(vg);
			nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));

			// Top line: Name in standard white text
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(text_wx));
			nvgText(vg, text_x, top_y, item.name.c_str(), nullptr);

			// Bottom line: Position
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
					return a.original_index < b.original_index;
				});
			} else {
				std::ranges::sort(m_items, [](const auto& a, const auto& b) {
					return a.original_index > b.original_index;
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
			m_selected_name = m_items[item_idx].name;
			Refresh();

			if (m_owner) {
				m_owner->SelectWaypointByName(m_selected_name);
			}
		}
	}

	void OnLeftDClick(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			const auto& item = m_items[item_idx];
			if (item.pos != Position() && g_gui.IsEditorOpen()) {
				g_gui.SetScreenCenterPosition(item.pos);
			}
		}
	}

	void OnContextMenu(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			m_selected_name = m_items[item_idx].name;
			Refresh();
			if (m_owner) {
				m_owner->SelectWaypointByName(m_selected_name);
			}
		}
		ShowContextMenu(event.GetPosition());
	}

	void ShowContextMenu(const wxPoint& pos) {
		wxMenu menu;
		if (!m_selected_name.empty()) {
			menu.Append(wxID_FORWARD, "Go to Waypoint (Double-Click)")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
			menu.Append(wxID_COPY, "Copy Position")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_COPY));
			menu.AppendSeparator();
			menu.Append(wxID_EDIT, "Rename Waypoint...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
			menu.Append(wxID_DELETE, "Remove Waypoint")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
			menu.AppendSeparator();
		}
		menu.Append(wxID_ADD, "Add Waypoint...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent& ev) {
			if (!m_owner) return;
			switch (ev.GetId()) {
				case wxID_FORWARD:
					m_owner->OnGoToWaypoint();
					break;
				case wxID_COPY: {
					auto* wp = m_owner->GetSelectedWaypoint();
					if (wp && wxTheClipboard->Open()) {
						const std::string pos_str = std::format("{}, {}, {}", wp->pos.x, wp->pos.y, wp->pos.z);
						wxTheClipboard->SetData(new wxTextDataObject(wxstr(pos_str)));
						wxTheClipboard->Close();
						g_gui.SetStatusText("Waypoint position copied to clipboard.");
					}
					break;
				}
				case wxID_EDIT:
					m_owner->OnClickRenameWaypoint();
					break;
				case wxID_DELETE:
					m_owner->OnClickRemoveWaypoint();
					break;
				case wxID_ADD:
					m_owner->OnClickAddWaypoint();
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
			SetToolTip(wxstr(std::format("{} | {}", item.name, item.formatted_pos)));
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

	WaypointPalettePanel* m_owner = nullptr;
	std::vector<WaypointItem> m_items;
	std::vector<size_t> m_visible_indices;
	std::string m_filter_query;
	std::string m_selected_name;
	int m_hover_row = -1;
	bool m_has_sort = false;
	TilesetSortKey m_sort_key = TilesetSortKey::Name;
	TilesetSortDirection m_sort_dir = TilesetSortDirection::Ascending;
};

// ============================================================================
// WaypointPalettePanel
// ============================================================================

WaypointPalettePanel::WaypointPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Waypoints");

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

	// 2. Modern Waypoint Card Canvas
	canvas_ = new WaypointCanvas(this);
	box_sizer->Add(canvas_, 1, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 3. Action Button Bar (Add, Rename, Remove)
	auto* button_sizer = new wxBoxSizer(wxHORIZONTAL);
	const int dip_2 = FromDIP(2);

	add_button_ = new wxButton(this, wxID_ANY, "Add", wxDefaultPosition, wxSize(FromDIP(50), -1));
	add_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	add_button_->SetFont(Theme::GetFont(9, false));
	add_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickAddWaypoint(); });
	button_sizer->Add(add_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	rename_button_ = new wxButton(this, wxID_ANY, "Rename", wxDefaultPosition, wxSize(FromDIP(60), -1));
	rename_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
	rename_button_->SetFont(Theme::GetFont(9, false));
	rename_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRenameWaypoint(); });
	button_sizer->Add(rename_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	remove_button_ = new wxButton(this, wxID_ANY, "Remove", wxDefaultPosition, wxSize(FromDIP(60), -1));
	remove_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	remove_button_->SetFont(Theme::GetFont(9, false));
	remove_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRemoveWaypoint(); });
	button_sizer->Add(remove_button_, 1, wxEXPAND);

	box_sizer->Add(button_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	root_sizer->Add(box_sizer, 1, wxEXPAND);
	SetSizer(root_sizer);
	Layout();
}

WaypointPalettePanel::~WaypointPalettePanel() = default;

wxString WaypointPalettePanel::GetName() const {
	return "Waypoint";
}

void WaypointPalettePanel::SelectFirstBrush() {
	if (map_ && !map_->waypoints.empty()) {
		SelectWaypointByName(map_->waypoints.begin()->first);
	}
}

Brush* WaypointPalettePanel::GetSelectedBrush() const {
	if (!g_brush_manager.waypoint_brush) {
		return nullptr;
	}
	Waypoint* wp = GetSelectedWaypoint();
	g_brush_manager.waypoint_brush->setWaypoint(wp);
	return g_brush_manager.waypoint_brush;
}

int WaypointPalettePanel::GetSelectedBrushSize() const {
	return 1;
}

bool WaypointPalettePanel::SelectBrush(const Brush* whatbrush) {
	return g_brush_manager.waypoint_brush && whatbrush == g_brush_manager.waypoint_brush;
}

void WaypointPalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
}

void WaypointPalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void WaypointPalettePanel::SetMap(Map* map) {
	if (map_ != map) {
		selected_waypoint_name_.clear();
		if (g_brush_manager.waypoint_brush) {
			g_brush_manager.waypoint_brush->setWaypoint(nullptr);
		}
	}
	map_ = map;
	Enable(map_ != nullptr);
	OnUpdate();
}

void WaypointPalettePanel::OnUpdate() {
	if (!map_) {
		if (canvas_) canvas_->SetItems({});
		if (add_button_) add_button_->Enable(false);
		if (rename_button_) rename_button_->Enable(false);
		if (remove_button_) remove_button_->Enable(false);
		return;
	}

	if (add_button_) add_button_->Enable(true);

	std::vector<WaypointItem> items;
	size_t index = 0;
	for (const auto& [name, wp] : map_->waypoints) {
		if (!wp) continue;
		items.push_back(WaypointItem {
			.name = wp->name,
			.lowercase_name = as_lower_str(wp->name),
			.pos = wp->pos,
			.formatted_pos = (wp->pos != Position())
				? std::format("Position: [{}, {}, {}]", wp->pos.x, wp->pos.y, wp->pos.z)
				: "Position: Unplaced",
			.original_index = index++
		});
	}

	if (canvas_) {
		canvas_->SetItems(std::move(items));
		canvas_->SetSelectedName(selected_waypoint_name_);
	}

	const bool has_selection = (GetSelectedWaypoint() != nullptr);
	if (rename_button_) rename_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

void WaypointPalettePanel::SelectWaypointByName(const std::string& name) {
	selected_waypoint_name_ = name;
	if (map_ && g_brush_manager.waypoint_brush) {
		Waypoint* wp = map_->waypoints.getWaypoint(name);
		g_brush_manager.waypoint_brush->setWaypoint(wp);
		g_gui.SelectBrush();
	}
	const bool has_selection = (GetSelectedWaypoint() != nullptr);
	if (rename_button_) rename_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

Waypoint* WaypointPalettePanel::GetSelectedWaypoint() const {
	if (!map_ || selected_waypoint_name_.empty()) {
		return nullptr;
	}
	return map_->waypoints.getWaypoint(selected_waypoint_name_);
}

void WaypointPalettePanel::OnClickAddWaypoint() {
	if (!map_) return;

	const std::string name = nstr(wxGetTextFromUser("Waypoint name:", "Add Waypoint", wxEmptyString, this).Trim().Trim(false));
	if (name.empty()) return;

	if (map_->waypoints.getWaypoint(name)) {
		g_gui.SetStatusText("A waypoint with this name already exists.");
		return;
	}

	auto wp = std::make_unique<Waypoint>();
	wp->name = name;
	map_->waypoints.addWaypoint(std::move(wp));
	map_->doChange();

	SelectWaypointByName(name);
	OnUpdate();
}

void WaypointPalettePanel::OnClickRenameWaypoint() {
	Waypoint* wp = GetSelectedWaypoint();
	if (!map_ || !wp) return;

	const std::string old_name = wp->name;
	const std::string new_name = nstr(wxGetTextFromUser("New waypoint name:", "Rename Waypoint", wxstr(old_name), this).Trim().Trim(false));
	if (new_name.empty() || new_name == old_name) return;

	if (map_->waypoints.getWaypoint(new_name)) {
		g_gui.SetStatusText("A waypoint with this name already exists.");
		return;
	}

	if (!map_->waypoints.renameWaypoint(old_name, new_name)) {
		g_gui.SetStatusText("Failed to rename waypoint.");
		return;
	}

	SelectWaypointByName(new_name);
	OnUpdate();
}

void WaypointPalettePanel::OnClickRemoveWaypoint() {
	Waypoint* wp = GetSelectedWaypoint();
	if (!map_ || !wp) return;

	const std::string name = wp->name;
	RemoveWaypointFromMap(wp);
	map_->waypoints.removeWaypoint(name);
	map_->doChange();

	if (selected_waypoint_name_ == name) {
		selected_waypoint_name_.clear();
		if (g_brush_manager.waypoint_brush) {
			g_brush_manager.waypoint_brush->setWaypoint(nullptr);
		}
	}
	OnUpdate();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnGoToWaypoint() {
	Waypoint* wp = GetSelectedWaypoint();
	if (wp && wp->pos != Position() && g_gui.IsEditorOpen()) {
		g_gui.SetScreenCenterPosition(wp->pos);
	}
}

void WaypointPalettePanel::RemoveWaypointFromMap(Waypoint* wp) {
	if (!map_ || !wp) return;
	if (wp->pos != Position() && map_->getTile(wp->pos)) {
		map_->getTileL(wp->pos)->decreaseWaypointCount();
	}
}

