#include "app/main.h"
#include "ui/result_window.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "util/image_manager.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "editor/editor.h"
#include "editor/operations/search_query.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/items.h"
#include "brushes/creature/creature_brush.h"

#include <wx/aui/auibar.h>
#include <wx/choice.h>
#include <nanovg.h>

#include <algorithm>
#include <array>
#include <format>
#include <unordered_map>

namespace {

constexpr int kTreeIndent = 20; // Indentation for child cards in the sub-tree
constexpr int kCardSpacing = 4; // Vertical spacing between cards (m_gridSpacing = 4)
constexpr int kGroupHeaderHeight = 38; // Matches rme::ui::grid::kListRowHeight + 2 for 32x32 icon
constexpr int kNormalChildHeight = 44; // Fits 32x32 icon + 2 lines of text
constexpr int kContainerCardHeight = 74; // Single card fitting Row 1 (32x32 items) + Row 2 (UID/AID/Position)

enum CustomToolID {
	TOOL_EXPAND_ALL = wxID_HIGHEST + 8001,
	TOOL_COLLAPSE_ALL
};

enum class RowKind {
	GroupHeader,
	NormalChild,
	Container
};

struct FlatVisualRow {
	RowKind kind = RowKind::GroupHeader;
	size_t group_index = 0;
	size_t child_index = 0;
	int y = 0;
	int height = 0;
};

std::string as_lower_str(std::string_view sv) {
	std::string s;
	s.reserve(sv.size());
	for (char ch : sv) {
		s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
	}
	return s;
}

} // namespace

// ============================================================================
// SearchResultCanvas
// ============================================================================

