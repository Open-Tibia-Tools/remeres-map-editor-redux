#include "palette/palette_zones.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/dialog_util.h"
#include "util/image_manager.h"
#include "util/common.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "brushes/zone/zone_brush.h"
#include "brushes/managers/brush_manager.h"
#include "rendering/indicators/zone_colors.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "editor/editor.h"
#include "editor/action_queue.h"

#include <nanovg.h>
#include <wx/menu.h>
#include <wx/textdlg.h>
#include <wx/msgdlg.h>
#include <wx/aui/auibar.h>
#include <algorithm>
#include <array>
#include <format>
#include <string_view>


namespace {

constexpr int kGroupHeaderHeight = 44;
constexpr int kChildHeight = 44;
constexpr int kCardSpacing = 4;
constexpr int kTreeIndent = 20;
constexpr int kIconSize = 32;

enum CustomToolID {
	TOOL_EXPAND_ALL = wxID_HIGHEST + 8101,
	TOOL_COLLAPSE_ALL
};



struct AreaClusterItem {
	size_t area_index = 0;
	Position centroid_pos;
	std::string formatted_pos;
	size_t tile_count = 0;
	std::string tile_count_str;
};

struct ZoneGroupItem {
	uint16_t id = 0;
	std::string name;
	std::string lowercase_name;
	glm::vec3 rgb { 0.5f, 0.5f, 0.5f };
	size_t total_tiles = 0;
	std::string total_tiles_str;
	std::string areas_str;
	std::vector<AreaClusterItem> clusters;
	bool expanded = false;
	size_t original_index = 0;
};

enum class VisualRowKind {
	GroupHeader,
	ChildArea
};

struct VisualRow {
	VisualRowKind kind = VisualRowKind::GroupHeader;
	size_t group_idx = 0;
	size_t child_idx = 0;
	int y = 0;
	int height = 0;
};

} // namespace

// ============================================================================
// ZoneCanvas
// ============================================================================

