#include "app/settings.h"
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

	m_indicatorDogEar = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	const int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	m_gridSpacing = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 4;
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

	// Name ordering: resolve the lowercase names once instead of on every comparison.
	struct KeyedItem {
		std::string lowerName;
		uint16_t id = 0;
	};
	std::vector<KeyedItem> keyed;
	keyed.reserve(items.size());
	for (uint16_t id : items) {
		keyed.push_back({ as_lower_str(GetSortName(id)), id });
	}

	std::ranges::sort(keyed, [ascending](const auto& lhs, const auto& rhs) {
		if (lhs.lowerName != rhs.lowerName) {
			return ascending ? lhs.lowerName < rhs.lowerName : lhs.lowerName > rhs.lowerName;
		}
		return ascending ? lhs.id < rhs.id : lhs.id > rhs.id;
	});

	std::ranges::transform(keyed, items.begin(), [](const auto& entry) {
		return entry.id;
	});
}

std::string VirtualItemGrid::GetSortName(uint16_t id) const {
	const auto definition = g_item_definitions.get(id);
	return definition ? std::string(definition.name()) : std::string();
}

void VirtualItemGrid::SetListMode(bool list) {
	if (m_listMode == list) {
		return;
	}
	m_listMode = list;
	m_labelCache.Clear();
	RefreshGrid();
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

	const size_t count = GetItemCount();
	const int padding = m_gridSpacing;

	if (m_listMode) {
		m_columns = 1;
		const int rowHeight = rme::ui::grid::kListRowHeight;
		const int contentHeight = count > 0 ? (padding + static_cast<int>(count) * (rowHeight + padding)) : 0;
		UpdateScrollbar(contentHeight);
		return;
	}

	const wxSize cell = CellSize();
	m_columns = std::max(1, (width - padding) / (cell.x + padding));
	int rows = (count + m_columns - 1) / m_columns;
	int contentHeight = rows * (cell.y + padding) + padding;

	UpdateScrollbar(contentHeight);
}

wxSize VirtualItemGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(250, 400));
}

int VirtualItemGrid::HitTest(int x, int y) const {
	const int padding = m_gridSpacing;
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;

	if (m_listMode) {
		const int rowHeight = rme::ui::grid::kListRowHeight;
		const int rowStep = rowHeight + padding;
		if (realY < padding) {
			return -1;
		}
		const int row = (realY - padding) / rowStep;
		const int offset = (realY - padding) % rowStep;
		if (offset >= rowHeight || row < 0 || row >= static_cast<int>(GetItemCount())) {
			return -1;
		}
		return row;
	}

	const wxSize cell = CellSize();
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
	const int padding = m_gridSpacing;
	if (m_listMode) {
		const int rowHeight = rme::ui::grid::kListRowHeight;
		const int width = std::max(1, GetClientSize().x - 2 * padding);
		return wxRect(padding, padding + index * (rowHeight + padding), width, rowHeight);
	}

	const wxSize cell = CellSize();
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

int VirtualItemGrid::GetOrCreateItemTexture(NVGcontext* vg, uint16_t id, std::optional<uint16_t> subtype) {
	return GetOrCreateItemImage(id, subtype);
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
	int scrollPos = GetScrollPosition();

	size_t count = GetItemCount();
	if (count == 0) {
		return;
	}

	const int rowHeight = (m_listMode ? rme::ui::grid::kListRowHeight : CellSize().y) + m_gridSpacing;
	const int startRow = std::max(0, (scrollPos - m_gridSpacing) / rowHeight);
	const int endRow = (scrollPos + height - m_gridSpacing + rowHeight - 1) / rowHeight + 1;

	const int startIdx = m_listMode ? startRow : startRow * m_columns;
	const int endIdx = m_listMode
		? std::min(static_cast<int>(count), endRow + 1)
		: std::min(static_cast<int>(count), (endRow + 1) * m_columns);

	for (int i = startIdx; i < endIdx; ++i) {
		if (i < 0) {
			continue;
		}

		const wxRect cell = GetItemRect(i);
		const bool isSelected = IsItemSelected(i);
		const bool isHovered = (i == m_hoverIndex);

		const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, cell, isSelected, isHovered, m_hoverAnim, m_showLabels && !m_listMode);

		const uint16_t id = GetItem(i);
		const auto definition = g_item_definitions.get(id);
		const uint16_t clientId = definition ? definition.clientId() : 0;
		const auto badge = rme::rendering::TechnicalItemRegistry::Classify(id, clientId);

		int texture = 0;
		if (badge == rme::rendering::TileIndicatorType::None) {
			texture = GetOrCreateItemTexture(vg, id, GetItemSubtype(i));
		}

		const int iconSize = m_listMode ? rme::ui::grid::kIconSizeBase : m_iconSize;
		const wxRect iconRect = m_listMode
			? wxRect(drawn.x + rme::ui::grid::kIconOffset, drawn.y + rme::ui::grid::kIconOffset, iconSize, iconSize)
			: rme::ui::grid::IconRect(drawn, iconSize);
		rme::ui::grid::DrawCellIcon(vg, iconRect, texture, badge);

		if (m_indicatorDogEar) {
			const NVGcolor stdColor = rme::ui::grid::GetStandardCategoryColor(Theme::Role::BrushCategoryDefaultRaw);
			rme::ui::grid::DrawDogEarBadge(vg, iconRect, stdColor);
		}

		if (m_listMode) {
			rme::ui::grid::DrawListLabel(vg, drawn, GetItemName(i));
		} else if (m_showLabels) {
			rme::ui::grid::DrawCellLabel(vg, cell, GetItemName(i), isSelected, m_labelCache);
		}
	}
}

void VirtualItemGrid::SetBrushIndicators(bool dogEar) {
	if (m_indicatorDogEar != dogEar) {
		m_indicatorDogEar = dogEar;
		Refresh();
	}
}

void VirtualItemGrid::SetGridSpacing(int spacingPx) {
	const int validSpacing = (spacingPx == 2 || spacingPx == 6) ? spacingPx : 4;
	if (m_gridSpacing != validSpacing) {
		m_gridSpacing = validSpacing;
		UpdateLayout();
		Refresh();
	}
}
