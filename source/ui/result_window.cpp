#include "app/main.h"
#include "ui/result_window.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/search/search_card_layout.h"
#include "ui/search/search_card_drawer.h"
#include "ui/search/search_result_model.h"
#include "util/image_manager.h"
#include "util/nanovg_canvas.h"
#include "util/grid_cell_renderer.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "editor/editor.h"
#include "editor/operations/search_query.h"
#include "item_definitions/core/item_definition_store.h"

#include <wx/aui/auibar.h>
#include <algorithm>
#include <format>

namespace {

enum CustomToolID {
	TOOL_EXPAND_ALL = wxID_HIGHEST + 8001,
	TOOL_COLLAPSE_ALL
};

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
		m_model.SetGroups(std::move(groups));
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	void ExpandAll() {
		m_model.ExpandAll();
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	void CollapseAll() {
		m_model.CollapseAll();
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	void SetFilter(const std::string& query) {
		m_model.SetFilter(query);
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	void SortGroups(TilesetSortKey key, TilesetSortDirection dir) {
		m_model.Sort(key, dir);
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	void SortGroupsByDefault() {
		m_model.SortByDefault();
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
	}

	[[nodiscard]] size_t GetTotalGroupCount() const { return m_model.GetTotalGroupCount(); }
	[[nodiscard]] size_t GetTotalMatchCount() const { return m_model.GetTotalMatchCount(); }

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override {
		const int scroll_pos = GetScrollPosition();
		const auto& visible_rows = m_model.GetVisibleRows();

		if (visible_rows.empty()) {
			rme::ui::search::SearchCardDrawer::DrawEmptyState(vg, width, height, scroll_pos, g_gui.IsEditorOpen());
			return;
		}

		const int indent = FromDIP(rme::ui::search::kTreeIndent);
		const int padding = rme::ui::grid::kPadding;
		const int visible_top = scroll_pos;
		const int visible_bottom = scroll_pos + height;

		for (size_t i = 0; i < visible_rows.size(); ++i) {
			const auto& vrow = visible_rows[i];
			if (vrow.y + vrow.height < visible_top || vrow.y > visible_bottom) {
				continue;
			}

			const bool is_group = (vrow.kind == rme::ui::search::RowKind::GroupHeader);
			const int card_x = is_group ? padding : padding + indent;
			const int card_w = is_group ? width - padding * 2 : width - padding * 2 - indent;
			const wxRect card_rect(card_x, vrow.y, std::max(1, card_w), vrow.height);

			const bool is_hovered = (static_cast<int>(i) == m_model.GetHoverRow());

			if (is_group) {
				const auto& group = m_model.GetGroups()[vrow.group_index];
				const bool is_selected = (static_cast<int>(vrow.group_index) == m_model.GetSelectedGroup() && m_model.GetSelectedChild() == -1);
				rme::ui::search::SearchCardDrawer::DrawGroupHeader(vg, this, card_rect, group, vrow.group_index, is_selected, is_hovered);
			} else {
				const auto& group = m_model.GetGroups()[vrow.group_index];
				const auto& child = group.children[vrow.child_index];
				const bool is_selected = (static_cast<int>(vrow.group_index) == m_model.GetSelectedGroup() && static_cast<int>(vrow.child_index) == m_model.GetSelectedChild());

				if (vrow.kind == rme::ui::search::RowKind::Container) {
					rme::ui::search::SearchCardDrawer::DrawContainerCard(vg, this, card_rect, child, vrow.child_index, group.children.size(), is_selected, is_hovered);
				} else {
					rme::ui::search::SearchCardDrawer::DrawChildCard(vg, this, card_rect, child, vrow.child_index, group.children.size(), is_selected, is_hovered);
				}
			}
		}
	}

private:
	void OnSize(wxSizeEvent& event) {
		m_model.RebuildVisibleRows();
		UpdateScrollbar(m_model.GetTotalContentHeight());
		Refresh();
		event.Skip();
	}

	void OnMouseDown(wxMouseEvent& event) {
		SetFocus();
		const int content_y = event.GetY() + GetScrollPosition();
		const int hit = m_model.HitTest(content_y);
		if (hit < 0 || hit >= static_cast<int>(m_model.GetVisibleRows().size())) {
			return;
		}

		const auto& vrow = m_model.GetVisibleRows()[hit];
		if (vrow.kind == rme::ui::search::RowKind::GroupHeader) {
			m_model.ToggleGroupExpanded(vrow.group_index);
			m_model.SetSelected(static_cast<int>(vrow.group_index), -1);
			UpdateScrollbar(m_model.GetTotalContentHeight());
			Refresh();
		} else {
			m_model.SetSelected(static_cast<int>(vrow.group_index), static_cast<int>(vrow.child_index));
			Refresh();

			const auto& group = m_model.GetGroups()[vrow.group_index];
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
		const int content_y = event.GetY() + GetScrollPosition();
		const int hit = m_model.HitTest(content_y);
		if (hit != m_model.GetHoverRow()) {
			m_model.SetHoverRow(hit);
			Refresh();
		}
		HandleMouseHover(event.GetX(), event.GetY());
	}

	void OnMouseLeave(wxMouseEvent& WXUNUSED(event)) {
		if (m_model.GetHoverRow() != -1) {
			m_model.SetHoverRow(-1);
			Refresh();
		}
		SetToolTip("");
	}

	void HandleMouseHover(int mouse_x, int mouse_y) {
		const int content_y = mouse_y + GetScrollPosition();
		const int hit = m_model.HitTest(content_y);
		if (hit < 0 || hit >= static_cast<int>(m_model.GetVisibleRows().size())) {
			SetToolTip("");
			return;
		}

		const auto& vrow = m_model.GetVisibleRows()[hit];
		const auto& group = m_model.GetGroups()[vrow.group_index];

		if (vrow.kind == rme::ui::search::RowKind::GroupHeader) {
			SetToolTip(wxstr(std::format("{} ({} {})", group.name, group.children.size(), group.children.size() == 1 ? "match" : "matches")));
			return;
		}

		const auto& child = group.children[vrow.child_index];

		if (vrow.kind == rme::ui::search::RowKind::Container) {
			const int indent = FromDIP(rme::ui::search::kTreeIndent);
			const int padding = rme::ui::grid::kPadding;
			const int card_x = padding + indent;
			const int card_w = std::max(1, GetClientSize().x - padding * 2 - indent);
			const wxRect card_rect(card_x, vrow.y, card_w, vrow.height);

			const auto geo = rme::ui::search::CardGeometry::ForContainer(card_rect, group.children.size(), this);

			// Check if mouse is over main container 32x32 icon on the left
			if (geo.GetMainIconRect().Contains(mouse_x, content_y)) {
				const auto def = g_item_definitions.get(child.item_id);
				const std::string item_name = def ? std::string(def.name()) : group.name;
				SetToolTip(wxstr(item_name));
				return;
			}

			// Check if mouse is over Row 1 (contained items)
			if (content_y >= geo.container_icons_y && content_y < geo.container_icons_y + geo.icon_sz && mouse_x >= geo.content_x) {
				if (child.container_items.empty()) {
					if (mouse_x < geo.content_x + geo.icon_sz) {
						SetToolTip("Empty container");
						return;
					}
				} else {
					for (size_t i = 0; i < child.container_items.size(); ++i) {
						if (geo.GetContainedSlotRect(i).Contains(mouse_x, content_y)) {
							const auto& item_entry = child.container_items[i];
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
		event.Skip();
	}

	rme::ui::search::SearchResultModel m_model;
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
	m_current_query = std::move(query);
	if (m_canvas) {
		m_canvas->SetGroups(std::move(groups));
		if (m_hasSort) {
			m_canvas->SortGroups(m_sortKey, m_sortDir);
		} else {
			m_canvas->SortGroupsByDefault();
		}
	}
	UpdateSummary();
}

void SearchResultWindow::Clear() {
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