class ZonePalettePanel::ZoneCanvas final : public NanoVGCanvas {
public:
	explicit ZoneCanvas(ZonePalettePanel* owner) :
		NanoVGCanvas(owner, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
		m_owner(owner) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));

		Bind(wxEVT_SIZE, &ZoneCanvas::OnSize, this);
		Bind(wxEVT_LEFT_DOWN, &ZoneCanvas::OnMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &ZoneCanvas::OnLeftDClick, this);
		Bind(wxEVT_RIGHT_DOWN, &ZoneCanvas::OnContextMenu, this);
		Bind(wxEVT_MOTION, &ZoneCanvas::OnMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &ZoneCanvas::OnMouseLeave, this);
	}

	void SetZones(std::vector<ZoneGroupItem> groups) {
		m_groups = std::move(groups);
		if (m_has_sort) {
			ApplySort();
		}
		RebuildVisibleRows();
	}

	void SetFilter(const std::string& query) {
		m_filter_query = as_lower_str(query);
		if (!m_filter_query.empty()) {
			for (auto& g : m_groups) {
				g.expanded = true;
			}
		}
		RebuildVisibleRows();
	}

	void ExpandAll() {
		for (auto& g : m_groups) {
			g.expanded = true;
		}
		RebuildVisibleRows();
	}

	void CollapseAll() {
		for (auto& g : m_groups) {
			g.expanded = false;
		}
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
		std::ranges::sort(m_groups, [](const auto& a, const auto& b) {
			return a.id < b.id;
		});
		RebuildVisibleRows();
	}

	void SetSelection(uint16_t zid, int cluster) {
		selected_zone_id_ = zid;
		selected_cluster_ = cluster;
		Refresh();
	}

	[[nodiscard]] uint16_t GetSelectedZoneId() const { return selected_zone_id_; }
	[[nodiscard]] int GetSelectedCluster() const { return selected_cluster_; }

	void RebuildVisibleRows() {
		m_visible_rows.clear();
		const int padding = rme::ui::grid::kPadding;
		int cur_y = padding;

		for (size_t gi = 0; gi < m_groups.size(); ++gi) {
			const auto& g = m_groups[gi];
			const bool group_matches = m_filter_query.empty() ||
			                           g.lowercase_name.find(m_filter_query) != std::string::npos ||
			                           std::to_string(g.id).find(m_filter_query) != std::string::npos;

			bool any_child_matches = false;
			if (!group_matches && !m_filter_query.empty()) {
				for (const auto& c : g.clusters) {
					if (c.formatted_pos.find(m_filter_query) != std::string::npos ||
					    std::format("area {}", c.area_index + 1).find(m_filter_query) != std::string::npos) {
						any_child_matches = true;
						break;
					}
				}
			}

			if (!group_matches && !any_child_matches) {
				continue;
			}

			// Group Header
			m_visible_rows.push_back(VisualRow {
				.kind = VisualRowKind::GroupHeader,
				.group_idx = gi,
				.child_idx = 0,
				.y = cur_y,
				.height = kGroupHeaderHeight,
			});
			cur_y += kGroupHeaderHeight + kCardSpacing;

			// Child clusters
			if (g.expanded && g.clusters.size() > 1) {
				for (size_t ci = 0; ci < g.clusters.size(); ++ci) {
					const auto& c = g.clusters[ci];
					if (!group_matches && !m_filter_query.empty()) {
						if (c.formatted_pos.find(m_filter_query) == std::string::npos &&
						    std::format("area {}", c.area_index + 1).find(m_filter_query) == std::string::npos) {
							continue;
						}
					}

					m_visible_rows.push_back(VisualRow {
						.kind = VisualRowKind::ChildArea,
						.group_idx = gi,
						.child_idx = ci,
						.y = cur_y,
						.height = kChildHeight,
					});
					cur_y += kChildHeight + kCardSpacing;
				}
			}
		}

		UpdateScrollbar(cur_y + padding);
		Refresh();
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override {
		const int scroll_pos = GetScrollPosition();
		const int padding = rme::ui::grid::kPadding;

		if (m_visible_rows.empty()) {
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 13.0f);
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
			nvgText(vg, width * 0.5f, static_cast<float>(scroll_pos + height / 2),
				m_groups.empty() ? "No zones defined in map" : "No matching zones found", nullptr);
			return;
		}

		const int indent = FromDIP(kTreeIndent);
		const int visible_top = scroll_pos;
		const int visible_bottom = scroll_pos + height;

		const int dip_6 = FromDIP(6);
		const int dip_7 = FromDIP(7);
		const int dip_8 = FromDIP(8);
		const int dip_10 = FromDIP(10);
		const int dip_12 = FromDIP(12);
		const int dip_13 = FromDIP(13);
		const int dip_11 = FromDIP(11);
		const int dip_24 = FromDIP(24);
		const int dip_80 = FromDIP(80);

		for (size_t vi = 0; vi < m_visible_rows.size(); ++vi) {
			const auto& vrow = m_visible_rows[vi];
			if (vrow.y + vrow.height < visible_top || vrow.y > visible_bottom) {
				continue;
			}

			const bool is_group = (vrow.kind == VisualRowKind::GroupHeader);
			const int card_x = is_group ? padding : padding + indent;
			const int card_w = is_group ? width - padding * 2 : width - padding * 2 - indent;
			const wxRect card_rect(card_x, vrow.y, std::max(1, card_w), vrow.height);

			const bool is_hovered = (static_cast<int>(vi) == m_hover_row);
			const auto& g = m_groups[vrow.group_idx];

			if (is_group) {
				const bool is_selected = (g.id == selected_zone_id_ && selected_cluster_ == -1);
				const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, card_rect, is_selected, is_hovered, 0.0f, false);
				const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

				const wxColour meta_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
				const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);

				// 1. Expander Chevron if multi-area
				const float arrow_x = static_cast<float>(drawn.x + dip_8);
				const float arrow_sz = static_cast<float>(dip_7);
				if (g.clusters.size() > 1) {
					nvgBeginPath(vg);
					if (g.expanded) {
						nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
						nvgLineTo(vg, arrow_x + arrow_sz, center_y - arrow_sz * 0.5f);
						nvgLineTo(vg, arrow_x + arrow_sz * 0.5f, center_y + arrow_sz * 0.5f);
					} else {
						nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
						nvgLineTo(vg, arrow_x + arrow_sz, center_y);
						nvgLineTo(vg, arrow_x, center_y + arrow_sz * 0.5f);
					}
					nvgClosePath(vg);
					nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
					nvgFill(vg);
				}

				// 2. Zone ID badge (#1, #2) with 14px font matching Search Results
				const float index_x = arrow_x + arrow_sz + static_cast<float>(dip_6);
				std::array<char, 32> id_buf;
				auto res = std::format_to_n(id_buf.data(), id_buf.size() - 1, "#{}", g.id);
				*res.out = '\0';

				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, index_x, center_y, id_buf.data(), nullptr);

				float id_bounds[4] = {0};
				nvgTextBounds(vg, index_x, center_y, id_buf.data(), nullptr, id_bounds);
				const float id_w = std::max(static_cast<float>(FromDIP(24)), id_bounds[2] - id_bounds[0]);

				// 3. Crisp 32x32 Color Tile (Sunken slot with zone RGB fill)
				const int icon_x = static_cast<int>(index_x + id_w + static_cast<float>(dip_6));
				const int icon_y = drawn.y + (drawn.height - kIconSize) / 2;
				const wxRect slot_rect(icon_x, icon_y, kIconSize, kIconSize);
				rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);
				rme::rendering::DrawNanoVGCustomIndicatorBadge(vg, static_cast<float>(slot_rect.x), static_cast<float>(slot_rect.y), static_cast<float>(kIconSize), g.rgb, "zone");

				// 4. Right side: Tiles count formatted like Search Results (14px font)
				const float badge_x = static_cast<float>(drawn.x + drawn.width - dip_8);
				std::array<char, 64> count_buf;
				auto count_res = std::format_to_n(count_buf.data(), count_buf.size() - 1, "({} {})",
					g.total_tiles, g.total_tiles == 1 ? "tile" : "tiles");
				*count_res.out = '\0';

				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, badge_x, center_y, count_buf.data(), nullptr);

				// 5. Center: Name on top line, Areas underneath
				const float text_x = static_cast<float>(icon_x + kIconSize + dip_8);
				const float top_y = static_cast<float>(drawn.y + dip_13);
				const float bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);
				const float avail_w = std::max(20.0f, badge_x - text_x - static_cast<float>(dip_80 + dip_10));

			nvgSave(vg);
			nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));

				// Top line: Zone Name (14px)
				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(text_wx));
				nvgText(vg, text_x, top_y, g.name.c_str(), nullptr);

				// Bottom line: Area count
				std::array<char, 64> area_buf;
				auto a_res = std::format_to_n(area_buf.data(), area_buf.size() - 1, "({} {})",
					g.clusters.size(), g.clusters.size() == 1 ? "area" : "areas");
				*a_res.out = '\0';

				nvgFontSize(vg, 12.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, text_x, bottom_y, area_buf.data(), nullptr);

			nvgRestore(vg);

			} else {
				// Child Area Card
				const auto& c = g.clusters[vrow.child_idx];
				const bool is_selected = (g.id == selected_zone_id_ && static_cast<int>(vrow.child_idx) == selected_cluster_);
				const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, card_rect, is_selected, is_hovered, 0.0f, false);
				const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

				const wxColour meta_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
				const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);

				// 1. Child Index badge (#1, #2...) with 14px font
				const float index_x = static_cast<float>(drawn.x + dip_8);
				std::array<char, 32> id_buf;
				auto res = std::format_to_n(id_buf.data(), id_buf.size() - 1, "#{}", c.area_index + 1);
				*res.out = '\0';

				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, index_x, center_y, id_buf.data(), nullptr);

				// 2. Child 32x32 crisp Color Tile (same color as parent!)
				const int icon_x = static_cast<int>(index_x + static_cast<float>(dip_24 + dip_6));
				const int icon_y = drawn.y + (drawn.height - kIconSize) / 2;
				const wxRect slot_rect(icon_x, icon_y, kIconSize, kIconSize);
				rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);
				rme::rendering::DrawNanoVGCustomIndicatorBadge(vg, static_cast<float>(slot_rect.x), static_cast<float>(slot_rect.y), static_cast<float>(kIconSize), g.rgb, "zone");

				// 3. Right side: Tile count (14px font)
				const float badge_x = static_cast<float>(drawn.x + drawn.width - dip_8);
				std::array<char, 64> count_buf;
				auto count_res = std::format_to_n(count_buf.data(), count_buf.size() - 1, "({} {})",
					c.tile_count, c.tile_count == 1 ? "tile" : "tiles");
				*count_res.out = '\0';

				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, badge_x, center_y, count_buf.data(), nullptr);

				// 4. Center: Area #X on top, Centroid coordinates underneath
				const float text_x = static_cast<float>(icon_x + kIconSize + dip_8);
				const float top_y = static_cast<float>(drawn.y + dip_13);
				const float bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);
				const float avail_w = std::max(20.0f, badge_x - text_x - static_cast<float>(dip_80 + dip_10));

			nvgSave(vg);
			nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));

				// Top line: Area title (14px)
				std::array<char, 32> title_buf;
				auto t_res = std::format_to_n(title_buf.data(), title_buf.size() - 1, "Area #{}", c.area_index + 1);
				*t_res.out = '\0';

				nvgFontSize(vg, 14.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(text_wx));
				nvgText(vg, text_x, top_y, title_buf.data(), nullptr);

				// Bottom line: Centroid coords
				nvgFontSize(vg, 12.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
				nvgText(vg, text_x, bottom_y, c.formatted_pos.c_str(), nullptr);

			nvgRestore(vg);
		}
	}
}

