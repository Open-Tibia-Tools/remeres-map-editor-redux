#include "app/settings.h"
#include "util/grid_cell_renderer.h"
#include "ui/find_item_window_views.h"

#include "brushes/creature/creature_brush.h"
#include "ui/gui.h"
#include "ui/theme.h"

#include <algorithm>
#include <limits>

wxDEFINE_EVENT(EVT_ADVANCED_FINDER_RESULT_RIGHT_ACTIVATE, wxCommandEvent);

AdvancedFinderResultsView::AdvancedFinderResultsView(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxVSCROLL | wxWANTS_CHARS) {
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	indicator_dogear_ = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	const int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	grid_spacing_ = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 4;
	bindEvents();
}

void AdvancedFinderResultsView::bindEvents() {
	Bind(wxEVT_SIZE, &AdvancedFinderResultsView::onSize, this);
	Bind(wxEVT_LEFT_DOWN, &AdvancedFinderResultsView::onMouseDown, this);
	Bind(wxEVT_MOTION, &AdvancedFinderResultsView::onMouseMove, this);
	Bind(wxEVT_LEAVE_WINDOW, &AdvancedFinderResultsView::onMouseLeave, this);
	Bind(wxEVT_MOUSEWHEEL, &AdvancedFinderResultsView::onMouseWheel, this);
	Bind(wxEVT_KEY_DOWN, &AdvancedFinderResultsView::onKeyDown, this);
	Bind(wxEVT_LEFT_DCLICK, &AdvancedFinderResultsView::onLeftDoubleClick, this);
	Bind(wxEVT_RIGHT_DCLICK, &AdvancedFinderResultsView::onRightDoubleClick, this);
	Bind(wxEVT_CONTEXT_MENU, &AdvancedFinderResultsView::onContextMenu, this);
}

void AdvancedFinderResultsView::SetMode(AdvancedFinderResultViewMode mode) {
	if (mode_ == mode) {
		return;
	}

	mode_ = mode;
	SetScrollStep(mode_ == AdvancedFinderResultViewMode::Grid ? std::max(1, gridCardHeight() + gridGap()) : std::max(1, listItemHeight()));
	updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
	EnsureSelectionVisible();
	refreshNow();
}

void AdvancedFinderResultsView::SetRows(std::vector<const AdvancedFinderCatalogRow*> rows, const AdvancedFinderSelectionKey& preferred_selection) {
	original_rows_ = std::move(rows);
	rows_ = original_rows_;
	empty_state_ = rows_.empty() ? EmptyState::Prompt : EmptyState::Rows;
	primary_message_.clear();
	secondary_message_.clear();

	if (has_sort_) {
		applySort();
	}

	if (rows_.empty()) {
		selected_index_ = -1;
		updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
		refreshNow();
		return;
	}

	selectDefaultRow(preferred_selection);
	updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
	EnsureSelectionVisible();
	refreshNow();
}

void AdvancedFinderResultsView::SetPrompt(std::string primary, std::string secondary) {
	empty_state_ = EmptyState::Prompt;
	primary_message_ = std::move(primary);
	secondary_message_ = std::move(secondary);
	rows_.clear();
	selected_index_ = -1;
	updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
	refreshNow();
}

void AdvancedFinderResultsView::SetNoMatches(std::string primary, std::string secondary) {
	empty_state_ = EmptyState::NoMatches;
	primary_message_ = std::move(primary);
	secondary_message_ = std::move(secondary);
	rows_.clear();
	selected_index_ = -1;
	updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
	refreshNow();
}

const AdvancedFinderCatalogRow* AdvancedFinderResultsView::GetSelectedRow() const {
	if (empty_state_ != EmptyState::Rows || selected_index_ < 0 || selected_index_ >= static_cast<int>(rows_.size())) {
		return nullptr;
	}

	return rows_[selected_index_];
}

int AdvancedFinderResultsView::GetSelectionIndex() const {
	return selected_index_;
}

void AdvancedFinderResultsView::SetSelectionIndex(int index) {
	setSelectionInternal(index, false);
	EnsureSelectionVisible();
	refreshNow();
}

void AdvancedFinderResultsView::EnsureSelectionVisible() {
	if (selected_index_ < 0 || selected_index_ >= static_cast<int>(rows_.size())) {
		return;
	}

	const wxRect rect = itemRect(static_cast<size_t>(selected_index_));
	const int scroll_pos = GetScrollPosition();
	const int view_height = GetClientSize().y;
	const int top = rect.y;
	const int bottom = rect.y + rect.height;

	if (top < scroll_pos) {
		SetScrollPosition(top);
	} else if (bottom > scroll_pos + view_height) {
		SetScrollPosition(bottom - view_height);
	}
}

