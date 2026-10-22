#include "ui/controls/collection_brush_grid.h"

#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/creature/creature_brush.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/gui.h"
#include "util/common.h"

#include <algorithm>
#include <cmath>

namespace rme::ui {

namespace {
	constexpr int kAnimInterval = 20;
	constexpr float kAnimThreshold = 0.01f;
	constexpr float kAnimFactor = 0.3f;

	uint32_t GetBrushSortID(const Brush* brush) {
		if (!brush) {
			return 0;
		}
		if (const auto* raw = dynamic_cast<const RAWBrush*>(brush)) {
			return raw->getItemID();
		}
		if (const auto* cb = dynamic_cast<const CreatureBrush*>(brush)) {
			if (cb->getType()) {
				if (cb->getType()->outfit.lookType != 0) {
					return static_cast<uint32_t>(cb->getType()->outfit.lookType);
				}
				if (cb->getType()->outfit.lookItem != 0) {
					return static_cast<uint32_t>(cb->getType()->outfit.lookItem);
				}
			}
		}
		if (brush->getLookID() != 0) {
			return static_cast<uint32_t>(brush->getLookID());
		}
		return brush->getID();
	}
}

CollectionBrushGrid::CollectionBrushGrid(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id) {
	m_itemSize = rme::ui::grid::CellWidth(m_iconSize);

	Bind(wxEVT_LEFT_DOWN, &CollectionBrushGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &CollectionBrushGrid::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &CollectionBrushGrid::OnLeave, this);
	Bind(wxEVT_SIZE, &CollectionBrushGrid::OnSize, this);
	Bind(wxEVT_TIMER, &CollectionBrushGrid::OnTimer, this, m_animTimer.GetId());
}

CollectionBrushGrid::~CollectionBrushGrid() {
	if (m_animTimer.IsRunning()) {
		m_animTimer.Stop();
	}
}

void CollectionBrushGrid::SetBrushes(const std::vector<Brush*>& brushes) {
	m_allBrushes = brushes;
	ApplyFilterAndSort();
}

void CollectionBrushGrid::SetFilter(const std::string& query) {
	m_filterQuery = query;
	ApplyFilterAndSort();
	SetScrollPosition(0);
}

void CollectionBrushGrid::SetIconSize(int sizePx) {
	sizePx = std::clamp(sizePx, 32, 128);
	if (m_iconSize != sizePx) {
		m_iconSize = sizePx;
		m_itemSize = rme::ui::grid::CellWidth(m_iconSize);
		m_labelCache.Clear();
		UpdateLayout();
		Refresh();
	}
}

void CollectionBrushGrid::SetShowLabels(bool show) {
	if (m_showLabels != show) {
		m_showLabels = show;
		m_labelCache.Clear();
		UpdateLayout();
		Refresh();
	}
}

void CollectionBrushGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_hasSort = true;
	m_sortKey = key;
	m_sortDir = dir;
	ApplyFilterAndSort();
}

void CollectionBrushGrid::ClearSort() {
	m_hasSort = false;
	ApplyFilterAndSort();
}

Brush* CollectionBrushGrid::GetSelectedBrush() const {
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_filteredBrushes.size())) {
		return m_filteredBrushes[m_selectedIndex];
	}
	return nullptr;
}