class SearchResultWindow::SearchResultCanvas final : public NanoVGCanvas {
public:
	explicit SearchResultCanvas(wxWindow* parent) :
		NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxWANTS_CHARS) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));

		Bind(wxEVT_SIZE, &SearchResultCanvas::OnSize, this);
		Bind(wxEVT_LEFT_DOWN, &SearchResultCanvas::OnMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &SearchResultCanvas::OnLeftDClick, this);
		Bind(wxEVT_MOTION, &SearchResultCanvas::OnMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &SearchResultCanvas::OnMouseLeave, this);
		Bind(wxEVT_KEY_DOWN, &SearchResultCanvas::OnKeyDown, this);
	}

	void SetGroups(std::vector<SearchResultGroup> groups) {
		m_groups = std::move(groups);
		m_selected_group = -1;
		m_selected_child = -1;
		m_hover_row = -1;
		ReindexGroups();
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

	void SetFilter(const std::string& query) {
		m_filter_query = as_lower_str(query);
		m_filter_number.reset();
		if (!m_filter_query.empty() && std::ranges::all_of(m_filter_query, [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); })) {
			try {
				m_filter_number = static_cast<uint32_t>(std::stoul(m_filter_query));
			} catch (...) {}
		}

		if (!m_filter_query.empty()) {
			for (auto& g : m_groups) {
				const bool name_matches = (!g.lowercase_name.empty() && g.lowercase_name.find(m_filter_query) != std::string::npos) ||
				                          (!g.id_str.empty() && g.id_str.find(m_filter_query) != std::string::npos);
				if (name_matches) {
					g.expanded = (g.children.size() <= 100);
				} else {
					g.expanded = MatchesGroupChildren(g);
				}
			}
		}
		RebuildVisibleRows();
	}

	void SortGroups(TilesetSortKey key, TilesetSortDirection dir) {
		if (key == TilesetSortKey::ID) {
			if (dir == TilesetSortDirection::Ascending) {
				std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
					return a.id < b.id;
				});
			} else {
				std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
					return a.id > b.id;
				});
			}
		} else {
			if (dir == TilesetSortDirection::Ascending) {
				std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
					return a.name < b.name;
				});
			} else {
				std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
					return a.name > b.name;
				});
			}
		}
		ReindexGroups();
		RebuildVisibleRows();
	}

	void SortGroupsByDefault() {
		// Default sort: most matches first, then alphabetical name
		std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
			if (a.children.size() != b.children.size()) {
				return a.children.size() > b.children.size();
			}
			return a.name < b.name;
		});
		ReindexGroups();
		RebuildVisibleRows();
	}

	[[nodiscard]] size_t GetTotalGroupCount() const { return m_groups.size(); }

	[[nodiscard]] size_t GetTotalMatchCount() const {
		size_t count = 0;
		for (const auto& g : m_groups) {
			count += g.children.size();
		}
		return count;
	}

	void RebuildVisibleRows() {
		m_visible_rows.clear();

		const int padding = rme::ui::grid::kPadding;
		int current_y = padding;

		for (size_t gi = 0; gi < m_groups.size(); ++gi) {
			const auto& group = m_groups[gi];
			const bool group_name_matches = m_filter_query.empty() ||
			                                (!group.lowercase_name.empty() && group.lowercase_name.find(m_filter_query) != std::string::npos) ||
			                                (!group.id_str.empty() && group.id_str.find(m_filter_query) != std::string::npos);

			if (!group_name_matches && !MatchesGroupChildren(group)) {
				continue;
			}

			// Group Header Row
			m_visible_rows.push_back(FlatVisualRow {
				.kind = RowKind::GroupHeader,
				.group_index = gi,
				.child_index = 0,
				.y = current_y,
				.height = kGroupHeaderHeight,
			});
			current_y += kGroupHeaderHeight + kCardSpacing;

			if (group.expanded) {
				for (size_t ci = 0; ci < group.children.size(); ++ci) {
					const auto& child = group.children[ci];
					if (!group_name_matches && !MatchesChild(child)) {
						continue;
					}

					// Single unified card per child position
					const int child_h = (child.kind == SearchResultKind::Container) ? kContainerCardHeight : kNormalChildHeight;
					m_visible_rows.push_back(FlatVisualRow {
						.kind = (child.kind == SearchResultKind::Container) ? RowKind::Container : RowKind::NormalChild,
						.group_index = gi,
						.child_index = ci,
						.y = current_y,
						.height = child_h,
					});
					current_y += child_h + kCardSpacing;
				}
			}
		}

		UpdateScrollbar(current_y + padding);
		Refresh();
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override {
		const int scroll_pos = GetScrollPosition();

		if (m_visible_rows.empty()) {
			DrawEmptyState(vg, width, height, scroll_pos);
			return;
		}

		const int indent = FromDIP(kTreeIndent);
		const int padding = rme::ui::grid::kPadding;
		const int visible_top = scroll_pos;
		const int visible_bottom = scroll_pos + height;

		for (size_t i = 0; i < m_visible_rows.size(); ++i) {
			const auto& vrow = m_visible_rows[i];
			if (vrow.y + vrow.height < visible_top || vrow.y > visible_bottom) {
				continue;
			}

			const bool is_group = (vrow.kind == RowKind::GroupHeader);
			const int card_x = is_group ? padding : padding + indent;
			const int card_w = is_group ? width - padding * 2 : width - padding * 2 - indent;
			const wxRect card_rect(card_x, vrow.y, std::max(1, card_w), vrow.height);

			const bool is_hovered = (static_cast<int>(i) == m_hover_row);

			if (is_group) {
				const auto& group = m_groups[vrow.group_index];
				const bool is_selected = (static_cast<int>(vrow.group_index) == m_selected_group && m_selected_child == -1);
				DrawGroupHeader(vg, card_rect, group, vrow.group_index, is_selected, is_hovered);
			} else {
				const auto& group = m_groups[vrow.group_index];
				const auto& child = group.children[vrow.child_index];
				const bool is_selected = (static_cast<int>(vrow.group_index) == m_selected_group && static_cast<int>(vrow.child_index) == m_selected_child);

				if (vrow.kind == RowKind::Container) {
					DrawContainerCard(vg, card_rect, child, vrow.child_index, group.children.size(), is_selected, is_hovered);
				} else {
					DrawChildRow(vg, card_rect, child, vrow.child_index, group.children.size(), is_selected, is_hovered);
				}
			}
		}
	}

private:
	void ReindexGroups() {
		// Nothing additional needed; index is based on display position
	}

	bool MatchesGroupChildren(const SearchResultGroup& g) const {
		for (const auto& c : g.children) {
			if (MatchesChild(c)) {
				return true;
			}
		}
		return false;
	}

	bool MatchesGroup(const SearchResultGroup& g) const {
		if (m_filter_query.empty()) {
			return true;
		}

		if (!g.lowercase_name.empty() && g.lowercase_name.find(m_filter_query) != std::string::npos) {
			return true;
		}
		if (!g.id_str.empty() && g.id_str.find(m_filter_query) != std::string::npos) {
			return true;
		}

		return MatchesGroupChildren(g);
	}

	bool MatchesChild(const SearchResultChildEntry& c) const {
		if (m_filter_query.empty()) {
			return true;
		}

		if (m_filter_number.has_value()) {
			const uint32_t num = *m_filter_number;
			if (c.unique_id == num || c.action_id == num || c.item_id == num) {
				return true;
			}
			if (static_cast<uint32_t>(c.position.x) == num ||
			    static_cast<uint32_t>(c.position.y) == num ||
			    static_cast<uint32_t>(c.position.z) == num) {
				return true;
			}
		}

		if (c.unique_id > 0 && std::to_string(c.unique_id).find(m_filter_query) != std::string::npos) {
			return true;
		}
		if (c.action_id > 0 && std::to_string(c.action_id).find(m_filter_query) != std::string::npos) {
			return true;
		}
		if (!c.text.empty() && as_lower_str(c.text).find(m_filter_query) != std::string::npos) {
			return true;
		}
		if (!c.subtitle.empty() && as_lower_str(c.subtitle).find(m_filter_query) != std::string::npos) {
			return true;
		}
		if (c.kind == SearchResultKind::Teleport) {
			const std::string dest_str = std::format("{},{},{}", c.teleport_destination.x, c.teleport_destination.y, c.teleport_destination.z);
			if (dest_str.find(m_filter_query) != std::string::npos) {
				return true;
			}
		}
		for (const auto& ci : c.container_items) {
			if (std::to_string(ci.item_id).find(m_filter_query) != std::string::npos) {
				return true;
			}
			const auto def = g_item_definitions.get(ci.item_id);
			if (def && as_lower_str(std::string(def.name())).find(m_filter_query) != std::string::npos) {
				return true;
			}
		}
		const std::string pos_str = std::format("{},{},{}", c.position.x, c.position.y, c.position.z);
		return pos_str.find(m_filter_query) != std::string::npos;
	}

	void DrawEmptyState(NVGcontext* vg, int width, int height, int scroll_pos) const {
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		const float center_y = static_cast<float>(scroll_pos + height / 2);

		nvgFontSize(vg, 14.0f);
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
		if (!g_gui.IsEditorOpen()) {
			nvgText(vg, width * 0.5f, center_y - 10.0f, "No map open", nullptr);
			nvgFontSize(vg, 12.0f);
			nvgText(vg, width * 0.5f, center_y + 12.0f, "Open a map to view search results", nullptr);
		} else {
			nvgText(vg, width * 0.5f, center_y - 10.0f, "No matching items found", nullptr);
			nvgFontSize(vg, 12.0f);
			nvgText(vg, width * 0.5f, center_y + 12.0f, "Try adjusting your search criteria or category filter", nullptr);
		}
	}

	int GetTextureForGroup(NVGcontext* vg, const SearchResultGroup& group) {
		if (group.is_creature) {
			if (group.brush) {
				if (Sprite* sprite = group.brush->getSprite()) {
					return GetOrCreateSpriteTexture(vg, sprite);
				}
			}
			return 0;
		}
		return GetOrCreateItemImage(static_cast<uint16_t>(group.id));
	}

	void DrawGroupHeader(NVGcontext* vg, const wxRect& rect, const SearchResultGroup& group, size_t group_idx, bool selected, bool hovered) {
		const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);
		const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

		// 1. Expander Chevron Arrow
		const float arrow_x = static_cast<float>(drawn.x + FromDIP(8));
		const float arrow_sz = static_cast<float>(FromDIP(7));
		const NVGcolor arrow_col = selected
			? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent))
			: NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle));

		nvgBeginPath(vg);
		if (group.expanded) {
			nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
			nvgLineTo(vg, arrow_x + arrow_sz, center_y - arrow_sz * 0.5f);
			nvgLineTo(vg, arrow_x + arrow_sz * 0.5f, center_y + arrow_sz * 0.5f);
		} else {
			nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
			nvgLineTo(vg, arrow_x + arrow_sz, center_y);
			nvgLineTo(vg, arrow_x, center_y + arrow_sz * 0.5f);
		}
		nvgClosePath(vg);
		nvgFillColor(vg, arrow_col);
		nvgFill(vg);

		// 2. Index number
		const std::string index_str = std::format("#{}", group_idx + 1);
		const float index_x = arrow_x + arrow_sz + FromDIP(6);

		nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
		nvgText(vg, index_x, center_y, index_str.c_str(), nullptr);

		// Measure index text width
		float index_bounds[4] = {0};
		nvgTextBounds(vg, index_x, center_y, index_str.c_str(), nullptr, index_bounds);
		const float index_width = std::max(static_cast<float>(FromDIP(26)), index_bounds[2] - index_bounds[0]);

		// 3. 32x32 Icon
		const int icon_sz = rme::ui::grid::kIconSizeBase; // 32
		const int icon_x = static_cast<int>(index_x + index_width + FromDIP(6));
		const int icon_y = drawn.y + (drawn.height - icon_sz) / 2;
		const wxRect icon_rect(icon_x, icon_y, icon_sz, icon_sz);

		const int tex = GetTextureForGroup(vg, group);
		rme::ui::grid::DrawCellIcon(vg, icon_rect, tex, rme::rendering::TileIndicatorType::None);

		// 4. Group Name
		const float text_x = static_cast<float>(icon_x + icon_sz + FromDIP(8));
		const float badge_w = static_cast<float>(FromDIP(80));
		const float avail_w = std::max(20.0f, static_cast<float>(drawn.x + drawn.width) - text_x - badge_w);

		nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));

		nvgSave(vg);
		nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));
		nvgText(vg, text_x, center_y, group.name.c_str(), nullptr);
		nvgRestore(vg);

		// 5. Match count badge on the right
		const std::string count_str = std::format("({} {})", group.children.size(), group.children.size() == 1 ? "match" : "matches");
		nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
		nvgText(vg, static_cast<float>(drawn.x + drawn.width - FromDIP(8)), center_y, count_str.c_str(), nullptr);
	}

	static constexpr int GetIndexColumnWidth(size_t total_count) {
		if (total_count >= 1000) return 40;
		if (total_count >= 100) return 32;
		return 24;
	}

	// Unified card for a container position:
	// Main 32x32 icon on the left (vertically centered); to the right: Row 1 has 32x32 contained item slots, Row 2 has UID/AID on top and Position on bottom
	void DrawContainerCard(NVGcontext* vg, const wxRect& rect, const SearchResultChildEntry& child, size_t child_idx, size_t total_children, bool selected, bool hovered) {
		const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);
		const int icon_sz = 32;

		const wxColour meta_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
		const wxColour accent_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Accent);

		// -------------------------------------------------------------
		// Child Index number (#1, #2, ...) on the left
		// -------------------------------------------------------------
		const int index_col_w = FromDIP(GetIndexColumnWidth(total_children));
		const int index_x = drawn.x + FromDIP(6);
		const float center_y = static_cast<float>(drawn.y + drawn.height / 2);
		const std::string index_str = std::format("#{}", child_idx + 1);

		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
		nvgText(vg, static_cast<float>(index_x + index_col_w), center_y, index_str.c_str(), nullptr);

		// -------------------------------------------------------------
		// Main Container 32x32 icon on the left (vertically centered)
		// -------------------------------------------------------------
		const int main_icon_x = index_x + index_col_w + FromDIP(6);
		const int main_icon_y = drawn.y + (drawn.height - icon_sz) / 2;
		const wxRect main_slot(main_icon_x, main_icon_y, icon_sz, icon_sz);
		rme::ui::grid::DrawCardChrome(vg, main_slot, false, false, 0.0f, false);

		const int main_tex = GetOrCreateItemImage(child.item_id);
		rme::ui::grid::DrawCellIcon(vg, main_slot, main_tex, rme::rendering::TileIndicatorType::None);

		// Content area begins to the right of the main 32x32 container icon
		const int content_x = main_icon_x + icon_sz + FromDIP(8);
		const int icon_gap = 4;
		const int icon_step = icon_sz + icon_gap;
		const int icons_y = drawn.y + FromDIP(6);

		// -------------------------------------------------------------
		// Row 1: Horizontal strip of 32x32 contained items (or empty 32x32 box)
		// -------------------------------------------------------------
		if (child.container_items.empty()) {
			const wxRect empty_slot(content_x, icons_y, icon_sz, icon_sz);
			rme::ui::grid::DrawCardChrome(vg, empty_slot, false, false, 0.0f, false);

			nvgFontFace(vg, "sans");
			nvgFontSize(vg, 12.0f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
			nvgText(vg, static_cast<float>(content_x + icon_sz + FromDIP(8)), static_cast<float>(icons_y + icon_sz / 2), "(empty)", nullptr);
		} else {
			const int max_x = drawn.x + drawn.width - FromDIP(44);
			size_t drawn_count = 0;

			for (size_t i = 0; i < child.container_items.size(); ++i) {
				const int ix = content_x + static_cast<int>(i) * icon_step;
				if (ix + icon_sz > max_x) {
					// Overflow badge slot (+N)
					const wxRect overflow_rect(ix, icons_y, icon_sz, icon_sz);
					rme::ui::grid::DrawCardChrome(vg, overflow_rect, false, false, 0.0f, false);

					const std::string more_str = std::format("+{}", child.container_items.size() - drawn_count);
					nvgFontFace(vg, "sans");
					nvgFontSize(vg, 13.0f);
					nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
					nvgFillColor(vg, NvgUtils::ToNvColor(accent_wx));
					nvgText(vg, static_cast<float>(ix + icon_sz / 2), static_cast<float>(icons_y + icon_sz / 2), more_str.c_str(), nullptr);
					break;
				}

				// 32x32 sunken slot
				const wxRect slot_rect(ix, icons_y, icon_sz, icon_sz);
				rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);

				const auto& contained = child.container_items[i];
				const int ctex = GetOrCreateItemImage(contained.item_id);
				rme::ui::grid::DrawCellIcon(vg, slot_rect, ctex, rme::rendering::TileIndicatorType::None);

				// Draw count badge if count > 1
				if (contained.count > 1) {
					const std::string count_str = std::to_string(contained.count);
					nvgFontSize(vg, 11.0f);
					nvgFontFace(vg, "sans");
					nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);

					// Text shadow
					nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
					nvgText(vg, static_cast<float>(ix + icon_sz), static_cast<float>(icons_y + icon_sz), count_str.c_str(), nullptr);

					// Text in TooltipCountText theme color
					const wxColour count_col = Theme::Get(Theme::Role::TooltipCountText);
					nvgFillColor(vg, nvgRGBA(count_col.Red(), count_col.Green(), count_col.Blue(), 255));
					nvgText(vg, static_cast<float>(ix + icon_sz - 1), static_cast<float>(icons_y + icon_sz - 1), count_str.c_str(), nullptr);
				}

				++drawn_count;
			}
		}

		// -------------------------------------------------------------
		// Row 2: Action ID, Unique ID on top, Position on bottom
		// -------------------------------------------------------------
		const float top_y = static_cast<float>(drawn.y + FromDIP(48));
		const float bottom_y = static_cast<float>(drawn.y + FromDIP(64));

		nvgSave(vg);
		nvgScissor(vg, static_cast<float>(content_x), static_cast<float>(drawn.y), static_cast<float>(drawn.x + drawn.width - content_x - FromDIP(8)), static_cast<float>(drawn.height));

		DrawAttributeTokens(vg, static_cast<float>(content_x), top_y, child, selected);

		// Bottom line: Position
		const std::string coords_str = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
		nvgText(vg, static_cast<float>(content_x), bottom_y, coords_str.c_str(), nullptr);

		nvgRestore(vg);
	}

	struct AttributeToken {
		std::string text;
		wxColour color;
	};

	void DrawAttributeTokens(NVGcontext* vg, float start_x, float y, const SearchResultChildEntry& child, bool selected) {
		std::array<AttributeToken, 6> tokens;
		size_t token_count = 0;

		const wxColour white_wx = Theme::Get(Theme::Role::TextOnAccent);
		const wxColour uid_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipUniqueId);
		const wxColour aid_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipActionId);
		const wxColour tele_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipTeleport);
		const wxColour text_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipTextValue);
		const wxColour accent_wx = selected ? white_wx : Theme::Get(Theme::Role::Accent);
		const wxColour subtle_wx = selected ? white_wx : Theme::Get(Theme::Role::TextSubtle);

		if (child.kind == SearchResultKind::Teleport) {
			tokens[token_count++] = {
				std::format("Dest: [{}, {}, {}]", child.teleport_destination.x, child.teleport_destination.y, child.teleport_destination.z),
				tele_wx
			};
		} else if (child.kind == SearchResultKind::Creature) {
			tokens[token_count++] = {
				child.subtitle.empty() ? "Creature" : child.subtitle,
				accent_wx
			};
		} else if (child.kind == SearchResultKind::Pickupable) {
			tokens[token_count++] = {
				"Pickupable",
				accent_wx
			};
		}

		if (child.unique_id > 0) {
			tokens[token_count++] = {
				std::format("UID: {}", child.unique_id),
				uid_wx
			};
		}

		if (child.action_id > 0) {
			tokens[token_count++] = {
				std::format("AID: {}", child.action_id),
				aid_wx
			};
		}

		if (!child.text.empty()) {
			tokens[token_count++] = {
				std::format("Text: \"{}\"", child.text),
				text_wx
			};
		}

		if (token_count == 0) {
			tokens[token_count++] = {
				(child.kind == SearchResultKind::Container) ? "Container Instance" : "Item Instance",
				subtle_wx
			};
		}

		nvgFontFace(vg, "sans");
		nvgFontSize(vg, 13.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

		float cur_x = start_x;
		const float gap = static_cast<float>(FromDIP(10));

		for (size_t i = 0; i < token_count; ++i) {
			const auto& token = tokens[i];
			nvgFillColor(vg, NvgUtils::ToNvColor(token.color));
			nvgText(vg, cur_x, y, token.text.c_str(), nullptr);

			float bounds[4] = {0};
			nvgTextBounds(vg, cur_x, y, token.text.c_str(), nullptr, bounds);
			cur_x = bounds[2] + gap;
		}
	}

	void DrawChildRow(NVGcontext* vg, const wxRect& rect, const SearchResultChildEntry& child, size_t child_idx, size_t total_children, bool selected, bool hovered) {
		const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);

		const wxColour meta_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);

		// -------------------------------------------------------------
		// Child Index number (#1, #2, ...) on the left
		// -------------------------------------------------------------
		const int index_col_w = FromDIP(GetIndexColumnWidth(total_children));
		const int index_x = drawn.x + FromDIP(6);
		const float center_y = static_cast<float>(drawn.y + drawn.height / 2);
		const std::string index_str = std::format("#{}", child_idx + 1);

		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
		nvgText(vg, static_cast<float>(index_x + index_col_w), center_y, index_str.c_str(), nullptr);

		// 32x32 item/creature icon next to index
		const int icon_sz = 32;
		const int icon_x = index_x + index_col_w + FromDIP(6);
		const int icon_y = drawn.y + (drawn.height - icon_sz) / 2;

		const wxRect slot_rect(icon_x, icon_y, icon_sz, icon_sz);
		rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);

		int tex = 0;
		if (child.is_creature) {
			if (child.brush) {
				if (Sprite* sprite = child.brush->getSprite()) {
					tex = GetOrCreateSpriteTexture(vg, sprite);
				}
			}
		} else {
			tex = GetOrCreateItemImage(child.item_id);
		}
		rme::ui::grid::DrawCellIcon(vg, slot_rect, tex, rme::rendering::TileIndicatorType::None);

		// Text section next to 32x32 icon
		const float text_x = static_cast<float>(icon_x + icon_sz + FromDIP(8));
		const float top_y = static_cast<float>(drawn.y + FromDIP(13));
		const float bottom_y = static_cast<float>(drawn.y + drawn.height - FromDIP(11));

		nvgSave(vg);
		nvgScissor(vg, text_x, static_cast<float>(drawn.y), static_cast<float>(drawn.x + drawn.width - text_x - FromDIP(8)), static_cast<float>(drawn.height));

		DrawAttributeTokens(vg, text_x, top_y, child, selected);

		// Bottom line: Coordinates
		const std::string coords_str = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
		nvgText(vg, text_x, bottom_y, coords_str.c_str(), nullptr);

		nvgRestore(vg);
	}

	void OnSize(wxSizeEvent& event) {
		RebuildVisibleRows();
		event.Skip();
	}

	void OnMouseDown(wxMouseEvent& event) {
		SetFocus();
		const int hit = HitTest(event.GetY());
		if (hit < 0 || hit >= static_cast<int>(m_visible_rows.size())) {
			return;
		}

		const auto& vrow = m_visible_rows[hit];
		if (vrow.kind == RowKind::GroupHeader) {
			// Toggle expand / collapse
			auto& group = m_groups[vrow.group_index];
			group.expanded = !group.expanded;
			m_selected_group = static_cast<int>(vrow.group_index);
			m_selected_child = -1;
			RebuildVisibleRows();
		} else {
			// Select child and teleport camera
			m_selected_group = static_cast<int>(vrow.group_index);
			m_selected_child = static_cast<int>(vrow.child_index);
			Refresh();

			const auto& group = m_groups[vrow.group_index];
			const auto& child = group.children[vrow.child_index];
			if (g_gui.IsEditorOpen()) {
				g_gui.SetScreenCenterPosition(child.position);
			}
		}
	}

	void OnLeftDClick(wxMouseEvent& event) {
		OnMouseDown(event);
	}

	void OnMouseMove(wxMouseEvent& event) {
		const int hit = HitTest(event.GetY());
		if (hit != m_hover_row) {
			m_hover_row = hit;
			Refresh();
		}
		HandleMouseHover(event.GetX(), event.GetY());
	}

	void OnMouseLeave(wxMouseEvent& WXUNUSED(event)) {
		if (m_hover_row != -1) {
			m_hover_row = -1;
			Refresh();
		}
		SetToolTip("");
	}

	void HandleMouseHover(int mouse_x, int mouse_y) {
		const int hit = HitTest(mouse_y);
		if (hit < 0 || hit >= static_cast<int>(m_visible_rows.size())) {
			SetToolTip("");
			return;
		}

		const auto& vrow = m_visible_rows[hit];
		if (vrow.kind == RowKind::GroupHeader) {
			const auto& group = m_groups[vrow.group_index];
			SetToolTip(wxstr(std::format("{} ({} {})", group.name, group.children.size(), group.children.size() == 1 ? "match" : "matches")));
			return;
		}

		const auto& group = m_groups[vrow.group_index];
		const auto& child = group.children[vrow.child_index];

		if (vrow.kind == RowKind::Container) {
			const int indent = FromDIP(kTreeIndent);
			const int padding = rme::ui::grid::kPadding;
			const int card_x = padding + indent;
			const int icon_sz = 32;
			const int index_col_w = FromDIP(GetIndexColumnWidth(group.children.size()));
			const int main_icon_x = card_x + FromDIP(6) + index_col_w + FromDIP(6);
			const int content_x = main_icon_x + icon_sz + FromDIP(8);
			const int content_y = mouse_y + GetScrollPosition();
			const int icon_gap = 4;
			const int icon_step = icon_sz + icon_gap;
			const int icons_y = vrow.y + FromDIP(6);

			// Check if mouse is over main container 32x32 icon on the left
			const int main_icon_y = vrow.y + (vrow.height - icon_sz) / 2;
			if (content_y >= main_icon_y && content_y < main_icon_y + icon_sz &&
				mouse_x >= main_icon_x && mouse_x < main_icon_x + icon_sz) {
				const auto def = g_item_definitions.get(child.item_id);
				const std::string item_name = def ? std::string(def.name()) : group.name;
				SetToolTip(wxstr(item_name));
				return;
			}

			// Check if mouse is over Row 1 (contained items)
			if (content_y >= icons_y && content_y < icons_y + icon_sz && mouse_x >= content_x) {
				if (child.container_items.empty()) {
					if (mouse_x < content_x + icon_sz) {
						SetToolTip("Empty container");
						return;
					}
				} else {
					const int col = (mouse_x - content_x) / icon_step;
					const int offset_in_col = (mouse_x - content_x) % icon_step;
					if (col >= 0 && col < static_cast<int>(child.container_items.size()) && offset_in_col <= icon_sz) {
						const auto& item_entry = child.container_items[col];
						const auto def = g_item_definitions.get(item_entry.item_id);
						const std::string item_name = def ? std::string(def.name()) : std::format("Item #{}", item_entry.item_id);
						if (item_entry.count > 1) {
							SetToolTip(wxstr(std::format("{}x {}", item_entry.count, item_name)));
						} else {
							SetToolTip(wxstr(item_name));
						}
						return;
					}
				}
			}
		}

		// Tooltips for specific child types
		const std::string child_prefix = std::format("#{}: ", vrow.child_index + 1);
		if (child.kind == SearchResultKind::Teleport) {
			SetToolTip(wxstr(std::format("{}Teleport to [{}, {}, {}] | Pos: [{}, {}, {}]",
				child_prefix,
				child.teleport_destination.x, child.teleport_destination.y, child.teleport_destination.z,
				child.position.x, child.position.y, child.position.z)));
		} else if (child.kind == SearchResultKind::Creature) {
			SetToolTip(wxstr(std::format("{}{} | Pos: [{}, {}, {}]", child_prefix, child.subtitle, child.position.x, child.position.y, child.position.z)));
		} else if (child.unique_id > 0 && child.action_id > 0) {
			SetToolTip(wxstr(std::format("{}UID: {} | AID: {} | Pos: [{}, {}, {}]", child_prefix, child.unique_id, child.action_id, child.position.x, child.position.y, child.position.z)));
		} else if (child.unique_id > 0) {
			SetToolTip(wxstr(std::format("{}UID: {} | Pos: [{}, {}, {}]", child_prefix, child.unique_id, child.position.x, child.position.y, child.position.z)));
		} else if (child.action_id > 0) {
			SetToolTip(wxstr(std::format("{}AID: {} | Pos: [{}, {}, {}]", child_prefix, child.action_id, child.position.x, child.position.y, child.position.z)));
		} else if (!child.text.empty()) {
			SetToolTip(wxstr(std::format("{}\"{}\" | Pos: [{}, {}, {}]", child_prefix, child.text, child.position.x, child.position.y, child.position.z)));
		} else {
			SetToolTip(wxstr(std::format("{}Pos: [{}, {}, {}]", child_prefix, child.position.x, child.position.y, child.position.z)));
		}
	}

	void OnKeyDown(wxKeyEvent& event) {
		if (m_visible_rows.empty()) {
			event.Skip();
			return;
		}

		switch (event.GetKeyCode()) {
			case WXK_UP:
			case WXK_DOWN:
			case WXK_RETURN:
			case WXK_NUMPAD_ENTER:
				break;
			default:
				event.Skip();
				break;
		}
	}

	[[nodiscard]] int HitTest(int y) const {
		if (m_visible_rows.empty()) {
			return -1;
		}
		const int content_y = y + GetScrollPosition();
		auto it = std::lower_bound(m_visible_rows.begin(), m_visible_rows.end(), content_y, [](const FlatVisualRow& row, int target_y) {
			return (row.y + row.height) <= target_y;
		});
		if (it != m_visible_rows.end() && content_y >= it->y && content_y < it->y + it->height) {
			return static_cast<int>(it - m_visible_rows.begin());
		}
		return -1;
	}

	std::vector<SearchResultGroup> m_groups;
	std::vector<FlatVisualRow> m_visible_rows;
	std::string m_filter_query;
	std::optional<uint32_t> m_filter_number;
	int m_selected_group = -1;
	int m_selected_child = -1;
	int m_hover_row = -1;
};

