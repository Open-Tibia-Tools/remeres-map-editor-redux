#ifndef RME_UTIL_NANOVG_TABLE_CTRL_H_
#define RME_UTIL_NANOVG_TABLE_CTRL_H_

#include "app/main.h"
#include "util/nanovg_canvas.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nanovg.h>

struct TableColumn {
	std::string id;
	std::string title;
	int width = 0;              // DIP width (0 = flexible / takes remaining space)
	int align = NVG_ALIGN_LEFT; // NVG_ALIGN_LEFT, NVG_ALIGN_CENTER, NVG_ALIGN_RIGHT
	bool sortable = true;
};

struct TableRow {
	uint64_t id = 0;
	std::string key;
	std::vector<std::string> cells;
	std::string icon_name;      // SVG asset path (e.g. ICON_HOUSE_USER or empty)
	int icon_col = -1;          // Column to draw icon in (-1 for none)
	void* client_data = nullptr;

	// Optional color swatch
	bool has_color_swatch = false;
	int color_swatch_col = -1;
	uint8_t swatch_r = 0;
	uint8_t swatch_g = 0;
	uint8_t swatch_b = 0;

	// Optional hierarchy / tree group
	int indent_level = 0;       // Indentation level (0 = root, 1 = child, etc.)
	int group_col = -1;         // Column containing the expander arrow and indentation (-1 for none)
	bool is_group = false;      // True if row can expand/collapse children
	bool is_expanded = false;   // Current expand/collapse state
};

/**
 * @class NanoVGTableCtrl
 * @brief High-performance virtual table control rendered via NanoVG (OpenGL).
 *
 * Provides a modern, dark/light theme aware table with:
 * - Fixed header with column titles and interactive sorting (▲/▼ indicators)
 * - Virtual row scrolling (constant 60fps regardless of row count)
 * - Rock-solid, focus-independent selection highlighting that stays vibrant
 *   when the user interacts with the MapCanvas or other editor windows
 * - Consistent typography matching the application theme
 */
class NanoVGTableCtrl : public NanoVGCanvas {
public:
	NanoVGTableCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, long style = wxVSCROLL | wxWANTS_CHARS);
	~NanoVGTableCtrl() override;

	// Structure & Data
	void SetColumns(std::vector<TableColumn> columns);
	void SetRows(std::vector<TableRow> rows);
	void ClearRows();
	void UpdateRow(size_t index, TableRow row);
	void UpdateRowCells(size_t index, std::vector<std::string> cells);
	void SetRowHeight(int dip);
	[[nodiscard]] int GetRowHeight() const { return row_height_dip_; }

	[[nodiscard]] size_t GetRowCount() const { return rows_.size(); }
	[[nodiscard]] const TableRow* GetRow(size_t index) const;
	[[nodiscard]] const TableRow* GetSelectedRow() const;

	// Selection
	void SetSelection(int index, bool notify = true);
	void SelectById(uint64_t id, bool notify = false);
	void SelectByKey(const std::string& key, bool notify = false);
	void ClearSelection();
	[[nodiscard]] int GetSelection() const { return selected_index_; }
	[[nodiscard]] uint64_t GetSelectedId() const { return selected_id_; }
	[[nodiscard]] const std::string& GetSelectedKey() const { return selected_key_; }
	void EnsureVisible(int index);

	// Sorting
	void SetSort(int column, bool ascending);
	[[nodiscard]] int GetSortColumn() const { return sort_column_; }
	[[nodiscard]] bool IsSortAscending() const { return sort_ascending_; }

	// Callbacks
	using SelectionCallback = std::function<void(int index, const TableRow& row)>;
	using ActivationCallback = std::function<void(int index, const TableRow& row)>;
	using SortCallback = std::function<void(int column, bool ascending)>;
	using ContextMenuCallback = std::function<void(int index, const TableRow* row, const wxPoint& pos)>;
	using ToggleGroupCallback = std::function<void(int index, const TableRow& row)>;

	void SetOnSelect(SelectionCallback callback) { on_select_ = std::move(callback); }
	void SetOnActivate(ActivationCallback callback) { on_activate_ = std::move(callback); }
	void SetOnSort(SortCallback callback) { on_sort_ = std::move(callback); }
	void SetOnContextMenu(ContextMenuCallback callback) { on_context_menu_ = std::move(callback); }
	void SetOnToggleGroup(ToggleGroupCallback callback) { on_toggle_group_ = std::move(callback); }

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	wxSize DoGetBestClientSize() const override;

	void OnSize(wxSizeEvent& event);
	void OnMouseDown(wxMouseEvent& event);
	void OnRightDown(wxMouseEvent& event);
	void OnLeftDClick(wxMouseEvent& event);
	void OnMotion(wxMouseEvent& event);
	void OnLeave(wxMouseEvent& event);
	void OnKeyDown(wxKeyEvent& event);

	[[nodiscard]] int HeaderHeight() const;
	[[nodiscard]] int RowHeight() const;
	[[nodiscard]] std::vector<int> CalculateColumnWidths(int total_width) const;
	[[nodiscard]] int HitTestHeader(int x) const;
	[[nodiscard]] int HitTestRow(int y) const;

	void UpdateMetrics();
	void ApplyDefaultSort();

	std::vector<TableColumn> columns_;
	std::vector<TableRow> rows_;
	int row_height_dip_ = 22;

	int selected_index_ = -1;
	uint64_t selected_id_ = 0;
	std::string selected_key_;

	int hover_row_ = -1;
	int hover_header_col_ = -1;

	int sort_column_ = -1;
	bool sort_ascending_ = true;

	SelectionCallback on_select_;
	ActivationCallback on_activate_;
	SortCallback on_sort_;
	ContextMenuCallback on_context_menu_;
	ToggleGroupCallback on_toggle_group_;
};

#endif // RME_UTIL_NANOVG_TABLE_CTRL_H_
