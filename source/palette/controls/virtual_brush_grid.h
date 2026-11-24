#ifndef RME_PALETTE_CONTROLS_VIRTUAL_BRUSH_GRID_H_
#define RME_PALETTE_CONTROLS_VIRTUAL_BRUSH_GRID_H_

#include "app/main.h"
#include "util/nanovg_canvas.h"
#include "util/grid_cell_renderer.h"
#include "palette/panels/brush_panel.h"
#include "palette/controls/brush_grid_selection.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// A single user pick (click, or the search jump on release). Client data is the picked Brush*;
// it is sent before the brush becomes current. The event propagates to the palette window.
wxDECLARE_EVENT(EVT_VIRTUAL_BRUSH_GRID_PICKED, wxCommandEvent);
// The selection is being dragged. Sent synchronously from the motion handler so a handler can
// run wxDropSource::DoDragDrop; it reads GetSelectedBrushes() from the event object.
wxDECLARE_EVENT(EVT_VIRTUAL_BRUSH_GRID_BEGIN_DRAG, wxCommandEvent);

/**
 * @class VirtualBrushGrid
 * @brief High-performance brush grid using NanoVG for GPU-accelerated rendering.
 *
 * This control displays a grid of brush icons with virtual scrolling,
 * supporting thousands of brushes at 60fps. Uses texture caching for
 * efficient sprite rendering.
 */
class VirtualBrushGrid : public NanoVGCanvas, public BrushBoxInterface {
public:
	/**
	 * @brief Constructs a VirtualBrushGrid.
	 * @param parent Parent window
	 * @param _tileset The dynamic tileset containing brushes
	 * @param iconSizePx Icon render size in pixels
	 */
	VirtualBrushGrid(wxWindow* parent, const DynamicTilesetDefinition* _tileset, int iconSizePx);
	~VirtualBrushGrid() override;
	wxWindow* GetSelfWindow() override {
		return this;
	}

	// BrushBoxInterface
	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	bool SelectBrush(const Brush* brush) override;

	using DisplayMode = TilesetDisplayMode;

	// Cell metrics live in util/grid_cell_renderer.h so every grid that shows
	// tiles agrees on them.
	void SetDisplayMode(DisplayMode mode) override;

	// Toolbar support
	void SetSort(TilesetSortKey key, TilesetSortDirection dir) override;
	void ClearSort() override;
	void SetShowLabels(bool show) override;
	void SetTileSize(int sizePx) override;
	void SetFilterQuery(const std::string& query, const std::vector<Brush*>* overrideSource = nullptr) override;
	void SetBrushIndicators(bool dogEar);
	void SetGridSpacing(int spacingPx);
	bool GetIndicatorDogEar() const { return m_indicatorDogEar; }
	int GetGridSpacing() const { return m_gridSpacing; }

	// Multi-selection (Ctrl toggles, Shift selects a range). Only a single pick changes the
	// current brush; GetSelectedBrush() keeps returning that brush.
	[[nodiscard]] std::vector<Brush*> GetSelectedBrushes() const;
	void ClearSelection();
	[[nodiscard]] Brush* BrushAt(int x, int y) const;
	// Client position of the current brush's cell (for a keyboard-invoked context menu).
	[[nodiscard]] wxPoint GetSelectedCellPosition() const;
	// Rebuilds the list from the tileset now (a reorder keeps the size, so it is not noticed
	// otherwise); keeps the selection and the scroll position.
	void ReloadBrushes();
	// Makes the selected brush current like a click on it (used by a committed search).
	void CommitSelectedBrush();

protected:
	/**
	 * @brief Performs NanoVG rendering of the brush grid.
	 * @param vg NanoVG context
	 * @param width Canvas width
	 * @param height Canvas height
	 */
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;

	wxSize DoGetBestClientSize() const override;

	// Event Handlers
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseUp(wxMouseEvent& event);
	void OnRightDown(wxMouseEvent& event);
	void OnMotion(wxMouseEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnShow(wxShowEvent& event);

	// Internal helpers
	void UpdateLayout();
	// Scrolls the selected brush into view. When the grid is not on screen yet
	// (its palette page is still being switched to) its size is not final, so
	// the scroll is kept pending and redone once it is shown and sized.
	void EnsureSelectedVisible();
	bool m_pendingScrollToSelected = false;
	void RefreshBrushList();
	void ApplySort();
	// Makes `brush` current: activates the palette, sends EVT_VIRTUAL_BRUSH_GRID_PICKED and
	// selects it (when it already is the current brush, it only sends the event and focuses the map).
	void CommitPick(Brush* brush);
	// Search results (override brushes) jump to the brush's own palette page.
	void CommitSearchPick(Brush* brush);
	void SendPicked(Brush* brush);
	// Recomputes selected_index and m_selectedFlags from m_selection. Called after every change of
	// the selection or the displayed list, so painting never searches the selection.
	void SyncSelection();
	int HitTest(int x, int y) const;
	// Display index of the brush cell at (x, y), or nullopt for empty space.
	[[nodiscard]] std::optional<size_t> CellAt(int x, int y) const;
	wxRect GetItemRect(int index) const;
	void DrawBrushItem(NVGcontext* vg, int index, const wxRect& rect);

	DisplayMode display_mode = DisplayMode::Grid;
	int icon_size_px;
	// Display index of the current (primary) brush, -1 when it is not shown.
	int selected_index;
	BrushGridSelection m_selection;
	// One entry per display index: 1 when that brush is a selection member (read while painting).
	std::vector<uint8_t> m_selectedFlags;
	// Drag state of the left button: armed on a selected cell, started past the drag threshold.
	bool m_dragArmed = false;
	bool m_dragStarted = false;
	wxPoint m_dragOrigin;
	// Display index of the cell under the last left press, nullopt when none.
	std::optional<size_t> m_pressIndex;
	// A search result picked on press; it jumps on release unless a drag starts.
	Brush* m_pendingSearchPick = nullptr;
	int hover_index;
	int columns;
	int item_size;
	size_t observed_tileset_size;

	std::vector<Brush*> m_display_brushes;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;
	bool m_hasSort = false;
	bool m_showLabels = false;
	std::string m_filterQuery;
	std::vector<Brush*> m_overrideBrushes;
	bool m_hasOverrideBrushes = false;
	bool m_indicatorDogEar = false;
	int m_gridSpacing = rme::ui::grid::kPadding;

	// Optimization: wrapped label cache shared with the item grids.
	rme::ui::grid::LabelCache m_labelCache;

	// Animation state
	wxTimer m_animTimer;
	float hover_anim = 0.0f;
	void OnTimer(wxTimerEvent& event);
};

#endif
