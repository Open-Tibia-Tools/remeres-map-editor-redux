#include "util/nanovg_table_ctrl.h"
#include "ui/theme.h"
#include "util/nvg_utils.h"
#include "util/image_manager.h"
#include <glad/glad.h>
#include <algorithm>
#include <numeric>

NanoVGTableCtrl::NanoVGTableCtrl(wxWindow* parent, wxWindowID id, long style) :
	NanoVGCanvas(parent, id, style) {
	Bind(wxEVT_SIZE, &NanoVGTableCtrl::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &NanoVGTableCtrl::OnMouseDown, this);
	Bind(wxEVT_RIGHT_DOWN, &NanoVGTableCtrl::OnRightDown, this);
	Bind(wxEVT_LEFT_DCLICK, &NanoVGTableCtrl::OnLeftDClick, this);
	Bind(wxEVT_MOTION, &NanoVGTableCtrl::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &NanoVGTableCtrl::OnLeave, this);
	Bind(wxEVT_KEY_DOWN, &NanoVGTableCtrl::OnKeyDown, this);
}

NanoVGTableCtrl::~NanoVGTableCtrl() = default;

int NanoVGTableCtrl::HeaderHeight() const {
	return FromDIP(24);
}

int NanoVGTableCtrl::RowHeight() const {
	return FromDIP(22);
}

wxSize NanoVGTableCtrl::DoGetBestClientSize() const {
	return FromDIP(wxSize(220, 200));
}

void NanoVGTableCtrl::SetColumns(std::vector<TableColumn> columns) {
	columns_ = std::move(columns);
	Refresh();
}

void NanoVGTableCtrl::SetRows(std::vector<TableRow> rows) {
	rows_ = std::move(rows);

	// Try to restore selection by key or ID if available
	selected_index_ = -1;
	for (size_t i = 0; i < rows_.size(); ++i) {
		if (selected_id_ != 0 && rows_[i].id == selected_id_) {
			selected_index_ = static_cast<int>(i);
			break;
		}
		if (!selected_key_.empty() && rows_[i].key == selected_key_) {
			selected_index_ = static_cast<int>(i);
			break;
		}
	}

	UpdateMetrics();
	Refresh();
}

void NanoVGTableCtrl::ClearRows() {
	rows_.clear();
	selected_index_ = -1;
	UpdateMetrics();
	Refresh();
}

void NanoVGTableCtrl::UpdateRow(size_t index, TableRow row) {
	if (index < rows_.size()) {
		rows_[index] = std::move(row);
		Refresh();
	}
}

void NanoVGTableCtrl::UpdateRowCells(size_t index, std::vector<std::string> cells) {
	if (index < rows_.size()) {
		rows_[index].cells = std::move(cells);
		Refresh();
	}
}

const TableRow* NanoVGTableCtrl::GetRow(size_t index) const {
	if (index < rows_.size()) {
		return &rows_[index];
	}
	return nullptr;
}

const TableRow* NanoVGTableCtrl::GetSelectedRow() const {
	if (selected_index_ >= 0 && selected_index_ < static_cast<int>(rows_.size())) {
		return &rows_[selected_index_];
	}
	return nullptr;
}

void NanoVGTableCtrl::SetSelection(int index, bool notify) {
	if (index < -1 || index >= static_cast<int>(rows_.size())) {
		return;
	}

	selected_index_ = index;
	if (selected_index_ >= 0) {
		selected_id_ = rows_[selected_index_].id;
		selected_key_ = rows_[selected_index_].key;
	} else {
		selected_id_ = 0;
		selected_key_.clear();
	}

	Refresh();

	if (notify && on_select_ && selected_index_ >= 0) {
		on_select_(selected_index_, rows_[selected_index_]);
	}
}

void NanoVGTableCtrl::SelectById(uint64_t id, bool notify) {
	selected_id_ = id;
	int found_idx = -1;
	for (size_t i = 0; i < rows_.size(); ++i) {
		if (rows_[i].id == id) {
			found_idx = static_cast<int>(i);
			selected_key_ = rows_[i].key;
			break;
		}
	}
	SetSelection(found_idx, notify);
	if (found_idx >= 0) {
		EnsureVisible(found_idx);
	}
}

