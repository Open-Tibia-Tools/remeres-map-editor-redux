#ifndef RME_PALETTE_CONTROLS_TILESET_GRID_TOOLBAR_H_
#define RME_PALETTE_CONTROLS_TILESET_GRID_TOOLBAR_H_

#include "app/main.h"
#include "util/grid_view_options.h"

#include <wx/panel.h>

#include <functional>
#include <string>

class wxAuiToolBar;
class wxSearchCtrl;

/**
 * @class TilesetGridToolbar
 * @brief Toolbar for grids of tileset icons: sorting, label toggle, icon size
 *        and a search box.
 *
 * The palette tileset grid and the replace tool item grids share this control.
 * It owns only the chrome and the current state; every user change is reported
 * through callbacks so each grid applies it in its own way.
 */
class TilesetGridToolbar : public wxPanel {
public:
	struct Actions {
		/** Sort by the given key and direction (the popup chose the key). */
		std::function<void(TilesetSortKey, TilesetSortDirection)> sort;
		/** Back to the grid's natural order. */
		std::function<void()> clear_sort;
		/** Label toggle under the icons. */
		std::function<void(bool)> show_labels;
		/** Icon render size in pixels (32, 64, 128). */
		std::function<void(int)> icon_size;
		/** Live text typed in the search box. */
		std::function<void(const std::string&)> filter;
		/** Search committed (Enter or the search button). */
		std::function<void()> filter_committed;
		/** Search cleared through the cancel button or Escape. */
		std::function<void()> filter_cleared;
		/** Search box lost focus (used to persist the filter state). */
		std::function<void()> filter_focus_lost;
		/** "Filter all" toggled; only used when with_filter_all is set. */
		std::function<void(bool)> filter_all;
	};

	enum ToolID {
		TOOL_SORT_AZ = wxID_HIGHEST + 7001,
		TOOL_SORT_ZA,
		TOOL_TOGGLE_LABELS,
		TOOL_CHANGE_SIZE,
		TOOL_FILTER_ALL,
	};

	TilesetGridToolbar(wxWindow* parent, Actions actions, bool with_filter_all = false);

	/** Reflects the current state without invoking the callbacks. */
	void SetState(bool show_labels, int icon_size, bool has_sort, TilesetSortKey sort_key, TilesetSortDirection sort_dir, bool filter_all);
	void SetFilterQuery(const std::string& query);
	std::string GetFilterQuery() const;

	bool GetShowLabels() const {
		return m_showLabels;
	}
	int GetIconSize() const {
		return m_iconSize;
	}
	bool GetFilterAll() const {
		return m_filterAll;
	}

	void ApplyTheme();
	void FocusSearch();

	/**
	 * Stacked layout: the tools on one row and the search box below, for the
	 * narrow palette sidebar. The default is a single row.
	 */
	void SetStacked(bool stacked);

private:
	void OnToolClick(wxCommandEvent& event);
	void OnSortButtonClick(TilesetSortDirection dir, int toolId);
	void OnSizeButtonClick(int toolId);
	void OnSearchChanged(wxCommandEvent& event);
	void OnSearchCancel(wxCommandEvent& event);
	void OnSearchCancelIfNeeded();
	void ApplySort(TilesetSortKey key, TilesetSortDirection dir);
	void SetIconSize(int sizePx);
	void ApplySearchTheme();

	Actions m_actions;
	wxAuiToolBar* m_toolbar = nullptr;
	wxSearchCtrl* m_searchCtrl = nullptr;
	wxBoxSizer* m_toolbarRow = nullptr;
	wxBoxSizer* m_searchRow = nullptr;
	wxBoxSizer* m_rootSizer = nullptr;

	bool m_showLabels = false;
	int m_iconSize = 32;
	bool m_hasSort = false;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;
	bool m_filterAll = false;
};

#endif