private:
	void ApplySort() {
		if (m_sort_key == TilesetSortKey::Name) {
			if (m_sort_dir == TilesetSortDirection::Ascending) {
				std::ranges::sort(m_groups, [](const auto& a, const auto& b) {
					return a.name < b.name;
				});
			} else {
				std::ranges::sort(m_groups, [](const auto& a, const auto& b) {
					return a.name > b.name;
				});
			}
		} else if (m_sort_key == TilesetSortKey::ID) {
			if (m_sort_dir == TilesetSortDirection::Ascending) {
				std::ranges::sort(m_groups, [](const auto& a, const auto& b) {
					return a.id < b.id;
				});
			} else {
				std::ranges::sort(m_groups, [](const auto& a, const auto& b) {
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
		if (m_visible_rows.empty()) return -1;
		const int padding = rme::ui::grid::kPadding;
		const int content_y = mouse_y + GetScrollPosition();
		if (content_y < padding) return -1;
		auto it = std::lower_bound(m_visible_rows.begin(), m_visible_rows.end(), content_y, [](const VisualRow& row, int target_y) {
			return (row.y + row.height) <= target_y;
		});
		if (it != m_visible_rows.end() && content_y >= it->y && content_y < it->y + it->height) {
			return static_cast<int>(it - m_visible_rows.begin());
		}
		return -1;
	}

	void OnMouseDown(wxMouseEvent& event) {
		SetFocus();
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_rows.size())) {
			const auto& vrow = m_visible_rows[hit];
			auto& g = m_groups[vrow.group_idx];

			if (vrow.kind == VisualRowKind::GroupHeader) {
				// Check if click was on expand chevron
				const int indent = FromDIP(kTreeIndent);
				const int dip_20 = FromDIP(20);
				if (g.clusters.size() > 1 && event.GetX() < dip_20) {
					g.expanded = !g.expanded;
					RebuildVisibleRows();
					return;
				}

				selected_zone_id_ = g.id;
				selected_cluster_ = -1;
				Refresh();

				if (m_owner) {
					m_owner->SelectZoneById(selected_zone_id_, -1);
				}
			} else {
				selected_zone_id_ = g.id;
				selected_cluster_ = static_cast<int>(vrow.child_idx);
				Refresh();

				if (m_owner) {
					m_owner->SelectZoneById(selected_zone_id_, selected_cluster_);
				}
			}
		}
	}

	void OnLeftDClick(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_rows.size())) {
			const auto& vrow = m_visible_rows[hit];
			const auto& g = m_groups[vrow.group_idx];

			if (vrow.kind == VisualRowKind::ChildArea) {
				const auto& c = g.clusters[vrow.child_idx];
				if (c.centroid_pos.isValid() && g_gui.IsEditorOpen()) {
					g_gui.SetScreenCenterPosition(c.centroid_pos);
				}
			} else {
				if (!g.clusters.empty() && g_gui.IsEditorOpen()) {
					const auto it = std::ranges::max_element(g.clusters, {}, &AreaClusterItem::tile_count);
					if (it != g.clusters.end() && it->centroid_pos.isValid()) {
						g_gui.SetScreenCenterPosition(it->centroid_pos);
					}
				}
			}
		}
	}

	void OnContextMenu(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit >= 0 && hit < static_cast<int>(m_visible_rows.size())) {
			const auto& vrow = m_visible_rows[hit];
			const auto& g = m_groups[vrow.group_idx];
			selected_zone_id_ = g.id;
			selected_cluster_ = (vrow.kind == VisualRowKind::ChildArea) ? static_cast<int>(vrow.child_idx) : -1;
			Refresh();
			if (m_owner) {
				m_owner->SelectZoneById(selected_zone_id_, selected_cluster_);
			}
		}
		ShowContextMenu(event.GetPosition());
	}

	void ShowContextMenu(const wxPoint& pos) {
		wxMenu menu;
		if (selected_zone_id_ != 0) {
			menu.Append(wxID_FORWARD, "Go to Zone (Double-Click)")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
			menu.Append(wxID_EDIT, "Rename Zone...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
			menu.AppendSeparator();
			menu.Append(wxID_DELETE, "Remove Zone")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
			menu.AppendSeparator();
		}
		menu.Append(wxID_ADD, "Add Zone...")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent& ev) {
			if (!m_owner) return;
			switch (ev.GetId()) {
				case wxID_FORWARD:
					m_owner->OnGoToZone();
					break;
				case wxID_EDIT:
					m_owner->OnClickRenameZone();
					break;
				case wxID_DELETE:
					m_owner->OnClickRemoveZone();
					break;
				case wxID_ADD:
					m_owner->OnClickAddZone();
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
		if (hit >= 0 && hit < static_cast<int>(m_visible_rows.size())) {
			const auto& vrow = m_visible_rows[hit];
			const auto& g = m_groups[vrow.group_idx];
			if (vrow.kind == VisualRowKind::GroupHeader) {
				SetToolTip(wxstr(std::format("Zone #{}: {} ({} areas, {} tiles)", g.id, g.name, g.clusters.size(), g.total_tiles)));
			} else {
				const auto& c = g.clusters[vrow.child_idx];
				SetToolTip(wxstr(std::format("Zone #{}, Area #{}: {} ({})", g.id, c.area_index + 1, c.formatted_pos, c.tile_count_str)));
			}
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

	ZonePalettePanel* m_owner = nullptr;
	std::vector<ZoneGroupItem> m_groups;
	std::vector<VisualRow> m_visible_rows;
	std::string m_filter_query;
	uint16_t selected_zone_id_ = 0;
	int selected_cluster_ = -1;
	int m_hover_row = -1;
	bool m_has_sort = false;
	TilesetSortKey m_sort_key = TilesetSortKey::Name;
	TilesetSortDirection m_sort_dir = TilesetSortDirection::Ascending;
};

// ============================================================================
// ZonePalettePanel
// ============================================================================

ZonePalettePanel::ZonePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Zones");

	// 1. TilesetGridToolbar (Filter, Sort, Expand All / Collapse All)
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

	// Expand / Collapse tools on toolbar
	if (auto* aui = toolbar_->GetToolBar()) {
		const wxColour icon_color = Theme::Get(Theme::Role::Text);
		aui->AddTool(TOOL_EXPAND_ALL, wxEmptyString,
			IMAGE_MANAGER.GetBitmapBundle(ICON_ANGLES_DOWN, icon_color),
			"Expand all zone areas");
		aui->AddTool(TOOL_COLLAPSE_ALL, wxEmptyString,
			IMAGE_MANAGER.GetBitmapBundle(ICON_ANGLES_RIGHT, icon_color),
			"Collapse all zone areas");
		aui->Realize();
		toolbar_->Layout();

		aui->Bind(wxEVT_TOOL, [this](wxCommandEvent& ev) {
			if (ev.GetId() == TOOL_EXPAND_ALL) ExpandAll();
			else if (ev.GetId() == TOOL_COLLAPSE_ALL) CollapseAll();
		});
	}

	box_sizer->Add(toolbar_, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	// 2. Modern Zone Tree Canvas
	canvas_ = new ZoneCanvas(this);
	box_sizer->Add(canvas_, 1, wxEXPAND | wxBOTTOM, FromDIP(4));

	// 3. Action Button Bar (Add, Rename, Remove)
	auto* button_sizer = new wxBoxSizer(wxHORIZONTAL);
	const int dip_2 = FromDIP(2);

	add_button_ = new wxButton(this, wxID_ANY, "Add", wxDefaultPosition, wxSize(FromDIP(50), -1));
	add_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	add_button_->SetFont(Theme::GetFont(9, false));
	add_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickAddZone(); });
	button_sizer->Add(add_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	rename_button_ = new wxButton(this, wxID_ANY, "Rename", wxDefaultPosition, wxSize(FromDIP(60), -1));
	rename_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PEN));
	rename_button_->SetFont(Theme::GetFont(9, false));
	rename_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRenameZone(); });
	button_sizer->Add(rename_button_, 1, wxEXPAND | wxRIGHT, dip_2);

	remove_button_ = new wxButton(this, wxID_ANY, "Remove", wxDefaultPosition, wxSize(FromDIP(60), -1));
	remove_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	remove_button_->SetFont(Theme::GetFont(9, false));
	remove_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnClickRemoveZone(); });
	button_sizer->Add(remove_button_, 1, wxEXPAND);

	box_sizer->Add(button_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(2));

	root_sizer->Add(box_sizer, 1, wxEXPAND);
	SetSizer(root_sizer);
	Layout();
}