// ============================================================================
// SearchResultWindow
// ============================================================================

SearchResultWindow::SearchResultWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);

	// 1. Universal TilesetGridToolbar (sorting, search box, stacked layout)
	TilesetGridToolbar::Actions actions;
	actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		OnSort(key, dir);
	};
	actions.clear_sort = [this]() {
		OnClearSort();
	};
	actions.filter = [this](const std::string& query) {
		ApplyFilter(query);
	};
	actions.filter_cleared = [this]() {
		ApplyFilter("");
	};

	m_gridToolbar = new TilesetGridToolbar(this, std::move(actions), TilesetGridToolbar::Part_Sort | TilesetGridToolbar::Part_Search);
	m_gridToolbar->SetStacked(true);

	// Add custom actions to the AUI toolbar: Expand All, Collapse All
	if (auto* aui = m_gridToolbar->GetToolBar()) {
		const wxColour icon_color = Theme::Get(Theme::Role::Text);

		aui->AddTool(TOOL_EXPAND_ALL, wxEmptyString,
			IMAGE_MANAGER.GetBitmapBundle(ICON_ANGLES_DOWN, icon_color),
			"Expand all item sub-trees");
		aui->AddTool(TOOL_COLLAPSE_ALL, wxEmptyString,
			IMAGE_MANAGER.GetBitmapBundle(ICON_ANGLES_RIGHT, icon_color),
			"Collapse all item sub-trees");

		aui->Realize();

		aui->Bind(wxEVT_TOOL, [this](wxCommandEvent& event) {
			switch (event.GetId()) {
				case TOOL_EXPAND_ALL:
					ExpandAll();
					break;
				case TOOL_COLLAPSE_ALL:
					CollapseAll();
					break;
				default:
					event.Skip();
					break;
			}
		});
	}

	root_sizer->Add(m_gridToolbar, 0, wxEXPAND);

	// 2. Hierarchical NanoVG Search Canvas
	m_canvas = new SearchResultCanvas(this);
	root_sizer->Add(m_canvas, 1, wxEXPAND);

	// 3. Status summary bar at bottom
	auto* status_panel = new wxPanel(this, wxID_ANY);
	status_panel->SetBackgroundColour(Theme::Get(Theme::Role::FooterSurface));
	auto* status_sizer = new wxBoxSizer(wxHORIZONTAL);
	m_summaryLabel = new wxStaticText(status_panel, wxID_ANY, "Ready");
	m_summaryLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	m_summaryLabel->SetFont(m_summaryLabel->GetFont().Bold());
	status_sizer->Add(m_summaryLabel, 1, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(4));
	status_panel->SetSizer(status_sizer);

	root_sizer->Add(status_panel, 0, wxEXPAND);

	SetSizer(root_sizer);
	Layout();
}

