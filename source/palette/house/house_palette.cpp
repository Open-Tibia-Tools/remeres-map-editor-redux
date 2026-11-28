#include "palette/house/house_palette.h"
#include "palette/house/edit_house_dialog.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/dialog_util.h"
#include "util/image_manager.h"
#include "util/common.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "rendering/indicators/technical_item_registry.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/managers/brush_manager.h"
#include "map/map.h"
#include "game/house.h"
#include "game/town.h"

#include <nanovg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <algorithm>
#include <array>
#include <format>
#include <string_view>


namespace {

constexpr int kCardHeight = 44;
constexpr int kCardSpacing = 4;
constexpr int kIconSize = 32;

inline std::string FormatNumberWithCommas(uint64_t val) {
	std::string s = std::to_string(val);
	int insert_pos = static_cast<int>(s.length()) - 3;
	while (insert_pos > 0) {
		s.insert(static_cast<size_t>(insert_pos), ",");
		insert_pos -= 3;
	}
	return s;
}

struct HouseItem {
	uint32_t id = 0;
	std::string name;
	std::string lowercase_name;
	uint32_t town_id = 0;
	bool is_guildhall = false;
	uint32_t rent = 0;
	size_t size = 0;
	Position exit_pos;
	std::string subtitle;
	std::string size_str;
	size_t original_index = 0;
	std::string formatted_rent;
};

} // namespace

// ============================================================================
// HouseCanvas
// ============================================================================

