#ifndef RME_LIBRARY_PANEL_H_
#define RME_LIBRARY_PANEL_H_

#include "ui/replace_tool/item_grid_panel.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <vector>
#include <string>
#include <unordered_map>

class Brush;
class Editor;

class LibraryPanel : public wxPanel, public ItemGridPanel::Listener {
public:
	class Listener {
	public:
		virtual ~Listener() { }
		virtual void OnLibraryItemSelected(uint16_t itemId) = 0;
		virtual void OnAddItemsAsRules(const std::vector<uint16_t>& itemIds) { }
		virtual void OnAutoMatchRules(Brush* brush) { }
	};

	LibraryPanel(wxWindow* parent, Listener* listener, Editor* editor = nullptr);
	virtual ~LibraryPanel();

	void DetachEditor();

	// ItemGridPanel::Listener
	void OnItemSelected(ItemGridPanel* source, uint16_t itemId) override;

	void RefreshViewportItems();
	void RefreshSelectionItems();

private:
	void InitLayout();
	void PopulateBrushGrid();
	void PopulateRelatedItems(uint16_t brushLookId);
	/** Selects the given brush again after the grid was rebuilt, then refreshes the related pane. */
	void RestoreBrushSelection(uint16_t brushLookId);
	uint16_t GetSidFromCid(uint16_t cid);

	void OnNotebookPageChanged(wxNotebookEvent& event);
	void OnMapSelectionChanged(size_t count);

	Listener* m_listener = nullptr;
	Editor* m_editor = nullptr;

	wxNotebook* m_notebook = nullptr;
	TilesetGridToolbar* m_itemsToolbar = nullptr;
	TilesetGridToolbar* m_brushesToolbar = nullptr;
	TilesetGridToolbar* m_viewportToolbar = nullptr;
	TilesetGridToolbar* m_selectionToolbar = nullptr;

	ItemGridPanel* m_allItemsGrid = nullptr;
	ItemGridPanel* m_brushGrid = nullptr;
	ItemGridPanel* m_relatedGrid = nullptr;
	ItemGridPanel* m_viewportGrid = nullptr;
	ItemGridPanel* m_selectionGrid = nullptr;

	wxButton* m_refreshViewportBtn = nullptr;
	wxButton* m_addViewportRulesBtn = nullptr;
	wxButton* m_addSelectionRulesBtn = nullptr;
	wxButton* m_fillUnmatchedBtn = nullptr;

	std::unordered_map<uint16_t, uint16_t> m_cidToSidCache;
	std::unordered_map<uint16_t, Brush*> m_brushLookup;
	Brush* m_currentBrush = nullptr;
	bool m_viewportInitialized = false;
};

#endif