void NanoVGTableCtrl::SelectByKey(const std::string& key, bool notify) {
	selected_key_ = key;
	int found_idx = -1;
	for (size_t i = 0; i < rows_.size(); ++i) {
		if (rows_[i].key == key) {
			found_idx = static_cast<int>(i);
			selected_id_ = rows_[i].id;
			break;
		}
	}
	SetSelection(found_idx, notify);
	if (found_idx >= 0) {
		EnsureVisible(found_idx);
	}
}

void NanoVGTableCtrl::ClearSelection() {
	selected_index_ = -1;
	selected_id_ = 0;
	selected_key_.clear();
	Refresh();
}

void NanoVGTableCtrl::EnsureVisible(int index) {
	if (index < 0 || index >= static_cast<int>(rows_.size())) {
		return;
	}

	const int row_h = RowHeight();
	const int client_h = GetClientSize().y - HeaderHeight();
	if (client_h <= 0) {
		return;
	}

	const int item_top = index * row_h;
	const int item_bottom = item_top + row_h;
	const int scroll_pos = GetScrollPosition();

	if (item_top < scroll_pos) {
		SetScrollPosition(item_top);
		Refresh();
	} else if (item_bottom > scroll_pos + client_h) {
		SetScrollPosition(item_bottom - client_h);
		Refresh();
	}
}

void NanoVGTableCtrl::SetSort(int column, bool ascending) {
	sort_column_ = column;
	sort_ascending_ = ascending;

	if (on_sort_) {
		on_sort_(sort_column_, sort_ascending_);
	} else {
		ApplyDefaultSort();
	}

	// Resync selection index after sort
	if (selected_id_ != 0 || !selected_key_.empty()) {
		for (size_t i = 0; i < rows_.size(); ++i) {
			if ((selected_id_ != 0 && rows_[i].id == selected_id_) ||
				(!selected_key_.empty() && rows_[i].key == selected_key_)) {
				selected_index_ = static_cast<int>(i);
				break;
			}
		}
	}

	Refresh();
}

void NanoVGTableCtrl::ApplyDefaultSort() {
	if (sort_column_ < 0 || sort_column_ >= static_cast<int>(columns_.size())) {
		return;
	}

	const size_t col = static_cast<size_t>(sort_column_);
	const bool asc = sort_ascending_;

	std::stable_sort(rows_.begin(), rows_.end(), [col, asc](const TableRow& a, const TableRow& b) {
		const std::string& va = (col < a.cells.size()) ? a.cells[col] : "";
		const std::string& vb = (col < b.cells.size()) ? b.cells[col] : "";

		// Try numeric comparison if both are valid numbers
		char* end_a = nullptr;
		char* end_b = nullptr;
		double num_a = std::strtod(va.c_str(), &end_a);
		double num_b = std::strtod(vb.c_str(), &end_b);

		if (end_a != va.c_str() && *end_a == '\0' && end_b != vb.c_str() && *end_b == '\0') {
			return asc ? (num_a < num_b) : (num_a > num_b);
		}

		int cmp = wxStricmp(wxstr(va), wxstr(vb));
		return asc ? (cmp < 0) : (cmp > 0);
	});
}

void NanoVGTableCtrl::UpdateMetrics() {
	const int total_h = HeaderHeight() + static_cast<int>(rows_.size()) * RowHeight();
	UpdateScrollbar(total_h);
}

std::vector<int> NanoVGTableCtrl::CalculateColumnWidths(int total_width) const {
	std::vector<int> widths(columns_.size(), 0);
	if (columns_.empty()) {
		return widths;
	}

	int fixed_sum = 0;
	int flex_count = 0;

	for (size_t i = 0; i < columns_.size(); ++i) {
		if (columns_[i].width > 0) {
			widths[i] = FromDIP(columns_[i].width);
			fixed_sum += widths[i];
		} else {
			++flex_count;
		}
	}

	const int remaining = std::max(0, total_width - fixed_sum);
	if (flex_count > 0) {
		const int per_flex = remaining / flex_count;
		int distributed = 0;
		for (size_t i = 0; i < columns_.size(); ++i) {
			if (columns_[i].width <= 0) {
				widths[i] = per_flex;
				distributed += per_flex;
			}
		}
		// Allocate remainder to last flex column
		for (int i = static_cast<int>(columns_.size()) - 1; i >= 0; --i) {
			if (columns_[i].width <= 0) {
				widths[i] += (remaining - distributed);
				break;
			}
		}
	}

	return widths;
}