void AdvancedFinderResultsView::onSize(wxSizeEvent& event) {
	cached_height_ = GetClientSize().y;
	updateLayoutMetrics(GetClientSize().x);
	EnsureSelectionVisible();
	event.Skip();
}

void AdvancedFinderResultsView::onMouseDown(wxMouseEvent& event) {
	const int index = hitTest(event.GetX(), event.GetY() + GetScrollPosition());
	if (index >= 0) {
		setSelectionInternal(index, true);
		EnsureSelectionVisible();
	}
	SetFocus();
}

void AdvancedFinderResultsView::onMouseMove(wxMouseEvent& event) {
	hover_position_ = wxPoint(event.GetX(), event.GetY());
	const int index = hitTest(event.GetX(), event.GetY() + GetScrollPosition());
	if (index != hover_index_ || (mode_ == AdvancedFinderResultViewMode::Grid && hover_index_ >= 0)) {
		hover_index_ = index;
		Refresh();
	}
	event.Skip();
}

void AdvancedFinderResultsView::onMouseLeave(wxMouseEvent& WXUNUSED(event)) {
	hover_position_ = wxDefaultPosition;
	if (hover_index_ != -1) {
		hover_index_ = -1;
		Refresh();
	}
}

void AdvancedFinderResultsView::onMouseWheel(wxMouseEvent& event) {
	event.Skip();
	CallAfter([this] {
		const wxPoint point = ScreenToClient(wxGetMousePosition());
		if (!GetClientRect().Contains(point)) {
			hover_position_ = wxDefaultPosition;
			hover_index_ = -1;
			Refresh();
			return;
		}

		hover_position_ = point;
		hover_index_ = hitTest(point.x, point.y + GetScrollPosition());
		Refresh();
	});
}

void AdvancedFinderResultsView::onKeyDown(wxKeyEvent& event) {
	if (rows_.empty() || empty_state_ != EmptyState::Rows) {
		event.Skip();
		return;
	}

	int next = selected_index_;
	const int columns = std::max(1, columns_);
	const int page_step = std::max(1, visibleRows()) * (mode_ == AdvancedFinderResultViewMode::Grid ? columns : 1);

	switch (event.GetKeyCode()) {
		case WXK_UP:
			next = (mode_ == AdvancedFinderResultViewMode::Grid) ? selected_index_ - columns : selected_index_ - 1;
			break;
		case WXK_DOWN:
			next = (mode_ == AdvancedFinderResultViewMode::Grid) ? selected_index_ + columns : selected_index_ + 1;
			break;
		case WXK_LEFT:
			if (mode_ == AdvancedFinderResultViewMode::Grid) {
				next = selected_index_ - 1;
			}
			break;
		case WXK_RIGHT:
			if (mode_ == AdvancedFinderResultViewMode::Grid) {
				next = selected_index_ + 1;
			}
			break;
		case WXK_HOME:
			next = 0;
			break;
		case WXK_END:
			next = static_cast<int>(rows_.size()) - 1;
			break;
		case WXK_PAGEUP:
			next = selected_index_ - page_step;
			break;
		case WXK_PAGEDOWN:
			next = selected_index_ + page_step;
			break;
		case WXK_RETURN:
		case WXK_NUMPAD_ENTER:
			sendActivateEvent();
			return;
		default:
			event.Skip();
			return;
	}

	next = std::clamp(next, 0, static_cast<int>(rows_.size()) - 1);
	if (next != selected_index_) {
		setSelectionInternal(next, true);
		EnsureSelectionVisible();
		return;
	}

	event.Skip();
}

void AdvancedFinderResultsView::onLeftDoubleClick(wxMouseEvent& event) {
	const int index = hitTest(event.GetX(), event.GetY() + GetScrollPosition());
	if (index >= 0) {
		setSelectionInternal(index, true);
		sendActivateEvent();
	}
	SetFocus();
}

void AdvancedFinderResultsView::onRightDoubleClick(wxMouseEvent& event) {
	const int index = hitTest(event.GetX(), event.GetY() + GetScrollPosition());
	if (index >= 0) {
		setSelectionInternal(index, true);
		wxCommandEvent activate_event(EVT_ADVANCED_FINDER_RESULT_RIGHT_ACTIVATE, GetId());
		activate_event.SetEventObject(this);
		activate_event.SetInt(selected_index_);
		ProcessWindowEvent(activate_event);
	}
	event.StopPropagation();
	SetFocus();
}

