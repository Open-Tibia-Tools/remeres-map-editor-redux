//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_BRUSH_ITEMS_WINDOW_H_
#define RME_UI_BRUSH_ITEMS_WINDOW_H_

#include "app/main.h"
#include "util/virtual_item_grid.h"
#include "util/nanovg_canvas.h"
#include <wx/panel.h>
#include <vector>
#include <string>

class Brush;
class TilesetGridToolbar;
class wxStaticText;

class BrushItemsGrid : public VirtualItemGrid {
public:
	explicit BrushItemsGrid(wxWindow* parent);
	~BrushItemsGrid() override = default;

	void SetItems(std::vector<uint16_t> items);
	void SetFilter(const std::string& query);
	void SetSelectedItem(uint16_t id);
	[[nodiscard]] uint16_t GetSelectedItem() const {
		return m_selectedItemId;
	}
	[[nodiscard]] bool ContainsItem(uint16_t id) const;

	size_t GetItemCount() const override;
	uint16_t GetItem(size_t index) const override;
	wxString GetItemName(size_t index) const override;
	wxString GetItemTooltip(size_t index) const override;
	bool IsItemSelected(int index) const override;

	void SetSort(TilesetSortKey key, TilesetSortDirection dir) override;
	void ClearSort() override;

protected:
	void OnItemSelected(int index) override;

private:
	void ApplyFilterAndSort();

	std::vector<uint16_t> m_rawItems;
	std::vector<uint16_t> m_displayItems;
	std::string m_filterQuery;
	uint16_t m_selectedItemId = 0;
};

class BrushBottomCardWidget : public NanoVGCanvas {
public:
	explicit BrushBottomCardWidget(wxWindow* parent);
	~BrushBottomCardWidget() override = default;

	void SetBrush(const Brush* brush);
	void SetIndicatorDogEar(bool dogEar);

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	wxSize DoGetBestClientSize() const override;

private:
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseEnter(wxMouseEvent& event);
	void OnMouseLeave(wxMouseEvent& event);

	const Brush* m_brush = nullptr;
	bool m_hovered = false;
	bool m_indicatorDogEar = false;
};

class BrushItemsWindow : public wxPanel {
public:
	explicit BrushItemsWindow(wxWindow* parent);
	~BrushItemsWindow() override = default;

	void SetActiveBrush(const Brush* brush);
	void RefreshItems();
	void OnBottomCardClicked();

private:
	void BuildUI();

	const Brush* m_currentSourceBrush = nullptr;
	std::string m_lastSourceBrushName;

	TilesetGridToolbar* m_toolbar = nullptr;
	BrushItemsGrid* m_grid = nullptr;
	BrushBottomCardWidget* m_bottomCard = nullptr;
	wxStaticText* m_emptyLabel = nullptr;
};

#endif // RME_UI_BRUSH_ITEMS_WINDOW_H_
