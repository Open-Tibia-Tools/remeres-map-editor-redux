#include "app/settings.h"
#include "app/main.h"
#include "brushes/brush.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/palette_window.h"
#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "rendering/core/graphics.h"

#include <nanovg.h>

#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/find_item_window_model.h"

#include <wx/settings.h>

#include <algorithm>
#include <cstdlib>
#include <unordered_set>
#include <utility>

wxDEFINE_EVENT(EVT_VIRTUAL_BRUSH_GRID_PICKED, wxCommandEvent);
wxDEFINE_EVENT(EVT_VIRTUAL_BRUSH_GRID_BEGIN_DRAG, wxCommandEvent);

namespace {
	static constexpr int TIMER_INTERVAL = 16;
	static constexpr float INTER_THRESHOLD = 0.01f;
	static constexpr float INTER_FACTOR = 0.2f;

	BrushGridSelection::Modifier ModifierOf(const wxMouseEvent& event) {
		if (event.ShiftDown()) {
			return BrushGridSelection::Modifier::Range;
		}
		if (event.CmdDown()) {
			return BrushGridSelection::Modifier::Toggle;
		}
		return BrushGridSelection::Modifier::None;
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

	m_indicatorDogEar = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	const int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	m_gridSpacing = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 4;

	item_size = rme::ui::grid::CellWidth(icon_size_px);

	Bind(wxEVT_LEFT_DOWN, &VirtualBrushGrid::OnMouseDown, this);
	Bind(wxEVT_LEFT_UP, &VirtualBrushGrid::OnMouseUp, this);
	Bind(wxEVT_RIGHT_DOWN, &VirtualBrushGrid::OnRightDown, this);
	Bind(wxEVT_MOTION, &VirtualBrushGrid::OnMotion, this);
	Bind(wxEVT_SIZE, &VirtualBrushGrid::OnSize, this);
	Bind(wxEVT_SHOW, &VirtualBrushGrid::OnShow, this);
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

	m_selection.Remap(m_display_brushes);
	SyncSelectedIndex();
}

void VirtualBrushGrid::SyncSelectedIndex() {
	selected_index = -1;
	if (const Brush* primary = m_selection.Primary()) {
		const auto it = std::ranges::find(m_display_brushes, primary);
		if (it != m_display_brushes.end()) {
			selected_index = static_cast<int>(it - m_display_brushes.begin());
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

	const int padding = m_gridSpacing;
	int totalItems = static_cast<int>(m_display_brushes.size());
	if (display_mode == DisplayMode::List) {
		columns = 1;
		int rowHeight = rme::ui::grid::kListRowHeight;
		int contentHeight = totalItems > 0 ? (padding + totalItems * (rowHeight + padding)) : 0;
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
	if (m_pendingScrollToSelected) {
		m_pendingScrollToSelected = false;
		// First paint after the page became visible: finish the deferred scroll
		// outside of painting and draw again.
		CallAfter([this]() {
			UpdateLayout();
			EnsureSelectedVisible(); // repaints itself if it scrolls
		});
	}

	if (!m_hasOverrideBrushes && observed_tileset_size != (tileset ? tileset->size() : 0)) {
		observed_tileset_size = tileset ? tileset->size() : 0;
		RefreshBrushList();
		UpdateLayout();
	}

	// Calculate visible range
	int scrollPos = GetScrollPosition();
	int rowStep = (display_mode == DisplayMode::List)
		? (rme::ui::grid::kListRowHeight + m_gridSpacing)
		: (rme::ui::grid::CellHeight(icon_size_px, m_showLabels) + m_gridSpacing);
	int startRow = std::max(0, (scrollPos - m_gridSpacing) / rowStep);
	int endRow = (scrollPos + height - m_gridSpacing + rowStep - 1) / rowStep + 1;

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

	const bool selected = m_selection.Contains(brush);
	const bool hovered = (i == hover_index);
	const bool listMode = display_mode == DisplayMode::List;

	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, hover_anim, m_showLabels && !listMode);

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

	if (m_indicatorDogEar) {
		const bool isLightTheme = (Theme::Get(Theme::Role::Surface).Red() > 128);
		const NVGcolor catNVG = rme::ui::grid::GetStandardCategoryColor(brush);
		rme::ui::grid::DrawDogEarBadge(vg, iconRect, catNVG);
	}

	if (listMode) {
		rme::ui::grid::DrawListLabel(vg, drawn, wxstr(brush->getName()));
	} else if (m_showLabels) {
		rme::ui::grid::DrawCellLabel(vg, drawn, wxstr(brush->getName()), selected, m_labelCache);
	}
}

wxRect VirtualBrushGrid::GetItemRect(int index) const {
	const int padding = m_gridSpacing;
	if (display_mode == DisplayMode::List) {
		int width = std::max(1, GetClientSize().x - 2 * padding);
		int rowHeight = rme::ui::grid::kListRowHeight;
		return wxRect(padding, padding + index * (rowHeight + padding), width, rowHeight);
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
	const int padding = m_gridSpacing;
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int realX = x;
	int totalItems = static_cast<int>(m_display_brushes.size());

	if (display_mode == DisplayMode::List) {
		if (realY < padding) {
			return -1;
		}
		int rowHeight = rme::ui::grid::kListRowHeight;
		int rowStep = rowHeight + padding;
		int row = (realY - padding) / rowStep;
		int offsetInRow = (realY - padding) % rowStep;

		if (offsetInRow >= rowHeight || row < 0 || row >= totalItems) {
			return -1;
		}

		if (realX >= padding && realX <= GetClientSize().x - padding) {
			return row;
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
	const int index = HitTest(event.GetX(), event.GetY());
	// Search results only become current on release (they jump to their own page then).
	const auto commit = m_hasOverrideBrushes ? BrushGridSelection::Commit::OnRelease : BrushGridSelection::Commit::OnPress;
	const BrushGridSelection::PressResult result = m_selection.Press(m_display_brushes, index, ModifierOf(event), commit);
	SyncSelectedIndex();

	m_dragArmed = result.can_drag;
	m_dragStarted = false;
	m_dragOrigin = event.GetPosition();
	m_pressIndex = index;
	m_pendingSearchPick = nullptr;

	if (Brush* picked = const_cast<Brush*>(result.picked)) {
		if (m_hasOverrideBrushes) {
			// Jumping rebuilds this list, so it waits for the release (a drag may start first).
			m_pendingSearchPick = picked;
		} else {
			CommitPick(picked);
		}
	}
	// Not skipped: on MSW the default handling would take the focus back from the map canvas.
	Refresh();
}

void VirtualBrushGrid::OnMouseUp(wxMouseEvent& event) {
	const bool dragged = m_dragStarted;
	m_dragArmed = false;
	m_dragStarted = false;

	if (Brush* collapsed = const_cast<Brush*>(m_selection.Release(dragged))) {
		SyncSelectedIndex();
		if (m_hasOverrideBrushes) {
			m_pendingSearchPick = collapsed;
		} else {
			CommitPick(collapsed);
		}
	}

	// A search result jumps only when the button is released on the cell it was pressed on.
	const bool released_on_pressed = m_pressIndex != -1 && HitTest(event.GetX(), event.GetY()) == m_pressIndex;
	m_pressIndex = -1;
	Brush* const search_pick = std::exchange(m_pendingSearchPick, nullptr);
	if (search_pick && !dragged && released_on_pressed) {
		CommitSearchPick(search_pick);
		return; // the jump may have rebuilt or hidden this grid
	}
	Refresh();
	event.Skip();
}

void VirtualBrushGrid::OnRightDown(wxMouseEvent& event) {
	m_selection.PressContext(m_display_brushes, HitTest(event.GetX(), event.GetY()));
	Refresh();
	// wxGTK only sends wxEVT_CONTEXT_MENU when the right button press is not handled.
	event.Skip();
}

void VirtualBrushGrid::SendPicked(Brush* brush) {
	wxCommandEvent picked(EVT_VIRTUAL_BRUSH_GRID_PICKED, GetId());
	picked.SetEventObject(this);
	picked.SetClientData(brush);
	ProcessWindowEvent(picked);
}

void VirtualBrushGrid::CommitSelectedBrush() {
	if (Brush* brush = GetSelectedBrush()) {
		CommitPick(brush);
	}
}

void VirtualBrushGrid::CommitPick(Brush* brush) {
	// Compared with the application's brush, not this grid's selection: another page or
	// window may have changed the brush since this grid last picked. A click on the current
	// brush still counts as a user pick (Recent), but only focuses the map.
	if (brush == g_gui.GetCurrentBrush()) {
		SendPicked(brush);
		g_gui.FocusCurrentMapCanvas();
		return;
	}

	for (wxWindow* w = GetParent(); w; w = w->GetParent()) {
		if (PaletteWindow* pw = dynamic_cast<PaletteWindow*>(w)) {
			g_gui.ActivatePalette(pw);
			break;
		}
	}
	SendPicked(brush);
	g_gui.SelectBrushInternal(brush);
}

void VirtualBrushGrid::CommitSearchPick(Brush* brush) {
	PaletteWindow* pw = nullptr;
	BrushPalettePanel* currentBrushPalettePanel = nullptr;
	for (wxWindow* w = GetParent(); w && !(pw && currentBrushPalettePanel); w = w->GetParent()) {
		if (!currentBrushPalettePanel) {
			currentBrushPalettePanel = dynamic_cast<BrushPalettePanel*>(w);
		}
		if (!pw) {
			pw = dynamic_cast<PaletteWindow*>(w);
		}
	}

	m_selection.Reset(brush);
	SyncSelectedIndex();
	SendPicked(brush);
	if (pw) {
		const std::string preferredPal = currentBrushPalettePanel ? currentBrushPalettePanel->GetName().ToStdString() : "";
		if (pw->JumpToBrush(brush, preferredPal)) {
			return;
		}
		g_gui.ActivatePalette(pw);
	}
	// No page holds the brush: select it here, like a normal click.
	g_gui.SelectBrushInternal(brush);
	Refresh();
}

void VirtualBrushGrid::OnMotion(wxMouseEvent& event) {
	if (m_dragArmed && !m_dragStarted && event.LeftIsDown()) {
		const wxPoint delta = event.GetPosition() - m_dragOrigin;
		const int threshold_x = std::max(1, wxSystemSettings::GetMetric(wxSYS_DRAG_X, this));
		const int threshold_y = std::max(1, wxSystemSettings::GetMetric(wxSYS_DRAG_Y, this));
		if (std::abs(delta.x) >= threshold_x || std::abs(delta.y) >= threshold_y) {
			m_dragStarted = true;
			m_pendingSearchPick = nullptr;
			wxCommandEvent begin_drag(EVT_VIRTUAL_BRUSH_GRID_BEGIN_DRAG, GetId());
			begin_drag.SetEventObject(this);
			ProcessWindowEvent(begin_drag);
			// The drop source ran its own loop and consumed the button release.
			m_dragArmed = false;
			m_dragStarted = false;
			m_selection.Release(true);
			Refresh();
			return;
		}
	}

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
	if (m_pendingScrollToSelected) {
		EnsureSelectedVisible();
	}
	Refresh();
	event.Skip();
}

void VirtualBrushGrid::OnShow(wxShowEvent& event) {
	if (event.IsShown() && m_pendingScrollToSelected) {
		// Sizers lay the page out after it is shown; scroll once that is done.
		CallAfter([this]() {
			UpdateLayout();
			EnsureSelectedVisible();
		});
	}
	event.Skip();
}

void VirtualBrushGrid::EnsureSelectedVisible() {
	if (selected_index < 0 || selected_index >= static_cast<int>(m_display_brushes.size())) {
		m_pendingScrollToSelected = false;
		return;
	}

	const int clientHeight = GetClientSize().y;
	if (!IsShownOnScreen() || clientHeight <= 0) {
		m_pendingScrollToSelected = true;
		return;
	}
	m_pendingScrollToSelected = false;

	const wxRect rect = GetItemRect(selected_index);
	const int scrollPos = GetScrollPosition();
	if (rect.y < scrollPos) {
		SetScrollPosition(rect.y - rme::ui::grid::kPadding);
	} else if (rect.y + rect.height > scrollPos + clientHeight) {
		SetScrollPosition(rect.y + rect.height - clientHeight + rme::ui::grid::kPadding);
	}
}

void VirtualBrushGrid::SelectFirstBrush() {
	if (!m_display_brushes.empty()) {
		m_selection.Reset(m_display_brushes.front());
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
			m_selection.Reset(m_display_brushes[i]);
			selected_index = static_cast<int>(i);

			// The palette page holding this grid may be hidden right now (it is
			// switched to after the brush is found), so its layout can be stale.
			UpdateLayout();
			EnsureSelectedVisible();
			if (!m_pendingScrollToSelected) {
				// Re-check once the page switch and its layout have settled.
				m_pendingScrollToSelected = true;
				CallAfter([this]() {
					UpdateLayout();
					EnsureSelectedVisible();
				});
			}

			Refresh();
			return true;
		}
	}
	m_selection.Reset(nullptr);
	selected_index = -1;
	Refresh();
	return false;
}

std::vector<Brush*> VirtualBrushGrid::GetSelectedBrushes() const {
	return m_selection.Selected(m_display_brushes);
}

void VirtualBrushGrid::ClearSelection() {
	m_selection.Reset(nullptr);
	selected_index = -1;
	m_pendingSearchPick = nullptr;
	Refresh();
}

Brush* VirtualBrushGrid::BrushAt(int x, int y) const {
	const int index = HitTest(x, y);
	return index >= 0 ? m_display_brushes[static_cast<size_t>(index)] : nullptr;
}

wxPoint VirtualBrushGrid::GetSelectedCellPosition() const {
	if (selected_index < 0) {
		return wxPoint(0, 0);
	}
	const wxRect rect = GetItemRect(selected_index);
	return wxPoint(rect.x + rect.width / 2, rect.y + rect.height / 2 - GetScrollPosition());
}

void VirtualBrushGrid::ReloadBrushes() {
	const int scroll = GetScrollPosition();
	observed_tileset_size = tileset ? tileset->size() : 0;
	RefreshBrushList();
	UpdateLayout();
	SetScrollPosition(scroll);
	Refresh();
}


void VirtualBrushGrid::SetBrushIndicators(bool dogEar) {
	if (m_indicatorDogEar != dogEar) {
		m_indicatorDogEar = dogEar;
		Refresh();
	}
}

void VirtualBrushGrid::SetGridSpacing(int spacingPx) {
	const int validSpacing = (spacingPx == 2 || spacingPx == 6) ? spacingPx : 4;
	if (m_gridSpacing != validSpacing) {
		m_gridSpacing = validSpacing;
		UpdateLayout();
		Refresh();
	}
}