void AdvancedFinderResultsView::onContextMenu(wxContextMenuEvent& event) {
	event.StopPropagation();
}

void AdvancedFinderResultsView::SetIconSize(int size) {
	const int validSize = (size == 64 || size == 128) ? size : 32;
	if (icon_size_ != validSize) {
		icon_size_ = validSize;
		updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
		Refresh();
	}
}

void AdvancedFinderResultsView::SetGridSpacing(int spacing) {
	const int validSpacing = (spacing == 2 || spacing == 6) ? spacing : 4;
	if (grid_spacing_ != validSpacing) {
		grid_spacing_ = validSpacing;
		updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
		Refresh();
	}
}

void AdvancedFinderResultsView::SetBrushIndicators(bool dogEar) {
	if (indicator_dogear_ != dogEar) {
		indicator_dogear_ = dogEar;
		Refresh();
	}
}

void AdvancedFinderResultsView::SetShowLabels(bool show) {
	if (show_labels_ != show) {
		show_labels_ = show;
		updateLayoutMetrics(cached_width_ > 0 ? cached_width_ : GetClientSize().x);
		Refresh();
	}
}

void AdvancedFinderResultsView::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	has_sort_ = true;
	sort_key_ = key;
	sort_dir_ = dir;
	applySort();
	EnsureSelectionVisible();
	refreshNow();
}

void AdvancedFinderResultsView::ClearSort() {
	if (has_sort_) {
		has_sort_ = false;
		const auto* selected = GetSelectedRow();
		rows_ = original_rows_;
		if (selected) {
			for (size_t i = 0; i < rows_.size(); ++i) {
				if (rows_[i] == selected) {
					selected_index_ = static_cast<int>(i);
					break;
				}
			}
		}
		EnsureSelectionVisible();
		refreshNow();
	}
}

void AdvancedFinderResultsView::applySort() {
	if (!has_sort_ || rows_.empty()) {
		return;
	}
	const auto* selected = GetSelectedRow();
	std::stable_sort(rows_.begin(), rows_.end(), [this](const AdvancedFinderCatalogRow* a, const AdvancedFinderCatalogRow* b) {
		const int idA = a->server_id > 0 ? a->server_id : a->client_id;
		const int idB = b->server_id > 0 ? b->server_id : b->client_id;
		if (sort_key_ == TilesetSortKey::ID) {
			return sort_dir_ == TilesetSortDirection::Ascending ? (idA < idB) : (idA > idB);
		} else {
			if (a->lower_label != b->lower_label) {
				return sort_dir_ == TilesetSortDirection::Ascending ? (a->lower_label < b->lower_label) : (a->lower_label > b->lower_label);
			}
			return idA < idB;
		}
	});
	if (selected) {
		for (size_t i = 0; i < rows_.size(); ++i) {
			if (rows_[i] == selected) {
				selected_index_ = static_cast<int>(i);
				break;
			}
		}
	}
}

void AdvancedFinderResultsView::updateLayoutMetrics(int width) {
	cached_width_ = width;
	cached_height_ = GetClientSize().y;
	padding_ = grid_spacing_;
	gap_ = grid_spacing_;
	card_width_ = rme::ui::grid::CellWidth(icon_size_);
	card_height_ = rme::ui::grid::CellHeight(icon_size_, show_labels_);
	list_item_height_ = FromDIP(40) + grid_spacing_;

	if (mode_ == AdvancedFinderResultViewMode::Grid) {
		columns_ = columnsForWidth(width);
	} else {
		columns_ = 1;
	}

	updateScrollbarForLayout();
}

void AdvancedFinderResultsView::updateScrollbarForLayout() {
	if (empty_state_ != EmptyState::Rows || rows_.empty()) {
		UpdateScrollbar(0);
		return;
	}

	int content_height = 0;
	if (mode_ == AdvancedFinderResultViewMode::List) {
		content_height = std::max(1, listHeaderHeight()) + static_cast<int>(rows_.size()) * std::max(1, list_item_height_);
	} else {
		const int row_count = (static_cast<int>(rows_.size()) + columns_ - 1) / columns_;
		content_height = padding_ * 2 + row_count * std::max(1, card_height_);
		if (row_count > 1) {
			content_height += (row_count - 1) * gap_;
		}
	}

	UpdateScrollbar(content_height);
	SetScrollStep(mode_ == AdvancedFinderResultViewMode::Grid ? std::max(1, card_height_ + gap_) : std::max(1, list_item_height_));
}

