#ifndef RME_UI_SEARCH_SEARCH_RESULT_MODEL_H_
#define RME_UI_SEARCH_SEARCH_RESULT_MODEL_H_

#include "editor/operations/search_query.h"
#include "palette/controls/tileset_grid_toolbar.h"

#include <optional>
#include <string>
#include <vector>

namespace rme::ui::search {

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

class SearchResultModel {
public:
	SearchResultModel() = default;

	void SetGroups(std::vector<SearchResultGroup> groups);
	void Clear();

	void SetFilter(const std::string& query);
	void ExpandAll();
	void CollapseAll();

	void Sort(TilesetSortKey key, TilesetSortDirection dir);
	void SortByDefault();

	void RebuildVisibleRows();

	[[nodiscard]] int HitTest(int content_y) const;

	[[nodiscard]] const std::vector<FlatVisualRow>& GetVisibleRows() const { return m_visible_rows; }
	[[nodiscard]] const std::vector<SearchResultGroup>& GetGroups() const { return m_groups; }
	[[nodiscard]] std::vector<SearchResultGroup>& GetGroups() { return m_groups; }

	[[nodiscard]] size_t GetTotalGroupCount() const { return m_groups.size(); }
	[[nodiscard]] size_t GetTotalMatchCount() const;

	[[nodiscard]] int GetTotalContentHeight() const { return m_total_content_height; }

	[[nodiscard]] int GetSelectedGroup() const { return m_selected_group; }
	[[nodiscard]] int GetSelectedChild() const { return m_selected_child; }
	[[nodiscard]] int GetHoverRow() const { return m_hover_row; }

	void SetSelected(int group_idx, int child_idx) {
		m_selected_group = group_idx;
		m_selected_child = child_idx;
	}

	void SetHoverRow(int hover_row) {
		m_hover_row = hover_row;
	}

	void ToggleGroupExpanded(size_t group_idx) {
		if (group_idx < m_groups.size()) {
			m_groups[group_idx].expanded = !m_groups[group_idx].expanded;
			RebuildVisibleRows();
		}
	}

	[[nodiscard]] bool MatchesGroup(const SearchResultGroup& g) const;
	[[nodiscard]] bool MatchesChild(const SearchResultChildEntry& c) const;
	[[nodiscard]] bool MatchesGroupChildren(const SearchResultGroup& g) const;

private:
	std::vector<SearchResultGroup> m_groups;
	std::vector<FlatVisualRow> m_visible_rows;

	std::string m_filter_query;
	std::optional<uint32_t> m_filter_number;

	int m_selected_group = -1;
	int m_selected_child = -1;
	int m_hover_row = -1;
	int m_total_content_height = 0;
};

} // namespace rme::ui::search

#endif // RME_UI_SEARCH_SEARCH_RESULT_MODEL_H_
