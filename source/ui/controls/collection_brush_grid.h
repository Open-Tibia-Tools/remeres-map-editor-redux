#ifndef RME_UI_CONTROLS_COLLECTION_BRUSH_GRID_H_
#define RME_UI_CONTROLS_COLLECTION_BRUSH_GRID_H_

#include "util/nanovg_canvas.h"
#include "util/grid_view_options.h"
#include "util/grid_cell_renderer.h"

#include <functional>
#include <string>
#include <vector>

class Brush;

namespace rme::ui {

class CollectionBrushGrid : public NanoVGCanvas {
public:
	explicit CollectionBrushGrid(wxWindow* parent, wxWindowID id = wxID_ANY);
	~CollectionBrushGrid() override;

	void SetBrushes(const std::vector<Brush*>& brushes);
	void SetFilter(const std::string& query);
	void SetDiscardCallback(std::function<void(Brush*)> cb) {
		m_discardCallback = std::move(cb);
	}

	void SetIconSize(int sizePx);
	void SetShowLabels(bool show);
	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void ClearSort();

	const std::vector<Brush*>& GetBrushes() const {
		return m_allBrushes;
	}

	size_t GetBrushCount() const {
		return m_filteredBrushes.size();
	}

	Brush* GetSelectedBrush() const;

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	wxSize DoGetBestClientSize() const override;

	void OnMouseDown(wxMouseEvent& event);
	void OnMotion(wxMouseEvent& event);
	void OnLeave(wxMouseEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnTimer(wxTimerEvent& event);

private:
	void UpdateLayout();
	int HitTest(int x, int y) const;
	wxRect GetItemRect(int index) const;
	void ApplyFilterAndSort();
	void DrawBrushItem(NVGcontext* vg, int index, const wxRect& rect);

	std::vector<Brush*> m_allBrushes;
	std::vector<Brush*> m_filteredBrushes;
	std::string m_filterQuery;

	int m_iconSize = 32;
	int m_itemSize = 36;
	bool m_showLabels = false;
	bool m_hasSort = false;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;

	int m_selectedIndex = -1;
	int m_hoverIndex = -1;
	float m_hoverAnim = 0.0f;
	int m_columns = 1;
	bool m_badgeHovered = false;

	wxTimer m_animTimer;
	rme::ui::grid::LabelCache m_labelCache;
	std::function<void(Brush*)> m_discardCallback;
};

} // namespace rme::ui

#endif // RME_UI_CONTROLS_COLLECTION_BRUSH_GRID_H_