void AdvancedFinderResultsView::setSelectionInternal(int index, bool notify) {
	if (empty_state_ != EmptyState::Rows || rows_.empty()) {
		selected_index_ = -1;
		return;
	}

	if (index < 0 || index >= static_cast<int>(rows_.size())) {
		index = -1;
	}

	if (selected_index_ == index) {
		if (notify) {
			sendSelectionEvent();
		}
		return;
	}

	selected_index_ = index;
	Refresh();

	if (notify) {
		sendSelectionEvent();
	}
}

void AdvancedFinderResultsView::selectDefaultRow(const AdvancedFinderSelectionKey& preferred_selection) {
	selected_index_ = -1;
	for (size_t index = 0; index < rows_.size(); ++index) {
		if (AdvancedFinderSelectionMatches(*rows_[index], preferred_selection)) {
			selected_index_ = static_cast<int>(index);
			break;
		}
	}

	if (selected_index_ < 0 && !rows_.empty()) {
		selected_index_ = 0;
	}
}

void AdvancedFinderResultsView::refreshNow() {
	Refresh();
}

void AdvancedFinderResultsView::sendSelectionEvent() {
	wxCommandEvent event(wxEVT_LISTBOX, GetId());
	event.SetEventObject(this);
	event.SetInt(selected_index_);
	ProcessWindowEvent(event);
}

void AdvancedFinderResultsView::sendActivateEvent() {
	wxCommandEvent event(wxEVT_LISTBOX_DCLICK, GetId());
	event.SetEventObject(this);
	event.SetInt(selected_index_);
	ProcessWindowEvent(event);
}

int AdvancedFinderResultsView::hitTest(int x, int y) const {
	if (empty_state_ != EmptyState::Rows || rows_.empty()) {
		return -1;
	}

	if (mode_ == AdvancedFinderResultViewMode::List) {
		const int header_height = std::max(1, listHeaderHeight());
		if (y < header_height) {
			return -1;
		}
		const int item_height = std::max(1, list_item_height_);
		const int index = (y - header_height) / item_height;
		if (index >= 0 && index < static_cast<int>(rows_.size())) {
			return index;
		}
		return -1;
	}

	const int row_height = std::max(1, card_height_ + gap_);
	const int row = (y - padding_) / row_height;
	const int row_offset = (y - padding_) % row_height;
	if (row < 0 || row_offset < 0 || row_offset >= card_height_) {
		return -1;
	}

	const int column_width = card_width_ + gap_;
	const int column = (x - padding_) / column_width;
	const int column_offset = (x - padding_) % column_width;
	if (column < 0 || column_offset < 0 || column_offset >= card_width_) {
		return -1;
	}

	const int index = row * columns_ + column;
	if (index >= 0 && index < static_cast<int>(rows_.size())) {
		return index;
	}
	return -1;
}

wxRect AdvancedFinderResultsView::itemRect(size_t index) const {
	if (mode_ == AdvancedFinderResultViewMode::List) {
		const int item_height = std::max(1, list_item_height_);
		return wxRect(0, std::max(1, listHeaderHeight()) + static_cast<int>(index) * item_height, GetClientSize().x, item_height);
	}

	const int row = static_cast<int>(index) / std::max(1, columns_);
	const int column = static_cast<int>(index) % std::max(1, columns_);
	const int row_height = std::max(1, card_height_ + gap_);
	const int x = padding_ + column * (card_width_ + gap_);
	const int y = padding_ + row * row_height;
	return wxRect(x, y, card_width_, card_height_);
}

Sprite* AdvancedFinderResultsView::spriteForRow(const AdvancedFinderCatalogRow& row) const {
	if (row.isCreature() && row.creature_brush != nullptr) {
		return row.creature_brush->getSprite();
	}

	if (row.client_id != 0) {
		return g_gui.gfx.getSprite(row.client_id);
	}

	return nullptr;
}

int AdvancedFinderResultsView::columnsForWidth(int width) const {
	const int usable_width = std::max(1, width - padding_ * 2);
	const int card_space = card_width_ + gap_;
	return std::max(1, (usable_width + gap_) / std::max(1, card_space));
}

int AdvancedFinderResultsView::visibleRows() const {
	const int row_height = (mode_ == AdvancedFinderResultViewMode::Grid) ? std::max(1, card_height_ + gap_) : std::max(1, list_item_height_);
	return std::max(1, GetClientSize().y / row_height);
}
