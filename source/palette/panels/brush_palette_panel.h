#ifndef RME_PALETTE_PANELS_BRUSH_PALETTE_PANEL_H_
#define RME_PALETTE_PANELS_BRUSH_PALETTE_PANEL_H_

#include "palette/palette_common.h"
#include "palette/panels/brush_panel.h"
#include "app/settings.h"

#include <wx/aui/auibar.h>
#include <unordered_map>

class wxSearchCtrl;

class BrushPalettePanel : public PalettePanel {
public:
	enum ToolID {
		TOOL_SORT_AZ = wxID_HIGHEST + 6001,
		TOOL_SORT_ZA,
		TOOL_TOGGLE_LABELS,
		TOOL_CHANGE_SIZE,
		TOOL_FILTER_ALL,
	};

	enum MenuID {
		MENU_SORT_BY_ID = wxID_HIGHEST + 6011,
		MENU_SORT_BY_NAME,
		MENU_SORT_DEFAULT,
		MENU_SIZE_32,
		MENU_SIZE_64,
		MENU_SIZE_128,
	};

	BrushPalettePanel(wxWindow* parent, const DynamicPaletteDefinition& palette, wxWindowID id = wxID_ANY);
	~BrushPalettePanel() override;

	// Interface
	// Flushes this panel and consequent views will feature reloaded data
	void InvalidateContents() override;
	// Loads the currently displayed page
	void LoadCurrentContents() override;
	// Loads all content in this panel
	void LoadAllContents() override;

	wxString GetName() const override;

	// Sets the display type (list or icons)
	void SetListType(BrushListType ltype);
	void SetListType(wxString ltype);

	// Select the first brush
	void SelectFirstBrush() override;
	// Returns the currently selected brush (first brush if panel is not loaded)
	Brush* GetSelectedBrush() const override;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatbrush) override;

	// Called when this page is displayed
	void OnSwitchIn() override;

	// Event handler for child window
	void OnSwitchingPage(wxChoicebookEvent& event);
	void OnPageChanged(wxChoicebookEvent& event);

	// Toolbar operations
	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void ClearSort();
	void SetShowLabels(bool show);
	void SetTileSize(int sizePx);
	void ApplyTheme();

	// Search and filter operations
	void ResetFilter();
	bool JumpToTilesetAndBrush(std::string_view tilesetName, const Brush* brush);

protected:
	void OnToolClick(wxCommandEvent& event);
	void OnSortButtonClick(TilesetSortDirection dir, int toolId);
	void OnSizeButtonClick(int toolId);
	void OnSearchText(wxCommandEvent& event);
	void OnSearchCancel(wxCommandEvent& event);
	void ApplyFilter();

	static void EnsureDefaultsLoaded();
	void LoadPaletteFilters();
	void SavePaletteFilters();

	std::string palette_name;
	const DynamicPaletteDefinition* m_paletteDef;
	wxChoicebook* choicebook;
	wxAuiToolBar* toolbar;
	wxSearchCtrl* m_searchCtrl;
	wxAuiToolBar* m_searchToolbar;
	std::string m_filterQuery;
	bool m_filterAll;

	TilesetSortKey m_sortKey;
	TilesetSortDirection m_sortDir;
	bool m_hasSort;
	bool m_showLabels;
	int m_tileSize;

	static TilesetSortKey s_defaultSortKey;
	static TilesetSortDirection s_defaultSortDir;
	static bool s_defaultHasSort;
	static bool s_defaultShowLabels;
	static int s_defaultTileSize;
	static bool s_defaultFilterAll;
	static bool s_defaultsLoaded;

	// No size_panel, it was unused

	std::unordered_map<wxWindow*, Brush*> remembered_brushes;
};

#endif