ZonePalettePanel::~ZonePalettePanel() = default;

wxString ZonePalettePanel::GetName() const {
	return "Zone";
}

void ZonePalettePanel::SelectFirstBrush() {
	if (map_ && !map_->zones.empty()) {
		for (const auto& z : map_->zones) {
			if (z.id != 0) {
				SelectZoneById(z.id, -1);
				break;
			}
		}
	}
}

Brush* ZonePalettePanel::GetSelectedBrush() const {
	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(selected_zone_id_);
		return g_brush_manager.zone_brush;
	}
	return nullptr;
}

int ZonePalettePanel::GetSelectedBrushSize() const {
	return 1;
}

bool ZonePalettePanel::SelectBrush(const Brush* whatbrush) {
	return g_brush_manager.zone_brush && whatbrush == g_brush_manager.zone_brush;
}

void ZonePalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
}

void ZonePalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void ZonePalettePanel::SetMap(Map* map) {
	if (map_ != map) {
		selected_zone_id_ = 0;
		selected_cluster_ = -1;
		if (g_brush_manager.zone_brush) {
			g_brush_manager.zone_brush->setZone(0);
		}
	}
	map_ = map;
	Enable(map_ != nullptr);
	OnUpdate();
}

void ZonePalettePanel::ExpandAll() {
	if (canvas_) canvas_->ExpandAll();
}

