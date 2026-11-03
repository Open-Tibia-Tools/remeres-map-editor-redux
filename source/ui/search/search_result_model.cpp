#include "ui/search/search_result_model.h"
#include "ui/search/search_card_layout.h"
#include "item_definitions/core/item_definition_store.h"
#include "util/grid_cell_renderer.h"

#include <algorithm>
#include <cctype>

namespace rme::ui::search {

namespace {

std::string ToLower(std::string_view str) {
	std::string lower;
	lower.reserve(str.size());
	for (const char ch : str) {
		lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
	}
	return lower;
}

} // namespace

void SearchResultModel::SetGroups(std::vector<SearchResultGroup> groups) {
	m_groups = std::move(groups);
	m_selected_group = -1;
	m_selected_child = -1;
	m_hover_row = -1;
	RebuildVisibleRows();
}

void SearchResultModel::Clear() {
	m_groups.clear();
	m_visible_rows.clear();
	m_selected_group = -1;
	m_selected_child = -1;
	m_hover_row = -1;
	m_total_content_height = 0;
}

void SearchResultModel::SetFilter(const std::string& query) {
	m_filter_query = ToLower(query);
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

void SearchResultModel::ExpandAll() {
	for (auto& g : m_groups) {
		g.expanded = true;
	}
	RebuildVisibleRows();
}

void SearchResultModel::CollapseAll() {
	for (auto& g : m_groups) {
		g.expanded = false;
	}
	RebuildVisibleRows();
}

void SearchResultModel::Sort(TilesetSortKey key, TilesetSortDirection dir) {
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
	RebuildVisibleRows();
}

void SearchResultModel::SortByDefault() {
	std::sort(m_groups.begin(), m_groups.end(), [](const auto& a, const auto& b) {
		if (a.children.size() != b.children.size()) {
			return a.children.size() > b.children.size();
		}
		return a.name < b.name;
	});
	RebuildVisibleRows();
}

size_t SearchResultModel::GetTotalMatchCount() const {
	size_t count = 0;
	for (const auto& g : m_groups) {
		count += g.children.size();
	}
	return count;
}

void SearchResultModel::RebuildVisibleRows() {
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

	m_total_content_height = current_y + padding;
}

int SearchResultModel::HitTest(int content_y) const {
	if (m_visible_rows.empty()) {
		return -1;
	}
	auto it = std::lower_bound(m_visible_rows.begin(), m_visible_rows.end(), content_y, [](const FlatVisualRow& row, int target_y) {
		return (row.y + row.height) <= target_y;
	});
	if (it != m_visible_rows.end() && content_y >= it->y && content_y < it->y + it->height) {
		return static_cast<int>(it - m_visible_rows.begin());
	}
	return -1;
}

bool SearchResultModel::MatchesGroupChildren(const SearchResultGroup& g) const {
	for (const auto& c : g.children) {
		if (MatchesChild(c)) {
			return true;
		}
	}
	return false;
}

bool SearchResultModel::MatchesGroup(const SearchResultGroup& g) const {
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

bool SearchResultModel::MatchesChild(const SearchResultChildEntry& c) const {
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
	if (!c.text.empty() && ToLower(c.text).find(m_filter_query) != std::string::npos) {
		return true;
	}
	if (!c.subtitle.empty() && ToLower(c.subtitle).find(m_filter_query) != std::string::npos) {
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
		if (def && ToLower(std::string(def.name())).find(m_filter_query) != std::string::npos) {
			return true;
		}
	}
	if (!c.formatted_pos.empty() && c.formatted_pos.find(m_filter_query) != std::string::npos) {
		return true;
	}
	return false;
}

} // namespace rme::ui::search
