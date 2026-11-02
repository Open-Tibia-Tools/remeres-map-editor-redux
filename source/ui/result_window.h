#ifndef RME_RESULT_WINDOW_H_
#define RME_RESULT_WINDOW_H_

#include "app/main.h"
#include "editor/operations/search_query.h"
#include "palette/controls/tileset_grid_toolbar.h"

#include <wx/panel.h>
#include <wx/stattext.h>
#include <string>
#include <vector>

class SearchResultWindow : public wxPanel {
public:
	explicit SearchResultWindow(wxWindow* parent);
	~SearchResultWindow() override;

	void SetSearchResults(std::vector<SearchResultGroup> groups, SearchQuery query = {});
	void Clear();
	void RefreshCurrentSearch();

	void ApplyFilter(const std::string& query);
	void ExpandAll();
	void CollapseAll();

	void OnSort(TilesetSortKey key, TilesetSortDirection dir);
	void OnClearSort();

	[[nodiscard]] const SearchQuery& GetCurrentQuery() const { return m_current_query; }

protected:
	void UpdateSummary();

private:
	class SearchResultCanvas;

	TilesetGridToolbar* m_gridToolbar = nullptr;
	SearchResultCanvas* m_canvas = nullptr;
	wxStaticText* m_summaryLabel = nullptr;

	SearchQuery m_current_query;
	bool m_hasSort = false;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;
};

#endif // RME_RESULT_WINDOW_H_