class HousePalette::HouseCanvas final : public NanoVGCanvas {
public:
	explicit HouseCanvas(HousePalette* owner, wxWindow* parent = nullptr) :
		NanoVGCanvas(parent ? parent : owner, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
		m_owner(owner) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));

		Bind(wxEVT_SIZE, &HouseCanvas::OnSize, this);
		Bind(wxEVT_LEFT_DOWN, &HouseCanvas::OnMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &HouseCanvas::OnLeftDClick, this);
		Bind(wxEVT_RIGHT_DOWN, &HouseCanvas::OnContextMenu, this);
		Bind(wxEVT_MOTION, &HouseCanvas::OnMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &HouseCanvas::OnMouseLeave, this);
	}

	void SetItems(std::vector<HouseItem> items) {
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

	void SetTownFilter(uint32_t town_id, bool filter_no_town, bool all_towns) {
		m_town_filter_id = town_id;
		m_filter_no_town = filter_no_town;
		m_all_towns = all_towns;
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

	void UpdateHouseSize(uint32_t house_id, const Map* map) {
		if (!map) return;
		bool changed = false;
		for (auto& item : m_items) {
			if (house_id == 0 || item.id == house_id) {
				if (const House* h = map->houses.getHouse(item.id)) {
					item.size = h->size();
					item.size_str = std::format("({} sqm)", item.size);
					changed = true;
				}
				if (house_id != 0) break;
			}
		}
		if (changed) {
			Refresh();
		}
	}

	void RebuildVisibleRows() {
		m_visible_indices.clear();
		for (size_t i = 0; i < m_items.size(); ++i) {
			const auto& item = m_items[i];

			// Town filter
			if (!m_all_towns) {
				if (m_filter_no_town) {
					if (item.town_id != 0) continue;
				} else {
					if (item.town_id != m_town_filter_id) continue;
				}
			}

			// Text filter
			if (m_filter_query.empty() ||
			    item.lowercase_name.find(m_filter_query) != std::string::npos ||
			    std::to_string(item.id).find(m_filter_query) != std::string::npos ||
			    (item.is_guildhall && std::string_view("guild").find(m_filter_query) != std::string_view::npos)) {
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
				m_items.empty() ? "No houses found in map" : "No matching houses", nullptr);
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
			const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

			const wxColour meta_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
			const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);
			const wxColour cyan_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : wxColour(0, 206, 209);
			const wxColour yellow_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TooltipTextValue);

			// 1. Index badge (#1, #2) with 14px font matching Search Results
			std::array<char, 32> id_buf;
			auto res = std::format_to_n(id_buf.data(), id_buf.size() - 1, "#{}", item.original_index + 1);
			*res.out = '\0';

			const int index_x = drawn.x + dip_6;
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, static_cast<float>(index_x), center_y, id_buf.data(), nullptr);

			// 2. 32x32 Sunken Icon slot with authentic House sprite indicator tile
			const int icon_x = index_x + index_col_w + dip_6;
			const int icon_y = drawn.y + (drawn.height - kIconSize) / 2;
			const wxRect slot_rect(icon_x, icon_y, kIconSize, kIconSize);
			rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);
			rme::ui::grid::DrawCellIcon(
				vg, slot_rect, 0,
				item.is_guildhall ? rme::rendering::TileIndicatorType::GuildHall : rme::rendering::TileIndicatorType::House
			);

			// 3. Right side: Size column in 14px font matching (matches) in search result
			const float size_x = static_cast<float>(drawn.x + drawn.width - dip_8);
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, size_x, center_y, item.size_str.c_str(), nullptr);

			float size_bounds[4] = {0};
			nvgTextBounds(vg, size_x, center_y, item.size_str.c_str(), nullptr, size_bounds);
			const float size_w = size_bounds[2] - size_bounds[0];

			// 4. Center: Name on top line, ID (cyan) and Rent (yellow/gold with commas) underneath
			const float text_x = static_cast<float>(icon_x + kIconSize + dip_8);
			const float top_y = static_cast<float>(drawn.y + dip_13);
			const float bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);
			const float avail_w = std::max(20.0f, size_x - size_w - text_x - static_cast<float>(dip_10));

			nvgSave(vg);
			nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));

			// Top line: House name
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(text_wx));
			nvgText(vg, text_x, top_y, item.name.c_str(), nullptr);

			// Bottom line: House ID in CYAN, Rent in YELLOW/GOLD next to it with thousand separators
			nvgFontSize(vg, 12.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

			// ID in Cyan
			nvgFillColor(vg, NvgUtils::ToNvColor(cyan_wx));
			nvgText(vg, text_x, bottom_y, item.subtitle.c_str(), nullptr);

			float id_bounds[4] = {0};
			nvgTextBounds(vg, text_x, bottom_y, item.subtitle.c_str(), nullptr, id_bounds);
			float cur_bottom_x = id_bounds[2] + static_cast<float>(dip_8);

			// Bullet separator •
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, cur_bottom_x, bottom_y, "•", nullptr);
			cur_bottom_x += static_cast<float>(dip_8);

			// Rent: 500,000 gp in Yellow/Gold (TooltipTextValue)
			std::array<char, 64> rent_buf;
			auto rent_res = std::format_to_n(rent_buf.data(), rent_buf.size() - 1, "Rent: {} gp", item.formatted_rent);
			*rent_res.out = '\0';

			nvgFillColor(vg, NvgUtils::ToNvColor(yellow_wx));
			nvgText(vg, cur_bottom_x, bottom_y, rent_buf.data(), nullptr);

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
		if (m_visible_indices.empty()) return -1;
		const int padding = rme::ui::grid::kPadding;
		const int content_y = mouse_y + GetScrollPosition() - padding;
		if (content_y < 0) return -1;
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
				m_owner->SelectHouseById(m_selected_id);
			}
		}
	}

	void OnLeftDClick(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_indices.size())) {
			const size_t item_idx = m_visible_indices[hit];
			const auto& item = m_items[item_idx];
			if (item.exit_pos.isValid() && g_gui.IsEditorOpen()) {
				g_gui.SetScreenCenterPosition(item.exit_pos);
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
				m_owner->SelectHouseById(m_selected_id);
			}
		}
		ShowContextMenu(event.GetPosition());
	}

	void ShowContextMenu(const wxPoint& pos) {
		wxMenu menu;
		if (m_selected_id != 0) {
			menu.Append(wxID_FORWARD, "Go to House Exit (Double-Click)")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
			menu.Append(wxID_EDIT, "Edit House...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
			menu.AppendSeparator();
			menu.Append(wxID_DELETE, "Remove House")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
			menu.AppendSeparator();
		}
		menu.Append(wxID_ADD, "Add House...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent& ev) {
			if (!m_owner) return;
			switch (ev.GetId()) {
				case wxID_FORWARD:
					m_owner->OnGoToExit();
					break;
				case wxID_EDIT:
					m_owner->OnClickEditHouse();
					break;
				case wxID_DELETE:
					m_owner->OnClickRemoveHouse();
					break;
				case wxID_ADD:
					m_owner->OnClickAddHouse();
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
			SetToolTip(wxstr(std::format("#{} {}{} | Rent: {} gp | Size: {} sqm", item.id, item.name, item.is_guildhall ? " [Guildhall]" : "", item.rent, item.size)));
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

	HousePalette* m_owner = nullptr;
	std::vector<HouseItem> m_items;
	std::vector<size_t> m_visible_indices;
	std::string m_filter_query;
	uint32_t m_selected_id = 0;
	uint32_t m_town_filter_id = 0;
	bool m_filter_no_town = false;
	bool m_all_towns = true;
	int m_hover_row = -1;
	bool m_has_sort = false;
	TilesetSortKey m_sort_key = TilesetSortKey::Name;
	TilesetSortDirection m_sort_dir = TilesetSortDirection::Ascending;
};

// ============================================================================
// HousePalette
// ============================================================================

HousePalette::HousePalette(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Houses");
	wxWindow* static_box = box_sizer->GetStaticBox();

	// 1. Town filter dropdown
	town_choice_ = new wxChoice(static_box, wxID_ANY);
	town_choice_->Bind(wxEVT_CHOICE, &HousePalette::OnTownFilterChange, this);
	box_sizer->Add(town_choice_, 0, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 2. TilesetGridToolbar (Filter & Sort)
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

	toolbar_ = new TilesetGridToolbar(static_box, std::move(actions), TilesetGridToolbar::Part_Sort | TilesetGridToolbar::Part_Search);
	box_sizer->Add(toolbar_, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	// 3. Modern House Card Canvas
	canvas_ = new HouseCanvas(this, static_box);
	box_sizer->Add(canvas_, 1, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 4. Action buttons (Add, Edit, Remove)
	auto* action_sizer = new wxBoxSizer(wxHORIZONTAL);
	const int dip_2 = FromDIP(2);

	add_button_ = new wxButton(static_box, wxID_ANY, "Add", wxDefaultPosition, wxSize(FromDIP(50), -1));
	add_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	add_button_->SetFont(Theme::GetFont(9, false));
	add_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickAddHouse(); });
	action_sizer->Add(add_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	edit_button_ = new wxButton(static_box, wxID_ANY, "Edit", wxDefaultPosition, wxSize(FromDIP(50), -1));
	edit_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
	edit_button_->SetFont(Theme::GetFont(9, false));
	edit_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickEditHouse(); });
	action_sizer->Add(edit_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	remove_button_ = new wxButton(static_box, wxID_ANY, "Remove", wxDefaultPosition, wxSize(FromDIP(60), -1));
	remove_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	remove_button_->SetFont(Theme::GetFont(9, false));
	remove_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRemoveHouse(); });
	action_sizer->Add(remove_button_, 1, wxEXPAND);

	box_sizer->Add(action_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 5. Brush toggles (House tiles, Select Exit)
	auto* brush_sizer = new wxBoxSizer(wxHORIZONTAL);
	house_brush_toggle_ = new wxToggleButton(static_box, wxID_ANY, "House tiles");
	house_brush_toggle_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PAINTBRUSH));
	house_brush_toggle_->SetFont(Theme::GetFont(9, false));
	house_brush_toggle_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { SelectHouseBrush(); });
	brush_sizer->Add(house_brush_toggle_, 1, wxEXPAND | wxRIGHT, dip_2);

	exit_brush_toggle_ = new wxToggleButton(static_box, wxID_ANY, "Select exit");
	exit_brush_toggle_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_DOOR_OPEN));
	exit_brush_toggle_->SetFont(Theme::GetFont(9, false));
	exit_brush_toggle_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { SelectExitBrush(); });
	brush_sizer->Add(exit_brush_toggle_, 1, wxEXPAND);

	box_sizer->Add(brush_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	root_sizer->Add(box_sizer, 1, wxEXPAND);
	SetSizer(root_sizer);
	Layout();
}