void CollectionBrushGrid::ApplyFilterAndSort() {
	if (m_filterQuery.empty()) {
		m_filteredBrushes = m_allBrushes;
	} else {
		m_filteredBrushes.clear();
		std::string lowerQuery = as_lower_str(m_filterQuery);
		for (Brush* b : m_allBrushes) {
			if (!b) {
				continue;
			}
			std::string name = as_lower_str(b->getName());
			if (name.find(lowerQuery) != std::string::npos) {
				m_filteredBrushes.push_back(b);
			}
		}
	}

	if (m_hasSort) {
		const bool ascending = (m_sortDir == TilesetSortDirection::Ascending);
		std::stable_sort(m_filteredBrushes.begin(), m_filteredBrushes.end(), [this, ascending](Brush* a, Brush* b) {
			if (!a && !b) return false;
			if (!a) return false;
			if (!b) return true;

			if (m_sortKey == TilesetSortKey::ID) {
				uint32_t idA = GetBrushSortID(a);
				uint32_t idB = GetBrushSortID(b);
				if (idA != idB) {
					return ascending ? (idA < idB) : (idA > idB);
				}
				std::string na = as_lower_str(a->getName());
				std::string nb = as_lower_str(b->getName());
				if (na != nb) {
					return ascending ? (na < nb) : (na > nb);
				}
			} else {
				std::string na = as_lower_str(a->getName());
				std::string nb = as_lower_str(b->getName());
				if (na != nb) {
					return ascending ? (na < nb) : (na > nb);
				}
				uint32_t idA = GetBrushSortID(a);
				uint32_t idB = GetBrushSortID(b);
				if (idA != idB) {
					return ascending ? (idA < idB) : (idA > idB);
				}
			}
			return false;
		});
	}

	UpdateLayout();
	Refresh();
}

void CollectionBrushGrid::UpdateLayout() {
	int width = GetClientSize().x;
	if (width <= 0) {
		width = 200;
	}

	constexpr int padding = rme::ui::grid::kPadding;
	int totalItems = static_cast<int>(m_filteredBrushes.size());
	int cellWidth = m_itemSize;
	int cellHeight = rme::ui::grid::CellHeight(m_iconSize, m_showLabels);
	m_columns = std::max(1, (width - padding) / (cellWidth + padding));
	int rows = (totalItems + m_columns - 1) / m_columns;
	int contentHeight = rows * (cellHeight + padding) + padding;
	UpdateScrollbar(contentHeight);
}

wxSize CollectionBrushGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(250, 400));
}

int CollectionBrushGrid::HitTest(int x, int y) const {
	constexpr int padding = rme::ui::grid::kPadding;
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int realX = x;
	int totalItems = static_cast<int>(m_filteredBrushes.size());

	if (realX < padding || realY < padding) {
		return -1;
	}

	int cellWidth = m_itemSize;
	int cellHeight = rme::ui::grid::CellHeight(m_iconSize, m_showLabels);

	int col = (realX - padding) / (cellWidth + padding);
	int row = (realY - padding) / (cellHeight + padding);

	if (col < 0 || col >= m_columns || row < 0) {
		return -1;
	}

	int index = row * m_columns + col;
	if (index >= 0 && index < totalItems) {
		wxRect rect = GetItemRect(index);
		rect.y -= scrollPos;
		if (rect.Contains(x, y)) {
			return index;
		}
	}
	return -1;
}

wxRect CollectionBrushGrid::GetItemRect(int index) const {
	constexpr int padding = rme::ui::grid::kPadding;
	int row = index / m_columns;
	int col = index % m_columns;
	int cellWidth = m_itemSize;
	int cellHeight = rme::ui::grid::CellHeight(m_iconSize, m_showLabels);

	return wxRect(
		padding + col * (cellWidth + padding),
		padding + row * (cellHeight + padding),
		cellWidth,
		cellHeight
	);
}

void CollectionBrushGrid::DrawBrushItem(NVGcontext* vg, int i, const wxRect& rect) {
	Brush* brush = (i < static_cast<int>(m_filteredBrushes.size())) ? m_filteredBrushes[i] : nullptr;
	if (!brush) {
		return;
	}

	const bool selected = (i == m_selectedIndex);
	const bool hovered = (i == m_hoverIndex);

	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, m_hoverAnim);

	const wxRect iconRect = rme::ui::grid::IconRect(drawn, m_iconSize);

	int texture = 0;
	const auto badge = rme::rendering::TechnicalItemRegistry::GetBrushIndicatorType(brush);
	if (badge == rme::rendering::TileIndicatorType::None) {
		if (Sprite* sprite = brush->getSprite()) {
			texture = GetOrCreateSpriteTexture(vg, sprite);
		} else {
			const int lookId = brush->getLookID();
			if (lookId < 0) {
				texture = GetOrCreateEditorIconTexture(vg, lookId);
			} else if (lookId > 0) {
				if (Sprite* sprite = g_gui.gfx.getSprite(lookId)) {
					texture = GetOrCreateSpriteTexture(vg, sprite);
				}
			}
		}
	}
	rme::ui::grid::DrawCellIcon(vg, iconRect, texture, badge);

	if (m_showLabels) {
		rme::ui::grid::DrawCellLabel(vg, drawn, wxstr(brush->getName()), selected, m_labelCache);
	}

	if (hovered) {
		rme::ui::grid::DrawRemoveBadge(vg, rect, m_badgeHovered);
	}
}

void CollectionBrushGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	int scrollPos = GetScrollPosition();
	int cellHeight = rme::ui::grid::CellHeight(m_iconSize, m_showLabels);
	int rowHeight = cellHeight + rme::ui::grid::kPadding;
	int startRow = scrollPos / rowHeight;
	int endRow = (scrollPos + height + rowHeight - 1) / rowHeight + 1;

	int startIdx = startRow * m_columns;
	int endIdx = std::min(static_cast<int>(m_filteredBrushes.size()), endRow * m_columns);

	for (int i = startIdx; i < endIdx; ++i) {
		DrawBrushItem(vg, i, GetItemRect(i));
	}
}

void CollectionBrushGrid::OnMouseDown(wxMouseEvent& event) {
	int idx = HitTest(event.GetX(), event.GetY());
	if (idx != -1 && idx < static_cast<int>(m_filteredBrushes.size())) {
		wxRect cell = GetItemRect(idx);
		cell.y -= GetScrollPosition();
		wxRect badgeRect = rme::ui::grid::GetBadgeRect(cell);
		if (badgeRect.Contains(event.GetPosition())) {
			Brush* b = m_filteredBrushes[idx];
			if (m_discardCallback) {
				m_discardCallback(b);
			}
			return;
		}
		m_selectedIndex = idx;
		Refresh();
	}
	event.Skip();
}

void CollectionBrushGrid::OnMotion(wxMouseEvent& event) {
	int idx = HitTest(event.GetX(), event.GetY());
	bool badgeHovered = false;
	if (idx != -1 && idx < static_cast<int>(m_filteredBrushes.size())) {
		wxRect cell = GetItemRect(idx);
		cell.y -= GetScrollPosition();
		wxRect badgeRect = rme::ui::grid::GetBadgeRect(cell);
		badgeHovered = badgeRect.Contains(event.GetPosition());
	}

	if (badgeHovered != m_badgeHovered) {
		m_badgeHovered = badgeHovered;
		Refresh();
	}

	if (idx != m_hoverIndex) {
		m_hoverIndex = idx;
		if (m_hoverIndex != -1) {
			m_hoverAnim = 0.0f;
			SetCursor(wxCursor(wxCURSOR_HAND));
			if (static_cast<size_t>(m_hoverIndex) < m_filteredBrushes.size()) {
				SetToolTip(wxstr(m_filteredBrushes[m_hoverIndex]->getName()));
			}
		} else {
			SetCursor(wxNullCursor);
			UnsetToolTip();
		}
		Refresh();
	}

	if (m_hoverIndex != -1 && !m_animTimer.IsRunning()) {
		m_animTimer.Start(kAnimInterval);
	}
	event.Skip();
}

void CollectionBrushGrid::OnLeave(wxMouseEvent& event) {
	m_badgeHovered = false;
	if (m_hoverIndex != -1) {
		m_hoverIndex = -1;
		UnsetToolTip();
		Refresh();
	}
	event.Skip();
}

void CollectionBrushGrid::OnSize(wxSizeEvent& event) {
	UpdateLayout();
	Refresh();
	event.Skip();
}

void CollectionBrushGrid::OnTimer(wxTimerEvent&) {
	float target = (m_hoverIndex != -1) ? 1.0f : 0.0f;
	if (std::abs(m_hoverAnim - target) > kAnimThreshold) {
		m_hoverAnim += (target - m_hoverAnim) * kAnimFactor;
		Refresh();
	} else {
		m_hoverAnim = target;
		if (m_hoverIndex == -1) {
			m_animTimer.Stop();
		}
	}
}

} // namespace rme::ui