int NanoVGTableCtrl::HitTestHeader(int x) const {
	const auto widths = CalculateColumnWidths(GetClientSize().x);
	int cur_x = 0;
	for (size_t i = 0; i < widths.size(); ++i) {
		if (x >= cur_x && x < cur_x + widths[i]) {
			return static_cast<int>(i);
		}
		cur_x += widths[i];
	}
	return -1;
}

int NanoVGTableCtrl::HitTestRow(int y) const {
	const int header_h = HeaderHeight();
	if (y < header_h) {
		return -1;
	}

	const int content_y = (y - header_h) + GetScrollPosition();
	const int row_h = RowHeight();
	if (row_h <= 0) {
		return -1;
	}

	const int idx = content_y / row_h;
	if (idx >= 0 && idx < static_cast<int>(rows_.size())) {
		return idx;
	}
	return -1;
}

void NanoVGTableCtrl::OnSize(wxSizeEvent& event) {
	UpdateMetrics();
	Refresh();
	event.Skip();
}

void NanoVGTableCtrl::OnMouseDown(wxMouseEvent& event) {
	const int y = event.GetY();
	const int header_h = HeaderHeight();

	if (y < header_h) {
		const int col = HitTestHeader(event.GetX());
		if (col >= 0 && col < static_cast<int>(columns_.size()) && columns_[col].sortable) {
			const bool new_asc = (sort_column_ == col) ? !sort_ascending_ : true;
			SetSort(col, new_asc);
		}
	} else {
		const int row = HitTestRow(y);
		if (row >= 0 && row < static_cast<int>(rows_.size())) {
			const TableRow& r = rows_[row];
			bool toggled = false;
			if (r.is_group && r.group_col >= 0 && r.group_col < static_cast<int>(columns_.size())) {
				const auto widths = CalculateColumnWidths(GetClientSize().x);
				int col_x = 0;
				for (int i = 0; i < r.group_col; ++i) {
					col_x += widths[i];
				}
				const int arrow_left = col_x + r.indent_level * FromDIP(14);
				const int arrow_right = arrow_left + FromDIP(22);
				if (event.GetX() >= arrow_left && event.GetX() <= arrow_right) {
					if (on_toggle_group_) {
						on_toggle_group_(row, r);
						toggled = true;
					}
				}
			}
			if (!toggled) {
				SetSelection(row, true);
			}
		}
	}

	SetFocus();
}

void NanoVGTableCtrl::OnRightDown(wxMouseEvent& event) {
	const int y = event.GetY();
	const int row = HitTestRow(y);
	if (row >= 0 && row < static_cast<int>(rows_.size())) {
		SetSelection(row, true);
	}
	SetFocus();

	if (on_context_menu_) {
		const TableRow* r = (row >= 0 && row < static_cast<int>(rows_.size())) ? &rows_[row] : nullptr;
		on_context_menu_(row, r, event.GetPosition());
	}
}

void NanoVGTableCtrl::OnLeftDClick(wxMouseEvent& event) {
	const int y = event.GetY();
	const int header_h = HeaderHeight();

	if (y >= header_h) {
		const int row = HitTestRow(y);
		if (row >= 0 && row < static_cast<int>(rows_.size())) {
			SetSelection(row, true);
			if (on_activate_) {
				on_activate_(row, rows_[row]);
			}
		}
	}
}

void NanoVGTableCtrl::OnMotion(wxMouseEvent& event) {
	const int y = event.GetY();
	const int header_h = HeaderHeight();

	int new_hover_header = -1;
	int new_hover_row = -1;

	if (y < header_h) {
		new_hover_header = HitTestHeader(event.GetX());
	} else {
		new_hover_row = HitTestRow(y);
	}

	if (new_hover_header != hover_header_col_ || new_hover_row != hover_row_) {
		hover_header_col_ = new_hover_header;
		hover_row_ = new_hover_row;
		Refresh();
	}

	event.Skip();
}