void ZonePalettePanel::CollapseAll() {
	if (canvas_) canvas_->CollapseAll();
}

void ZonePalettePanel::OnUpdate() {
	if (!map_ || !map_->supportsZones()) {
		if (canvas_) canvas_->SetZones({});
		if (add_button_) add_button_->Enable(false);
		if (rename_button_) rename_button_->Enable(false);
		if (remove_button_) remove_button_->Enable(false);
		return;
	}

	if (add_button_) add_button_->Enable(true);

	std::vector<ZoneGroupItem> groups;
	size_t orig_idx = 0;

	for (const auto& z : map_->zones) {
		if (z.id == 0) continue;

		auto raw_clusters = map_->getZoneClusters(z.id);
		const size_t total_tiles = map_->getZoneTilePositions(z.id).size();

		ZoneGroupItem g;
		g.id = z.id;
		g.name = z.name;
		g.lowercase_name = as_lower_str(z.name);
		g.rgb = rme::rendering::getZoneColor(z.id);
		g.total_tiles = total_tiles;
		g.total_tiles_str = std::format("{} tiles", total_tiles);
		g.areas_str = std::format("{} {}", raw_clusters.size(), raw_clusters.size() == 1 ? "area" : "areas");
		g.original_index = orig_idx++;

		for (size_t ci = 0; ci < raw_clusters.size(); ++ci) {
			const auto& c = raw_clusters[ci];
			g.clusters.push_back(AreaClusterItem {
				.area_index = ci,
				.centroid_pos = c.centroid_pos,
				.formatted_pos = std::format("[{}, {}, {}]", c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z),
				.tile_count = c.tile_count,
				.tile_count_str = std::format("{} tiles", c.tile_count)
			});
		}

		groups.push_back(std::move(g));
	}

	if (canvas_) {
		canvas_->SetZones(std::move(groups));
		canvas_->SetSelection(selected_zone_id_, selected_cluster_);
	}

	const bool has_selection = (selected_zone_id_ != 0 && map_->findZone(selected_zone_id_) != nullptr);
	if (rename_button_) rename_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

void ZonePalettePanel::SelectZoneById(uint16_t zone_id, int cluster) {
	selected_zone_id_ = zone_id;
	selected_cluster_ = cluster;

	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(zone_id);
	}
	g_gui.SelectBrush();

	const bool has_selection = (selected_zone_id_ != 0 && map_ && map_->findZone(selected_zone_id_) != nullptr);
	if (rename_button_) rename_button_->Enable(has_selection);
	if (remove_button_) remove_button_->Enable(has_selection);
}

