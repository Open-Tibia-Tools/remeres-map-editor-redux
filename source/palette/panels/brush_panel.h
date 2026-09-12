#ifndef RME_PALETTE_PANELS_BRUSH_PANEL_H_
#define RME_PALETTE_PANELS_BRUSH_PANEL_H_

#include "app/main.h"
#include "game/material_database.h"
#include "brushes/brush.h"

enum BrushListType {
	BRUSHLIST_ICONS_32,
	BRUSHLIST_ICONS_64,
	BRUSHLIST_ICONS_128,
	BRUSHLIST_LISTBOX,
	BRUSHLIST_TEXT_LISTBOX,
};

enum class TilesetSortKey {
	ID,
	Name
};

enum class TilesetSortDirection {
	Ascending,
	Descending
};

class BrushBoxInterface {
public:
	BrushBoxInterface(const DynamicTilesetDefinition* _tileset) :
		tileset(_tileset), loaded(false) {
		ASSERT(tileset);
	}
	virtual ~BrushBoxInterface() { }

	virtual wxWindow* GetSelfWindow() = 0;

	// Select the first brush
	virtual void SelectFirstBrush() = 0;
	// Returns the currently selected brush (First brush if panel is not loaded)
	virtual Brush* GetSelectedBrush() const = 0;
	// Select the brush in the parameter, this only changes the look of the panel
	virtual bool SelectBrush(const Brush* brush) = 0;

	virtual void SetSort(TilesetSortKey key, TilesetSortDirection dir) { }
	virtual void SetShowLabels(bool show) { }
	virtual void SetTileSize(int sizePx) { }
	virtual void SetFilterQuery(const std::string& query, const std::vector<Brush*>* overrideSource = nullptr) { }

protected:
	const DynamicTilesetDefinition* const tileset;
	bool loaded;
};

class BrushPanel : public wxPanel {
public:
	BrushPanel(wxWindow* parent);
	~BrushPanel();

	// Interface
	// Flushes this panel and consequent views will feature reloaded data
	void InvalidateContents();
	// Loads the content (This must be called before the panel is displayed, else it will appear empty
	void LoadContents();

	// Sets the display type (list or icons)
	void SetListType(BrushListType ltype);
	void SetListType(wxString ltype);
	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void SetShowLabels(bool show);
	void SetTileSize(int sizePx);
	void SetFilterQuery(const std::string& query, const std::vector<Brush*>* overrideSource = nullptr);
	// Assigns a tileset to this list
	void AssignTileset(const DynamicTilesetDefinition* tileset);

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (First brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatbrush);

	// Called when the window is about to be displayed
	void OnSwitchIn();
	// Called when this page is hidden
	void OnSwitchOut();

protected:
	const DynamicTilesetDefinition* tileset;
	wxSizer* sizer;
	BrushBoxInterface* brushbox;
	bool loaded;
	BrushListType list_type;
	bool has_sort = false;
	TilesetSortKey sort_key = TilesetSortKey::Name;
	TilesetSortDirection sort_dir = TilesetSortDirection::Ascending;
	bool show_labels = false;
	int tile_size_px = 32;
	std::string filter_query;
	std::vector<Brush*> override_brushes;
	bool has_override_brushes = false;
};

#endif