void NanoVGTableCtrl::OnLeave(wxMouseEvent& event) {
	if (hover_header_col_ != -1 || hover_row_ != -1) {
		hover_header_col_ = -1;
		hover_row_ = -1;
		Refresh();
	}
	event.Skip();
}

void NanoVGTableCtrl::OnKeyDown(wxKeyEvent& event) {
	if (rows_.empty()) {
		event.Skip();
		return;
	}

	const int key = event.GetKeyCode();
	int next = selected_index_;

	switch (key) {
		case WXK_UP:
			next = (selected_index_ <= 0) ? 0 : selected_index_ - 1;
			break;
		case WXK_DOWN:
			next = (selected_index_ < 0) ? 0 : std::min(static_cast<int>(rows_.size()) - 1, selected_index_ + 1);
			break;
		case WXK_HOME:
			next = 0;
			break;
		case WXK_END:
			next = static_cast<int>(rows_.size()) - 1;
			break;
		case WXK_PAGEUP: {
			const int page_rows = std::max(1, (GetClientSize().y - HeaderHeight()) / RowHeight());
			next = std::max(0, selected_index_ - page_rows);
			break;
		}
		case WXK_PAGEDOWN: {
			const int page_rows = std::max(1, (GetClientSize().y - HeaderHeight()) / RowHeight());
			next = std::min(static_cast<int>(rows_.size()) - 1, selected_index_ + page_rows);
			break;
		}
		case WXK_RIGHT:
			if (selected_index_ >= 0 && selected_index_ < static_cast<int>(rows_.size())) {
				const TableRow& r = rows_[selected_index_];
				if (r.is_group && !r.is_expanded && on_toggle_group_) {
					on_toggle_group_(selected_index_, r);
					return;
				}
			}
			break;
		case WXK_LEFT:
			if (selected_index_ >= 0 && selected_index_ < static_cast<int>(rows_.size())) {
				const TableRow& r = rows_[selected_index_];
				if (r.is_group && r.is_expanded && on_toggle_group_) {
					on_toggle_group_(selected_index_, r);
					return;
				}
			}
			break;
		case WXK_RETURN:
		case WXK_NUMPAD_ENTER:
			if (selected_index_ >= 0 && selected_index_ < static_cast<int>(rows_.size()) && on_activate_) {
				on_activate_(selected_index_, rows_[selected_index_]);
			}
			return;
		default:
			event.Skip();
			return;
	}

	if (next != selected_index_) {
		SetSelection(next, true);
		EnsureVisible(next);
	}
}

