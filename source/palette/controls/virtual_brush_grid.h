#ifndef RME_PALETTE_CONTROLS_VIRTUAL_BRUSH_GRID_H_
#define RME_PALETTE_CONTROLS_VIRTUAL_BRUSH_GRID_H_

#include "app/main.h"
#include "util/nanovg_canvas.h"
#include "palette/panels/brush_panel.h"

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

	enum class DisplayMode {
		Grid,
		List
	};

	static constexpr int LIST_ROW_HEIGHT = 36;
	static constexpr int GRID_PADDING = 4;
	static constexpr int GRID_ITEM_SIZE_BASE = 32;
	static constexpr int ICON_OFFSET = 2;

	void SetDisplayMode(DisplayMode mode);

	// Toolbar support
	void SetSort(TilesetSortKey key, TilesetSortDirection dir) override;
	void SetShowLabels(bool show) override;
	void SetTileSize(int sizePx) override;

	static constexpr int LABEL_HEIGHT = 16;

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
	void OnMotion(wxMouseEvent& event);
	void OnSize(wxSizeEvent& event);

	// Internal helpers
	void UpdateLayout();
	void RefreshBrushList();
	void ApplySort();
	int HitTest(int x, int y) const;
	wxRect GetItemRect(int index) const;
	void DrawBrushItem(NVGcontext* vg, int index, const wxRect& rect);

	DisplayMode display_mode = DisplayMode::Grid;
	int icon_size_px;
	int selected_index;
	int hover_index;
	int columns;
	int item_size;
	int padding;
	size_t observed_tileset_size;

	std::vector<Brush*> m_display_brushes;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;
	bool m_hasSort = false;
	bool m_showLabels = false;

	// Optimization: UTF8 name cache and truncated label cache
	mutable std::unordered_map<const Brush*, std::string> m_utf8NameCache;
	mutable std::unordered_map<const Brush*, std::string> m_truncatedLabelCache;

	// Animation state
	wxTimer m_animTimer;
	float hover_anim = 0.0f;
	void OnTimer(wxTimerEvent& event);
};

#endif
