#include "util/virtual_item_grid.h"
#include "util/common.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace {
	constexpr int kMinIconSize = 16;
	constexpr int kMaxIconSize = 128;
	constexpr int kDefaultIconSize = 32;

	// Hover animation, shared with the palette grid.
	constexpr int kAnimTimerInterval = 16;
	constexpr float kAnimThreshold = 0.01f;
	constexpr float kAnimFactor = 0.2f;
} // namespace

VirtualItemGrid::VirtualItemGrid(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxVSCROLL | wxWANTS_CHARS),
	// The size setting is not shared with the palette: each pane keeps its own.
	m_iconSize(kDefaultIconSize),
	m_showLabels(false),
	m_sortKey(TilesetSortKey::ID),
	m_sortDir(TilesetSortDirection::Ascending),
	m_hasSort(false),
	m_columns(1),
	m_selectedIndex(-1),
	m_hoverIndex(-1),
	m_animTimer(this) {
	Bind(wxEVT_SIZE, &VirtualItemGrid::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &VirtualItemGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &VirtualItemGrid::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &VirtualItemGrid::OnLeave, this);
	Bind(wxEVT_TIMER, &VirtualItemGrid::OnTimer, this, m_animTimer.GetId());
}

VirtualItemGrid::~VirtualItemGrid() {
	m_animTimer.Stop();
}

wxSize VirtualItemGrid::CellSize() const {
	// Cells are exactly as wide as the icon box and only grow downwards for the
	// label band - the label width never widens a card.
	return wxSize(rme::ui::grid::CellWidth(m_iconSize), rme::ui::grid::CellHeight(m_iconSize, m_showLabels));
}

void VirtualItemGrid::SetShowLabels(bool show) {
	if (m_showLabels == show) {
		return;
	}
	m_showLabels = show;
	m_labelCache.Clear();
	RefreshGrid();
}

void VirtualItemGrid::SetIconSize(int sizePx) {
	const int clamped = std::clamp(sizePx, kMinIconSize, kMaxIconSize);
	if (m_iconSize == clamped) {
		return;
	}
	m_iconSize = clamped;
	m_labelCache.Clear();
	RefreshGrid();
}

void VirtualItemGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_sortKey = key;
	m_sortDir = dir;
	m_hasSort = true;
}

void VirtualItemGrid::ClearSort() {
	m_hasSort = false;
}

void VirtualItemGrid::ApplySortTo(std::vector<uint16_t>& items) const {
	if (!m_hasSort || items.size() < 2) {
		return;
	}

	const bool ascending = m_sortDir == TilesetSortDirection::Ascending;
	if (m_sortKey == TilesetSortKey::ID) {
		std::ranges::sort(items, [ascending](uint16_t a, uint16_t b) {
			return ascending ? a < b : a > b;
		});
		return;
	}

	// Name ordering: resolve the names once instead of on every comparison.
	std::vector<std::pair<std::string, uint16_t>> keyed;
	keyed.reserve(items.size());
	for (uint16_t id : items) {
		keyed.emplace_back(GetSortName(id), id);
	}

	std::ranges::sort(keyed, [ascending](const auto& lhs, const auto& rhs) {
		if (lhs.first != rhs.first) {
			return ascending ? lhs.first < rhs.first : lhs.first > rhs.first;
		}
		return lhs.second < rhs.second;
	});

	std::ranges::transform(keyed, items.begin(), [](const auto& entry) {
		return entry.second;
	});
}

std::string VirtualItemGrid::GetSortName(uint16_t id) const {
	const auto definition = g_item_definitions.get(id);
	return definition ? std::string(definition.name()) : std::string();
}

void VirtualItemGrid::RefreshGrid() {
	UpdateLayout();
	Refresh();
}

void VirtualItemGrid::UpdateLayout() {
	int width = GetClientSize().x;
	if (width <= 0) {
		width = 200;
	}

	const wxSize cell = CellSize();
	const int padding = rme::ui::grid::kPadding;
	m_columns = std::max(1, (width - padding) / (cell.x + padding));
	size_t count = GetItemCount();
	int rows = (count + m_columns - 1) / m_columns;
	int contentHeight = rows * (cell.y + padding) + padding;

	UpdateScrollbar(contentHeight);
}

wxSize VirtualItemGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(250, 400));
}