void NanoVGTableCtrl::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	const int header_h = HeaderHeight();
	const int row_h = RowHeight();
	const int scroll_pos = GetScrollPosition();
	const auto col_widths = CalculateColumnWidths(width);

	std::vector<int> col_starts(columns_.size(), 0);
	int cur_x = 0;
	for (size_t c = 0; c < columns_.size(); ++c) {
		col_starts[c] = cur_x;
		cur_x += col_widths[c];
	}

	// ------------------------------------------------------------------------
	// 1. Draw Rows (Scissored below header)
	// ------------------------------------------------------------------------
	nvgSave(vg);
	// Scissor content area in scrolled space: top is (header_h + scroll_pos)
	nvgScissor(vg, 0.0f, static_cast<float>(header_h + scroll_pos), static_cast<float>(width), static_cast<float>(std::max(0, height - header_h)));

	const int first_row = std::max(0, scroll_pos / row_h);
	const int last_row = std::min(static_cast<int>(rows_.size()), (scroll_pos + height - header_h + row_h - 1) / row_h + 1);

	for (int i = first_row; i < last_row; ++i) {
		const float row_y = static_cast<float>(header_h + i * row_h);
		const bool is_selected = (i == selected_index_) ||
			(selected_id_ != 0 && rows_[i].id == selected_id_) ||
			(!selected_key_.empty() && rows_[i].key == selected_key_);
		const bool is_hovered = (i == hover_row_);

		// Row background
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, row_y, static_cast<float>(width), static_cast<float>(row_h));

		if (is_selected) {
			NVGcolor sel_col = NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent));
			sel_col.a = 1.0f; // Solid vibrant highlight
			nvgFillColor(vg, sel_col);
		} else if (is_hovered) {
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBaseHover)));
		} else {
			if (i % 2 == 1) {
				nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBase)));
			} else {
				nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Surface)));
			}
		}
		nvgFill(vg);

		// Subtle row bottom divider line
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0.0f, row_y + row_h);
		nvgLineTo(vg, static_cast<float>(width), row_y + row_h);
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Border)));
		nvgStroke(vg);

		// Row text color
		const wxColour text_wx = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);
		const NVGcolor text_col = NvgUtils::ToNvColor(text_wx);

		// Cell contents
		const TableRow& row_data = rows_[i];
		const float cell_center_y = row_y + row_h * 0.5f;

		for (size_t c = 0; c < columns_.size(); ++c) {
			const float cell_x = static_cast<float>(col_starts[c]);
			const float cell_w = static_cast<float>(col_widths[c]);
			if (cell_w <= 0.0f) {
				continue;
			}

			nvgSave(vg);
			nvgScissor(vg, cell_x + 3.0f, row_y, cell_w - 6.0f, static_cast<float>(row_h));

			if (row_data.has_color_swatch && row_data.color_swatch_col == static_cast<int>(c)) {
				const float swatch_w = static_cast<float>(FromDIP(14));
				const float swatch_h = static_cast<float>(FromDIP(14));
				const float swatch_x = cell_x + (cell_w - swatch_w) * 0.5f;
				const float swatch_y = row_y + (row_h - swatch_h) * 0.5f;

				nvgBeginPath(vg);
				nvgRoundedRect(vg, swatch_x, swatch_y, swatch_w, swatch_h, 3.0f);
				nvgFillColor(vg, nvgRGBA(row_data.swatch_r, row_data.swatch_g, row_data.swatch_b, 255));
				nvgFill(vg);

				nvgStrokeWidth(vg, 1.0f);
				nvgStrokeColor(vg, nvgRGBA(
					static_cast<unsigned char>(row_data.swatch_r * 0.65f),
					static_cast<unsigned char>(row_data.swatch_g * 0.65f),
					static_cast<unsigned char>(row_data.swatch_b * 0.65f),
					255
				));
				nvgStroke(vg);
			} else if (row_data.icon_col == static_cast<int>(c)) {
				if (!row_data.icon_name.empty()) {
					const float icon_sz = static_cast<float>(FromDIP(14));
					const float icon_x = cell_x + (cell_w - icon_sz) * 0.5f;
					const float icon_y = row_y + (row_h - icon_sz) * 0.5f;

					const wxColour icon_tint = is_selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Accent);
					int tex = IMAGE_MANAGER.GetNanoVGImage(vg, row_data.icon_name, icon_tint);
					if (tex > 0) {
						NVGpaint imgPaint = nvgImagePattern(vg, icon_x, icon_y, icon_sz, icon_sz, 0.0f, tex, 1.0f);
						nvgBeginPath(vg);
						nvgRect(vg, icon_x, icon_y, icon_sz, icon_sz);
						nvgFillPaint(vg, imgPaint);
						nvgFill(vg);
					}
				}
			} else if (c < row_data.cells.size()) {
				const std::string& text = row_data.cells[c];
				nvgFontFace(vg, "sans");
				nvgFontSize(vg, 12.0f);
				nvgFillColor(vg, text_col);

				int align = columns_[c].align | NVG_ALIGN_MIDDLE;
				nvgTextAlign(vg, align);

				float text_x = cell_x + 6.0f;
				if (columns_[c].align & NVG_ALIGN_CENTER) {
					text_x = cell_x + cell_w * 0.5f;
				} else if (columns_[c].align & NVG_ALIGN_RIGHT) {
					text_x = cell_x + cell_w - 6.0f;
				} else if (row_data.group_col == static_cast<int>(c)) {
					// Apply indent and expander arrow
					if (row_data.indent_level > 0) {
						text_x += row_data.indent_level * static_cast<float>(FromDIP(14));
					}
					if (row_data.is_group) {
						const float arrow_sz = static_cast<float>(FromDIP(7));
						const float ax = text_x + static_cast<float>(FromDIP(2));
						const NVGcolor arrow_col = is_selected ? text_col : NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle));

						nvgBeginPath(vg);
						if (row_data.is_expanded) {
							// Downward pointing triangle (expanded)
							nvgMoveTo(vg, ax, cell_center_y - arrow_sz * 0.4f);
							nvgLineTo(vg, ax + arrow_sz, cell_center_y - arrow_sz * 0.4f);
							nvgLineTo(vg, ax + arrow_sz * 0.5f, cell_center_y + arrow_sz * 0.5f);
						} else {
							// Rightward pointing triangle (collapsed)
							nvgMoveTo(vg, ax, cell_center_y - arrow_sz * 0.5f);
							nvgLineTo(vg, ax + arrow_sz * 0.866f, cell_center_y);
							nvgLineTo(vg, ax, cell_center_y + arrow_sz * 0.5f);
						}
						nvgClosePath(vg);
						nvgFillColor(vg, arrow_col);
						nvgFill(vg);

						text_x += static_cast<float>(FromDIP(16));
					}
				}

				nvgText(vg, text_x, cell_center_y, text.c_str(), nullptr);
			}

			nvgRestore(vg); // Restore cell scissor
		}
	}
	nvgRestore(vg); // Restore rows scissor

	// ------------------------------------------------------------------------
	// 2. Draw Fixed Header (Undo scroll translation to fix at top)
	// ------------------------------------------------------------------------
	nvgSave(vg);
	nvgTranslate(vg, 0, static_cast<float>(scroll_pos));

	// Header background
	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(header_h));
	nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::RaisedSurface)));
	nvgFill(vg);

	// Header bottom border
	nvgBeginPath(vg);
	nvgMoveTo(vg, 0.0f, static_cast<float>(header_h));
	nvgLineTo(vg, static_cast<float>(width), static_cast<float>(header_h));
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Border)));
	nvgStroke(vg);

	const float header_center_y = header_h * 0.5f;

	for (size_t c = 0; c < columns_.size(); ++c) {
		const float col_x = static_cast<float>(col_starts[c]);
		const float col_w = static_cast<float>(col_widths[c]);
		if (col_w <= 0.0f) {
			continue;
		}

		// Hover highlight on sortable column header
		if (static_cast<int>(c) == hover_header_col_ && columns_[c].sortable) {
			nvgBeginPath(vg);
			nvgRect(vg, col_x, 0.0f, col_w, static_cast<float>(header_h));
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBaseHover)));
			nvgFill(vg);
		}

		// Right divider line between column headers
		if (c + 1 < columns_.size()) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, col_x + col_w, 2.0f);
			nvgLineTo(vg, col_x + col_w, static_cast<float>(header_h) - 2.0f);
			nvgStrokeWidth(vg, 1.0f);
			nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Border)));
			nvgStroke(vg);
		}

		// Column title text
		nvgSave(vg);
		nvgScissor(vg, col_x + 2.0f, 0.0f, col_w - 4.0f, static_cast<float>(header_h));

		nvgFontFace(vg, "sans");
		nvgFontSize(vg, 12.0f);
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));

		int align = columns_[c].align | NVG_ALIGN_MIDDLE;
		nvgTextAlign(vg, align);

		float title_x = col_x + 6.0f;
		if (columns_[c].align & NVG_ALIGN_CENTER) {
			title_x = col_x + col_w * 0.5f;
		} else if (columns_[c].align & NVG_ALIGN_RIGHT) {
			title_x = col_x + col_w - 6.0f;
		}

		// Check if active sorted column
		std::string title_str = columns_[c].title;
		if (static_cast<int>(c) == sort_column_) {
			title_str += (sort_ascending_ ? "  ▲" : "  ▼");
		}

		nvgText(vg, title_x, header_center_y, title_str.c_str(), nullptr);
		nvgRestore(vg);
	}

	nvgRestore(vg); // Restore header translation
}
