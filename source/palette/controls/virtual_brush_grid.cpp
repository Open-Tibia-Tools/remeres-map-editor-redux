#include "app/main.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/palette_window.h"
#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "rendering/core/graphics.h"

#include <nanovg.h>

#include "brushes/raw/raw_brush.h"
#include "brushes/creature/creature_brush.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "game/creatures.h"
#include "ui/find_item_window_model.h"

#include <algorithm>
#include <unordered_set>

namespace {
	static constexpr int TIMER_INTERVAL = 16;
	static constexpr float INTER_THRESHOLD = 0.01f;
	static constexpr float INTER_FACTOR = 0.2f;

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

VirtualBrushGrid::VirtualBrushGrid(wxWindow* parent, const DynamicTilesetDefinition* _tileset, int iconSizePx) :
	NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
	BrushBoxInterface(_tileset),
	icon_size_px(std::clamp(iconSizePx, 32, 128)),
	selected_index(-1),
	hover_index(-1),
	columns(1),
	item_size(0),
	observed_tileset_size(_tileset ? _tileset->size() : 0),
	m_display_brushes(),
	m_animTimer(this) {

	item_size = rme::ui::grid::CellWidth(icon_size_px);

	Bind(wxEVT_LEFT_DOWN, &VirtualBrushGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &VirtualBrushGrid::OnMotion, this);
	Bind(wxEVT_SIZE, &VirtualBrushGrid::OnSize, this);
	Bind(wxEVT_TIMER, &VirtualBrushGrid::OnTimer, this);

	RefreshBrushList();
	UpdateLayout();
}

VirtualBrushGrid::~VirtualBrushGrid() = default;

void VirtualBrushGrid::SetDisplayMode(DisplayMode mode) {
	if (display_mode != mode) {
		display_mode = mode;
		m_labelCache.Clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::RefreshBrushList() {
	m_labelCache.Clear();
	Brush* selectedBrush = GetSelectedBrush();
	m_display_brushes.clear();

	static const std::vector<Brush*> s_emptyBrushes;
	const std::vector<Brush*>& sourceBrushes = m_hasOverrideBrushes ? m_overrideBrushes : (tileset ? tileset->brushes : s_emptyBrushes);

	std::vector<Brush*> uniqueSource;
	uniqueSource.reserve(sourceBrushes.size());
	std::unordered_set<const Brush*> seen;
	for (Brush* b : sourceBrushes) {
		if (b && seen.insert(b).second) {
			uniqueSource.push_back(b);
		}
	}

	if (!m_filterQuery.empty()) {
		m_display_brushes = FilterBrushesWithAdvancedFinder(uniqueSource, m_filterQuery);
		if (m_hasSort) {
			ApplySort();
		}
	} else {
		m_display_brushes = std::move(uniqueSource);
		if (m_hasSort) {
			ApplySort();
		}
	}

	selected_index = -1;
	if (selectedBrush) {
		for (size_t i = 0; i < m_display_brushes.size(); ++i) {
			if (m_display_brushes[i] == selectedBrush) {
				selected_index = static_cast<int>(i);
				break;
			}
		}
	}
}

void VirtualBrushGrid::ApplySort() {
	if (!m_hasSort) {
		return;
	}

	auto compare = [this](const Brush* a, const Brush* b) {
		if (!a && !b) return false;
		if (!a) return false;
		if (!b) return true;

		if (m_sortKey == TilesetSortKey::ID) {
			uint32_t idA = GetBrushSortID(a);
			uint32_t idB = GetBrushSortID(b);
			if (idA != idB) {
				return m_sortDir == TilesetSortDirection::Ascending ? (idA < idB) : (idA > idB);
			}
			int cmp = wxStricmp(wxstr(a->getName()), wxstr(b->getName()));
			if (cmp != 0) {
				return m_sortDir == TilesetSortDirection::Ascending ? (cmp < 0) : (cmp > 0);
			}
		} else {
			std::string nameA = a->getName();
			std::string nameB = b->getName();
			int cmp = wxStricmp(wxstr(nameA), wxstr(nameB));
			if (cmp != 0) {
				return m_sortDir == TilesetSortDirection::Ascending ? (cmp < 0) : (cmp > 0);
			}
			uint32_t idA = GetBrushSortID(a);
			uint32_t idB = GetBrushSortID(b);
			if (idA != idB) {
				return m_sortDir == TilesetSortDirection::Ascending ? (idA < idB) : (idA > idB);
			}
		}
		return false;
	};

	std::stable_sort(m_display_brushes.begin(), m_display_brushes.end(), compare);
}

void VirtualBrushGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_sortKey = key;
	m_sortDir = dir;
	m_hasSort = true;
	RefreshBrushList();
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::ClearSort() {
	m_hasSort = false;
	RefreshBrushList();
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::SetShowLabels(bool show) {
	if (m_showLabels != show) {
		m_showLabels = show;
		m_labelCache.Clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::SetTileSize(int sizePx) {
	sizePx = std::clamp(sizePx, 32, 128);
	if (icon_size_px != sizePx) {
		icon_size_px = sizePx;
		item_size = rme::ui::grid::CellWidth(icon_size_px);
		m_labelCache.Clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::SetFilterQuery(const std::string& query, const std::vector<Brush*>* overrideSource) {
	bool filterChanged = (m_filterQuery != query) || (m_hasOverrideBrushes != (overrideSource != nullptr));
	m_filterQuery = query;
	if (overrideSource) {
		m_overrideBrushes = *overrideSource;
		m_hasOverrideBrushes = true;
	} else {
		m_overrideBrushes.clear();
		m_hasOverrideBrushes = false;
	}
	RefreshBrushList();
	if (filterChanged) {
		SetScrollPosition(0);
	}
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::UpdateLayout() {
	int width = GetClientSize().x;
	if (width <= 0) {
		width = 200; // Default
	}

	constexpr int padding = rme::ui::grid::kPadding;
	int totalItems = static_cast<int>(m_display_brushes.size());
	if (display_mode == DisplayMode::List) {
		columns = 1;
		int contentHeight = totalItems * rme::ui::grid::kListRowHeight + padding;
		UpdateScrollbar(contentHeight);
	} else {
		int cellWidth = item_size;
		int cellHeight = rme::ui::grid::CellHeight(icon_size_px, m_showLabels);
		columns = std::max(1, (width - padding) / (cellWidth + padding));
		int rows = (totalItems + columns - 1) / columns;
		int contentHeight = rows * (cellHeight + padding) + padding;
		UpdateScrollbar(contentHeight);
	}
}

wxSize VirtualBrushGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(200, 300));
}

void VirtualBrushGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	if (!m_hasOverrideBrushes && observed_tileset_size != (tileset ? tileset->size() : 0)) {
		observed_tileset_size = tileset ? tileset->size() : 0;
		RefreshBrushList();
		UpdateLayout();
	}

	// Calculate visible range
	int scrollPos = GetScrollPosition();
	int rowHeight = (display_mode == DisplayMode::List) ? rme::ui::grid::kListRowHeight : (rme::ui::grid::CellHeight(icon_size_px, m_showLabels) + rme::ui::grid::kPadding);
	int startRow = scrollPos / rowHeight;
	int endRow = (scrollPos + height + rowHeight - 1) / rowHeight + 1;

	int startIdx = startRow * columns;
	int endIdx = std::min(static_cast<int>(m_display_brushes.size()), endRow * columns);

	// Draw visible items
	for (int i = startIdx; i < endIdx; ++i) {
		DrawBrushItem(vg, i, GetItemRect(i));
	}
}

void VirtualBrushGrid::DrawBrushItem(NVGcontext* vg, int i, const wxRect& rect) {
	Brush* brush = (i < static_cast<int>(m_display_brushes.size())) ? m_display_brushes[i] : nullptr;
	if (!brush) {
		return;
	}

	const bool selected = (i == selected_index);
	const bool hovered = (i == hover_index);
	const bool listMode = display_mode == DisplayMode::List;

	// Card chrome, hover grow and state colours come from the shared renderer
	// so this grid and the replace tool's item grid stay identical.
	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, hover_anim);

	const int iconSize = listMode ? rme::ui::grid::kIconSizeBase : icon_size_px;
	const wxRect iconRect = listMode
		? wxRect(drawn.x + rme::ui::grid::kIconOffset, drawn.y + rme::ui::grid::kIconOffset, iconSize, iconSize)
		: rme::ui::grid::IconRect(drawn, iconSize);

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

	if (listMode) {
		rme::ui::grid::DrawListLabel(vg, drawn, wxstr(brush->getName()));
	} else if (m_showLabels) {
		rme::ui::grid::DrawCellLabel(vg, drawn, wxstr(brush->getName()), selected, m_labelCache);
	}
}

wxRect VirtualBrushGrid::GetItemRect(int index) const {
	constexpr int padding = rme::ui::grid::kPadding;
	if (display_mode == DisplayMode::List) {
		int width = GetClientSize().x - 2 * padding;
		return wxRect(padding, padding + index * rme::ui::grid::kListRowHeight, width, rme::ui::grid::kListRowHeight);
	} else {
		int row = index / columns;
		int col = index % columns;
		int cellWidth = item_size;
		int cellHeight = rme::ui::grid::CellHeight(icon_size_px, m_showLabels);

		return wxRect(
			padding + col * (cellWidth + padding),
			padding + row * (cellHeight + padding),
			cellWidth,
			cellHeight
		);
	}
}

int VirtualBrushGrid::HitTest(int x, int y) const {
	constexpr int padding = rme::ui::grid::kPadding;
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int realX = x;
	int totalItems = static_cast<int>(m_display_brushes.size());

	if (display_mode == DisplayMode::List) {
		int row = (realY - padding) / rme::ui::grid::kListRowHeight;

		if (row < 0 || row >= totalItems) {
			return -1;
		}

		int index = row;
		// Check horizontal bounds properly
		if (realX >= padding && realX <= GetClientSize().x - padding) {
			return index;
		}
		return -1;
	} else {
		if (realX < padding || realY < padding) {
			return -1;
		}

		int cellWidth = item_size;
		int cellHeight = rme::ui::grid::CellHeight(icon_size_px, m_showLabels);

		int col = (realX - padding) / (cellWidth + padding);
		int row = (realY - padding) / (cellHeight + padding);

		if (col < 0 || col >= columns || row < 0) {
			return -1;
		}

		int index = row * columns + col;
		if (index >= 0 && index < totalItems) {
			wxRect rect = GetItemRect(index);
			// Adjust rect to scroll position for contains check
			rect.y -= scrollPos;
			if (rect.Contains(x, y)) {
				return index;
			}
		}
		return -1;
	}
}

void VirtualBrushGrid::OnMouseDown(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != -1) {
		if (m_hasOverrideBrushes && index >= 0 && static_cast<size_t>(index) < m_display_brushes.size()) {
			Brush* clickedBrush = m_display_brushes[index];
			if (clickedBrush) {
				PaletteWindow* pw = nullptr;
				BrushPalettePanel* currentBrushPalettePanel = nullptr;
				wxWindow* w = GetParent();
				while (w) {
					if (!currentBrushPalettePanel) {
						currentBrushPalettePanel = dynamic_cast<BrushPalettePanel*>(w);
					}
					if (!pw) {
						pw = dynamic_cast<PaletteWindow*>(w);
					}
					if (pw && currentBrushPalettePanel) {
						break;
					}
					w = w->GetParent();
				}
				if (pw) {
					std::string preferredPal = currentBrushPalettePanel ? currentBrushPalettePanel->GetName().ToStdString() : "";
					if (pw->JumpToBrush(clickedBrush, preferredPal)) {
						return;
					}
					// JumpToBrush may have rebuilt m_display_brushes before failing.
					if (static_cast<size_t>(index) >= m_display_brushes.size()) {
						return;
					}
				}
			}
		}

		if (index != selected_index) {
			selected_index = index;

			// Notify GUI - find PaletteWindow parent
			wxWindow* w = GetParent();
			while (w) {
				PaletteWindow* pw = dynamic_cast<PaletteWindow*>(w);
				if (pw) {
					g_gui.ActivatePalette(pw);
					break;
				}
				w = w->GetParent();
			}

			g_gui.SelectBrushInternal(m_display_brushes[selected_index]);
			Refresh();
		}
	}
}

void VirtualBrushGrid::OnMotion(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());

	if (index != hover_index) {
		hover_index = index;
		if (index != -1) {
			hover_anim = 0.0f; // Reset animation for new target
		}
		if (!m_animTimer.IsRunning()) {
			m_animTimer.Start(TIMER_INTERVAL);
		}
		Refresh();
	} else if (hover_index != -1 && !m_animTimer.IsRunning()) {
		m_animTimer.Start(TIMER_INTERVAL);
	}

	// Tooltip
	if (index >= 0 && static_cast<size_t>(index) < m_display_brushes.size()) {
		Brush* brush = m_display_brushes[index];
		if (brush) {
			wxString tip = wxstr(brush->getName());
			if (GetToolTipText() != tip) {
				SetToolTip(tip);
			}
		}
	} else {
		UnsetToolTip();
	}

	event.Skip();
}

void VirtualBrushGrid::OnTimer(wxTimerEvent& event) {
	float target = (hover_index != -1) ? 1.0f : 0.0f;
	if (std::abs(hover_anim - target) > INTER_THRESHOLD) {
		hover_anim += (target - hover_anim) * INTER_FACTOR;
		Refresh();
	} else {
		hover_anim = target;
		if (hover_index == -1) {
			m_animTimer.Stop();
		}
	}
}

void VirtualBrushGrid::OnSize(wxSizeEvent& event) {
	UpdateLayout();
	Refresh();
	event.Skip();
}

void VirtualBrushGrid::SelectFirstBrush() {
	if (!m_display_brushes.empty()) {
		selected_index = 0;
		Refresh();
	}
}

Brush* VirtualBrushGrid::GetSelectedBrush() const {
	if (selected_index >= 0 && selected_index < static_cast<int>(m_display_brushes.size())) {
		return m_display_brushes[selected_index];
	}
	return nullptr;
}

bool VirtualBrushGrid::SelectBrush(const Brush* brush) {
	for (size_t i = 0; i < m_display_brushes.size(); ++i) {
		if (m_display_brushes[i] == brush) {
			selected_index = static_cast<int>(i);

			// Ensure visible
			wxRect rect = GetItemRect(selected_index);
			int scrollPos = GetScrollPosition();
			int clientHeight = GetClientSize().y;

			if (rect.y < scrollPos) {
				SetScrollPosition(rect.y - rme::ui::grid::kPadding);
			} else if (rect.y + rect.height > scrollPos + clientHeight) {
				SetScrollPosition(rect.y + rect.height - clientHeight + rme::ui::grid::kPadding);
			}

			Refresh();
			return true;
		}
	}
	selected_index = -1;
	Refresh();
	return false;
}