void ZonePalettePanel::OnClickAddZone() {
	if (!map_ || !map_->supportsZones()) return;

	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Add Zone", wxEmptyString, this).Trim().Trim(false));
	if (name.empty()) return;

	const uint16_t id = map_->addZone(name);
	if (id == 0) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}

	SelectZoneById(id, -1);
	OnUpdate();
}

void ZonePalettePanel::OnClickRenameZone() {
	if (!map_ || selected_zone_id_ == 0) return;
	const Map::Zone* zone = map_->findZone(selected_zone_id_);
	if (!zone) return;

	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Rename Zone", wxstr(zone->name), this).Trim().Trim(false));
	if (name.empty() || name == zone->name) return;

	if (!map_->renameZone(selected_zone_id_, name)) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}

	OnUpdate();
}

void ZonePalettePanel::OnClickRemoveZone() {
	if (!map_ || selected_zone_id_ == 0) return;
	const Map::Zone* zone = map_->findZone(selected_zone_id_);
	if (!zone) return;

	const wxString question = wxstr(std::format("Remove zone \"{}\" (ID: {}) and clear it from all tiles?\nThis cannot be undone.", zone->name, selected_zone_id_));
	if (DialogUtil::PopupDialog(this, "Remove Zone", question, wxYES_NO | wxICON_WARNING) != wxYES) {
		return;
	}

	if (Editor* editor = g_gui.GetCurrentEditor()) {
		if (editor->actionQueue) {
			editor->actionQueue->clear();
		}
	}

	const uint16_t id = selected_zone_id_;
	map_->removeZone(id);
	selected_zone_id_ = 0;
	selected_cluster_ = -1;

	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(0);
	}

	OnUpdate();
	g_gui.SelectBrush();
	g_gui.RefreshView();
}

void ZonePalettePanel::OnGoToZone() {
	if (!map_ || selected_zone_id_ == 0) return;
	const auto clusters = map_->getZoneClusters(selected_zone_id_);
	if (clusters.empty()) return;

	if (selected_cluster_ >= 0 && static_cast<size_t>(selected_cluster_) < clusters.size()) {
		g_gui.SetScreenCenterPosition(clusters[static_cast<size_t>(selected_cluster_)].centroid_pos);
	} else {
		const auto it = std::ranges::max_element(clusters, {}, &ZoneCluster::tile_count);
		if (it != clusters.end()) {
			g_gui.SetScreenCenterPosition(it->centroid_pos);
		}
	}
}