HousePalette::~HousePalette() = default;

wxString HousePalette::GetName() const {
	return "House";
}

void HousePalette::SelectFirstBrush() {
	if (map_ && map_->houses.count() > 0) {
		SelectHouseById(map_->houses.begin()->second->getID());
	}
}

Brush* HousePalette::GetSelectedBrush() const {
	House* house = GetSelectedHouse();
	if (exit_brush_toggle_ && exit_brush_toggle_->GetValue()) {
		if (g_brush_manager.house_exit_brush) {
			g_brush_manager.house_exit_brush->setHouse(house);
			return (g_brush_manager.house_exit_brush->getHouseID() != 0 ? g_brush_manager.house_exit_brush : nullptr);
		}
	} else if (house_brush_toggle_ && house_brush_toggle_->GetValue()) {
		if (g_brush_manager.house_brush) {
			g_brush_manager.house_brush->setHouse(house);
			return (g_brush_manager.house_brush->getHouseID() != 0 ? g_brush_manager.house_brush : nullptr);
		}
	}
	return nullptr;
}

int HousePalette::GetSelectedBrushSize() const {
	return 1;
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

void HousePalette::SelectHouseBrush() {
	if (house_brush_toggle_) house_brush_toggle_->SetValue(true);
	if (exit_brush_toggle_) exit_brush_toggle_->SetValue(false);
	House* house = GetSelectedHouse();
	if (g_brush_manager.house_brush) {
		g_brush_manager.house_brush->setHouse(house);
	}
	g_gui.SelectBrush();
}

void HousePalette::SelectExitBrush() {
	if (house_brush_toggle_) house_brush_toggle_->SetValue(false);
	if (exit_brush_toggle_) exit_brush_toggle_->SetValue(true);
	House* house = GetSelectedHouse();
	if (g_brush_manager.house_exit_brush) {
		g_brush_manager.house_exit_brush->setHouse(house);
	}
	g_gui.SelectBrush();
}

void HousePalette::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
}