SearchResultWindow::~SearchResultWindow() = default;

void SearchResultWindow::SetSearchResults(std::vector<SearchResultGroup> groups, SearchQuery query) {
	m_all_groups = std::move(groups);
	m_current_query = std::move(query);
	if (m_canvas) {
		if (m_hasSort) {
			m_canvas->SetGroups(m_all_groups);
			m_canvas->SortGroups(m_sortKey, m_sortDir);
		} else {
			m_canvas->SetGroups(m_all_groups);
			m_canvas->SortGroupsByDefault();
		}
	}
	UpdateSummary();
}

void SearchResultWindow::Clear() {
	m_all_groups.clear();
	if (m_canvas) {
		m_canvas->SetGroups({});
	}
	UpdateSummary();
}

void SearchResultWindow::RefreshCurrentSearch() {
	if (!g_gui.IsEditorOpen()) {
		Clear();
		return;
	}

	auto results = SearchEngine::Execute(g_gui.GetCurrentMap(), m_current_query);
	SetSearchResults(std::move(results), m_current_query);
}

void SearchResultWindow::ApplyFilter(const std::string& query) {
	if (m_canvas) {
		m_canvas->SetFilter(query);
	}
	UpdateSummary();
}

void SearchResultWindow::ExpandAll() {
	if (m_canvas) {
		m_canvas->ExpandAll();
	}
}

void SearchResultWindow::CollapseAll() {
	if (m_canvas) {
		m_canvas->CollapseAll();
	}
}

void SearchResultWindow::OnSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_hasSort = true;
	m_sortKey = key;
	m_sortDir = dir;
	if (m_canvas) {
		m_canvas->SortGroups(key, dir);
	}
	UpdateSummary();
}

void SearchResultWindow::OnClearSort() {
	m_hasSort = false;
	if (m_canvas) {
		m_canvas->SortGroupsByDefault();
	}
	UpdateSummary();
}

void SearchResultWindow::UpdateSummary() {
	if (!m_summaryLabel || !m_canvas) {
		return;
	}

	const size_t groups = m_canvas->GetTotalGroupCount();
	const size_t matches = m_canvas->GetTotalMatchCount();
	const std::string title = m_current_query.query_title.empty() ? "Results" : m_current_query.query_title;

	m_summaryLabel->SetLabel(wxstr(std::format("{}: {} groups | {} matches", title, groups, matches)));
}