int VirtualItemGrid::HitTest(int x, int y) const {
	const wxSize cell = CellSize();
	const int padding = rme::ui::grid::kPadding;
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int col = (x - padding) / (cell.x + padding);
	int row = (realY - padding) / (cell.y + padding);

	if (col < 0 || col >= m_columns || row < 0) {
		return -1;
	}

	size_t index = row * m_columns + col;
	if (index < GetItemCount()) {
		wxRect r = GetItemRect(static_cast<int>(index));
		r.y -= scrollPos; // Convert back to local for test
		if (r.Contains(x, y)) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

wxRect VirtualItemGrid::GetItemRect(int index) const {
	const wxSize cell = CellSize();
	const int padding = rme::ui::grid::kPadding;
	int row = index / m_columns;
	int col = index % m_columns;
	return wxRect(
		padding + col * (cell.x + padding),
		padding + row * (cell.y + padding),
		cell.x,
		cell.y
	);
}

void VirtualItemGrid::SetSelection(int index) {
	if (index != m_selectedIndex) {
		m_selectedIndex = index;
		OnItemSelected(index);
		EnsureVisible(index);
		Refresh();
	}
}

uint16_t VirtualItemGrid::GetSelectedItemId() const {
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(GetItemCount())) {
		return GetItem(m_selectedIndex);
	}
	return 0;
}

void VirtualItemGrid::EnsureVisible(int index) {
	if (index < 0 || index >= static_cast<int>(GetItemCount())) {
		return;
	}

	const int padding = rme::ui::grid::kPadding;
	wxRect rect = GetItemRect(index);
	int scrollPos = GetScrollPosition();
	int clientH = GetClientSize().y;

	if (rect.y < scrollPos) {
		SetScrollPosition(rect.y - padding);
	} else if (rect.y + rect.height > scrollPos + clientH) {
		SetScrollPosition(rect.y + rect.height - clientH + padding);
	}
}

void VirtualItemGrid::OnSize(wxSizeEvent& event) {
	UpdateLayout();
	Refresh();
	event.Skip();
}

void VirtualItemGrid::OnMouseDown(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != -1) {
		SetSelection(index);
	}
	event.Skip();
}

void VirtualItemGrid::OnMotion(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != m_hoverIndex) {
		m_hoverIndex = index;

		if (m_hoverIndex != -1) {
			m_hoverAnim = 0.0f; // restart the grow for the new cell
			SetCursor(wxCursor(wxCURSOR_HAND));
			SetToolTip(GetItemTooltip(m_hoverIndex));
		} else {
			SetCursor(wxNullCursor);
			UnsetToolTip();
		}
		Refresh();
	}

	if (m_hoverIndex != -1 && !m_animTimer.IsRunning()) {
		m_animTimer.Start(kAnimTimerInterval);
	}
	event.Skip();
}

void VirtualItemGrid::OnLeave(wxMouseEvent& event) {
	if (m_hoverIndex != -1) {
		m_hoverIndex = -1;
		UnsetToolTip();
		Refresh();
	}
	event.Skip();
}

void VirtualItemGrid::OnTimer(wxTimerEvent&) {
	const float target = m_hoverIndex != -1 ? 1.0f : 0.0f;
	if (std::abs(m_hoverAnim - target) > kAnimThreshold) {
		m_hoverAnim += (target - m_hoverAnim) * kAnimFactor;
		Refresh();
		return;
	}

	m_hoverAnim = target;
	if (m_hoverIndex == -1) {
		m_animTimer.Stop();
	}
}

void VirtualItemGrid::ClearCache() {
	ClearImageCache();
}

int VirtualItemGrid::GetOrCreateItemTexture(NVGcontext* vg, uint16_t id) {
	return GetOrCreateItemImage(id);
}

wxString VirtualItemGrid::GetItemName(size_t index) const {
	if (index < GetItemCount()) {
		uint16_t id = GetItem(index);
		const auto definition = g_item_definitions.get(id);
		return definition ? wxstr(definition.name()) : wxString();
	}
	return "";
}

wxString VirtualItemGrid::GetItemTooltip(size_t index) const {
	return GetItemName(index);
}

void VirtualItemGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	const wxSize cell = CellSize();
	int rowHeight = cell.y + rme::ui::grid::kPadding;
	int scrollPos = GetScrollPosition();

	size_t count = GetItemCount();
	if (count == 0) {
		return;
	}

	// Calculate visible range
	int startRow = scrollPos / rowHeight;
	int endRow = (scrollPos + height + rowHeight - 1) / rowHeight;

	int startIdx = startRow * m_columns;
	int endIdx = std::min(static_cast<int>(count), (endRow + 1) * m_columns);

	for (int i = startIdx; i < endIdx; ++i) {
		if (i < 0) {
			continue;
		}

		const wxRect cell = GetItemRect(i);
		const bool isSelected = IsItemSelected(i);
		const bool isHovered = (i == m_hoverIndex);

		// Cell chrome, hover grow and state colours come from the shared
		// renderer so this grid and the palette grid stay identical.
		rme::ui::grid::DrawCardChrome(vg, cell, isSelected, isHovered, m_hoverAnim);

		const uint16_t id = GetItem(i);
		const auto definition = g_item_definitions.get(id);
		const uint16_t clientId = definition ? definition.clientId() : 0;
		const auto badge = rme::rendering::TechnicalItemRegistry::Classify(id, clientId);

		int texture = 0;
		if (badge == rme::rendering::TileIndicatorType::None) {
			texture = GetOrCreateItemTexture(vg, id);
		}
		rme::ui::grid::DrawCellIcon(vg, rme::ui::grid::IconRect(cell, m_iconSize), texture, badge);

		if (m_showLabels) {
			rme::ui::grid::DrawCellLabel(vg, cell, GetItemName(i), isSelected, m_labelCache);
		}
	}
}