void HousePalette::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void HousePalette::SetMap(Map* map) {
	if (map_ != map) {
		selected_house_id_ = 0;
		if (g_brush_manager.house_brush) {
			g_brush_manager.house_brush->setHouse(nullptr);
		}
		if (g_brush_manager.house_exit_brush) {
			g_brush_manager.house_exit_brush->setHouse(nullptr);
		}
	}
	map_ = map;
	Enable(map_ != nullptr);
	OnUpdate();
}

void HousePalette::OnUpdate() {
	if (!map_) {
		if (canvas_) canvas_->SetItems({});
		if (town_choice_) town_choice_->Clear();
		if (add_button_) add_button_->Enable(false);
		if (edit_button_) edit_button_->Enable(false);
		if (remove_button_) remove_button_->Enable(false);
		return;
	}

	if (add_button_) add_button_->Enable(true);

	// Update town choice options
	if (town_choice_) {
		const int cur_sel = town_choice_->GetSelection();
		uint32_t saved_town_id = 0;
		bool saved_no_town = false;
		if (cur_sel != wxNOT_FOUND && town_choice_->GetCount() > 0) {
			if (cur_sel == static_cast<int>(town_choice_->GetCount()) - 1) {
				saved_no_town = true;
			} else if (cur_sel > 0) {
				auto* t = static_cast<Town*>(town_choice_->GetClientData(cur_sel));
				if (t) saved_town_id = t->getID();
			}
		}

		town_choice_->Clear();
		town_choice_->Append("All Towns");
		int reselect_idx = 0;

		for (const auto& [id, town] : map_->towns) {
			if (!town) continue;
			const int idx = town_choice_->Append(wxstr(town->getName()), town.get());
			if (town->getID() == saved_town_id) {
				reselect_idx = idx;
			}
		}
		const int no_town_idx = town_choice_->Append("No Town");
		if (saved_no_town) {
			reselect_idx = no_town_idx;
		}
		town_choice_->SetSelection(reselect_idx);

		if (canvas_) {
			if (reselect_idx == 0) {
				canvas_->SetTownFilter(0, false, true);
			} else if (reselect_idx == no_town_idx) {
				canvas_->SetTownFilter(0, true, false);
			} else {
				auto* town = static_cast<Town*>(town_choice_->GetClientData(reselect_idx));
				canvas_->SetTownFilter(town ? town->getID() : 0, false, false);
			}
		}
	}

	std::vector<HouseItem> items;
	size_t original_idx = 0;
	for (const auto& [id, house] : map_->houses) {
		if (!house) continue;
		items.push_back(HouseItem {
			.id = house->getID(),
			.name = house->name,
			.lowercase_name = as_lower_str(house->name),
			.town_id = house->townid,
			.is_guildhall = house->guildhall,
			.rent = static_cast<uint32_t>(house->rent),
			.size = house->size(),
			.exit_pos = house->getExit(),
			.subtitle = std::format("ID: {}", house->getID()),
			.size_str = std::format("({} sqm)", house->size()),
			.original_index = original_idx++,
			.formatted_rent = FormatNumberWithCommas(static_cast<uint64_t>(house->rent))
		});
	}

	if (canvas_) {
		canvas_->SetItems(std::move(items));
		canvas_->SetSelectedId(selected_house_id_);
	}

	const bool has_selection = (GetSelectedHouse() != nullptr);
	if (edit_button_) edit_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

void HousePalette::OnTownFilterChange(wxCommandEvent& /*event*/) {
	if (!canvas_ || !town_choice_) return;

	const int town_idx = town_choice_->GetSelection();
	if (town_idx == 0 || town_idx == wxNOT_FOUND) {
		canvas_->SetTownFilter(0, false, true);
	} else if (town_idx == static_cast<int>(town_choice_->GetCount()) - 1) {
		canvas_->SetTownFilter(0, true, false);
	} else {
		auto* town = static_cast<Town*>(town_choice_->GetClientData(town_idx));
		canvas_->SetTownFilter(town ? town->getID() : 0, false, false);
	}
}

void HousePalette::UpdateHouseSize(uint32_t house_id) {
	if (!map_ || !canvas_) return;
	canvas_->UpdateHouseSize(house_id, map_);
}

void HousePalette::UpdateHouses() {
	OnUpdate();
}

void HousePalette::SelectHouseById(uint32_t house_id) {
	selected_house_id_ = house_id;
	House* house = GetSelectedHouse();
	if (house_brush_toggle_ && house_brush_toggle_->GetValue() && g_brush_manager.house_brush) {
		g_brush_manager.house_brush->setHouse(house);
		g_gui.SelectBrush();
	} else if (exit_brush_toggle_ && exit_brush_toggle_->GetValue() && g_brush_manager.house_exit_brush) {
		g_brush_manager.house_exit_brush->setHouse(house);
		g_gui.SelectBrush();
	}

	const bool has_selection = (house != nullptr);
	if (edit_button_) edit_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

House* HousePalette::GetSelectedHouse() const {
	if (!map_ || selected_house_id_ == 0) {
		return nullptr;
	}
	return map_->houses.getHouse(selected_house_id_);
}

void HousePalette::OnClickAddHouse() {
	if (!map_) return;

	auto new_house = std::make_unique<House>(*map_);
	new_house->setID(map_->houses.getEmptyID());
	new_house->name = std::format("Unnamed House #{}", new_house->getID());

	if (town_choice_) {
		const int town_idx = town_choice_->GetSelection();
		if (town_idx > 0 && town_idx < static_cast<int>(town_choice_->GetCount()) - 1) {
			auto* town = static_cast<Town*>(town_choice_->GetClientData(town_idx));
			new_house->townid = town ? town->getID() : 0;
		}
	}

	House* house_ptr = new_house.get();
	selected_house_id_ = house_ptr->getID();
	map_->houses.addHouse(std::move(new_house));
	map_->doChange();

	OnUpdate();
	g_gui.SelectBrush();
}

void HousePalette::OnClickEditHouse() {
	House* house = GetSelectedHouse();
	if (house && map_) {
		selected_house_id_ = house->getID();
		auto* d = new EditHouseDialog(g_gui.root, map_, house);
		if (d->ShowModal() == 1) {
			OnUpdate();
			g_gui.SelectBrush();
		}
		d->Destroy();
	}
}

void HousePalette::OnClickRemoveHouse() {
	House* house = GetSelectedHouse();
	if (house && map_) {
		const int ret = DialogUtil::PopupDialog(this, "Remove House", "Are you sure you want to remove this house? This cannot be undone.", wxYES_NO | wxICON_WARNING | wxCENTER);
		if (ret == wxYES) {
			if (selected_house_id_ == house->getID()) {
				selected_house_id_ = 0;
			}
			map_->houses.removeHouse(house);
			map_->doChange();
			OnUpdate();
			g_gui.SelectBrush();
			g_gui.RefreshView();
		}
	}
}

void HousePalette::OnGoToExit() {
	House* house = GetSelectedHouse();
	if (house && house->getExit().isValid() && g_gui.IsEditorOpen()) {
		g_gui.SetScreenCenterPosition(house->getExit());
	}
}

