//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/brush_items/brush_items_window.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/managers/brush_manager.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "rendering/indicators/technical_item_registry.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/common.h"
#include "util/grid_cell_renderer.h"

#include <wx/sizer.h>
#include <wx/stattext.h>

#include <unordered_set>
#include <algorithm>
#include <format>
#include <ranges>

// ============================================================================
// BrushItemsGrid
// ============================================================================

BrushItemsGrid::BrushItemsGrid(wxWindow* parent) :
	VirtualItemGrid(parent, wxID_ANY) {
	SetIconSize(32);
	SetShowLabels(g_settings.getBoolean(Config::BRUSH_ITEMS_SHOW_LABELS));
	SetBrushIndicators(g_settings.getBoolean(Config::BRUSH_ITEMS_INDICATOR_DOGEAR));
}

void BrushItemsGrid::SetItems(std::vector<uint16_t> items) {
	m_rawItems = std::move(items);
	ApplyFilterAndSort();
	if (m_selectedItemId != 0) {
		SetSelectedItem(m_selectedItemId);
	}
}

void BrushItemsGrid::SetFilter(const std::string& query) {
	m_filterQuery = query;
	ApplyFilterAndSort();
}

void BrushItemsGrid::SetSelectedItem(uint16_t id) {
	m_selectedItemId = id;
	auto it = std::ranges::find(m_displayItems, id);
	if (it != m_displayItems.end()) {
		SetSelection(static_cast<int>(std::distance(m_displayItems.begin(), it)));
	} else {
		SetSelection(-1);
	}
	Refresh();
}

bool BrushItemsGrid::ContainsItem(uint16_t id) const {
	return std::ranges::find(m_rawItems, id) != m_rawItems.end();
}

size_t BrushItemsGrid::GetItemCount() const {
	return m_displayItems.size();
}

uint16_t BrushItemsGrid::GetItem(size_t index) const {
	return (index < m_displayItems.size()) ? m_displayItems[index] : 0;
}

wxString BrushItemsGrid::GetItemName(size_t index) const {
	uint16_t id = GetItem(index);
	if (const auto def = g_item_definitions.get(id)) {
		const std::string_view name = def.name();
		if (!name.empty()) {
			return wxstr(name);
		}
	}
	return wxstr(std::format("Item {}", id));
}

wxString BrushItemsGrid::GetItemTooltip(size_t index) const {
	uint16_t id = GetItem(index);
	if (const auto def = g_item_definitions.get(id)) {
		const std::string_view name = def.name();
		return wxstr(std::format("{} (ID: {})", name.empty() ? "Item" : name, id));
	}
	return wxstr(std::format("Item {}", id));
}

bool BrushItemsGrid::IsItemSelected(int index) const {
	if (index >= 0 && index < static_cast<int>(m_displayItems.size())) {
		return m_displayItems[index] == m_selectedItemId;
	}
	return false;
}

void BrushItemsGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	VirtualItemGrid::SetSort(key, dir);
	ApplyFilterAndSort();
}

void BrushItemsGrid::ClearSort() {
	VirtualItemGrid::ClearSort();
	ApplyFilterAndSort();
}

void BrushItemsGrid::OnItemSelected(int index) {
	if (index >= 0 && index < static_cast<int>(m_displayItems.size())) {
		uint16_t id = m_displayItems[index];
		m_selectedItemId = id;
		SetSelection(index);
		if (RAWBrush* raw = g_materials.ensureRawBrush(id)) {
			// Use SelectBrushInternal so the main palette/tileset window is not navigated away or jumped
			g_gui.SelectBrushInternal(raw);
		}
	}
}

void BrushItemsGrid::ApplyFilterAndSort() {
	m_displayItems.clear();
	if (m_filterQuery.empty()) {
		m_displayItems = m_rawItems;
	} else {
		wxString q = wxstr(m_filterQuery).Lower();
		for (uint16_t id : m_rawItems) {
			if (std::to_string(id).find(m_filterQuery) != std::string::npos) {
				m_displayItems.push_back(id);
				continue;
			}
			if (const auto def = g_item_definitions.get(id)) {
				if (wxstr(def.name()).Lower().Find(q) != wxNOT_FOUND) {
					m_displayItems.push_back(id);
				}
			}
		}
	}

	if (HasSort()) {
		ApplySortTo(m_displayItems);
	}

	RefreshGrid();
	Refresh();
}

// ============================================================================
// BrushBottomCardWidget
// ============================================================================

BrushBottomCardWidget::BrushBottomCardWidget(wxWindow* parent) :
	NanoVGCanvas(parent, wxID_ANY, wxBORDER_NONE) {
	SetMinSize(FromDIP(wxSize(220, 42)));
	Bind(wxEVT_LEFT_DOWN, &BrushBottomCardWidget::OnMouseDown, this);
	Bind(wxEVT_ENTER_WINDOW, &BrushBottomCardWidget::OnMouseEnter, this);
	Bind(wxEVT_LEAVE_WINDOW, &BrushBottomCardWidget::OnMouseLeave, this);
}

void BrushBottomCardWidget::SetBrush(const Brush* brush) {
	m_brush = brush;
	if (m_brush) {
		SetToolTip(wxstr(std::format("Click to select {}", m_brush->getName())));
	} else {
		UnsetToolTip();
	}
	Refresh();
}

void BrushBottomCardWidget::SetIndicatorDogEar(bool dogEar) {
	m_indicatorDogEar = dogEar;
	Refresh();
}

wxSize BrushBottomCardWidget::DoGetBestClientSize() const {
	return FromDIP(wxSize(200, 42));
}

void BrushBottomCardWidget::OnMouseDown(wxMouseEvent&) {
	if (m_brush) {
		// Use SelectBrushInternal so the main palette/tileset window is not jumped or filtered
		g_gui.SelectBrushInternal(const_cast<Brush*>(m_brush));
		if (auto* win = dynamic_cast<BrushItemsWindow*>(GetParent())) {
			win->OnBottomCardClicked();
		}
		Refresh();
	}
}

void BrushBottomCardWidget::OnMouseEnter(wxMouseEvent&) {
	m_hovered = true;
	SetCursor(wxCursor(wxCURSOR_HAND));
	Refresh();
}

void BrushBottomCardWidget::OnMouseLeave(wxMouseEvent&) {
	m_hovered = false;
	SetCursor(wxCursor(wxCURSOR_ARROW));
	Refresh();
}

void BrushBottomCardWidget::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	// Surface background
	const wxColour bgCol = Theme::Get(Theme::Role::Surface);
	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	nvgFillColor(vg, nvgRGBA(bgCol.Red(), bgCol.Green(), bgCol.Blue(), 255));
	nvgFill(vg);

	// Top separator line
	const wxColour borderCol = Theme::Get(Theme::Role::Border);
	nvgBeginPath(vg);
	nvgMoveTo(vg, 0.0f, 0.5f);
	nvgLineTo(vg, static_cast<float>(width), 0.5f);
	nvgStrokeColor(vg, nvgRGBA(borderCol.Red(), borderCol.Green(), borderCol.Blue(), 255));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	if (!m_brush) {
		return;
	}

	const bool isSelected = (g_gui.GetCurrentBrush() == m_brush);
	const int cardSize = 32;
	const int cardX = 6;
	const int cardY = (height - cardSize) / 2;
	const wxRect cardRect(cardX, cardY, cardSize, cardSize);

	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, cardRect, isSelected, m_hovered, 0.0f, false);
	const wxRect iconRect = rme::ui::grid::IconRect(drawn, 28);

	int texture = 0;
	const auto badge = rme::rendering::TechnicalItemRegistry::GetBrushIndicatorType(m_brush);
	if (badge == rme::rendering::TileIndicatorType::None) {
		if (Sprite* sprite = m_brush->getSprite()) {
			texture = GetOrCreateSpriteTexture(vg, sprite);
		} else {
			const int lookId = m_brush->getLookID();
			if (lookId < 0) {
				texture = GetOrCreateEditorIconTexture(vg, lookId);
			} else if (lookId > 0) {
				if (Sprite* lookSprite = g_gui.gfx.getSprite(lookId)) {
					texture = GetOrCreateSpriteTexture(vg, lookSprite);
				}
			}
		}
	}
	rme::ui::grid::DrawCellIcon(vg, iconRect, texture, badge);

	if (m_indicatorDogEar) {
		const NVGcolor catNVG = rme::ui::grid::GetStandardCategoryColor(m_brush);
		rme::ui::grid::DrawDogEarBadge(vg, iconRect, catNVG);
	}

	// Draw brush name label
	const float textX = static_cast<float>(cardX + cardSize + 8);
	const float textY = height / 2.0f;
	const float maxTextWidth = static_cast<float>(width) - textX - 6.0f;
	if (maxTextWidth > 0.0f) {
		nvgSave(vg);
		nvgScissor(vg, textX, 0.0f, maxTextWidth, static_cast<float>(height));
		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		const wxColour textCol = isSelected ? Theme::Get(Theme::Role::Accent) : Theme::Get(Theme::Role::Text);
		nvgFillColor(vg, nvgRGBA(textCol.Red(), textCol.Green(), textCol.Blue(), 255));
		nvgText(vg, textX, textY, m_brush->getName().c_str(), nullptr);
		nvgRestore(vg);
	}
}

// ============================================================================
// BrushItemsWindow
// ============================================================================

BrushItemsWindow::BrushItemsWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	BuildUI();
}

void BrushItemsWindow::BuildUI() {
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Grid
	m_grid = new BrushItemsGrid(this);

	// Toolbar without search box
	TilesetGridToolbar::Actions actions;
	actions.filter = [this](const std::string& text) {
		if (m_grid) {
			m_grid->SetFilter(text);
		}
	};
	actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		if (m_grid) {
			m_grid->SetSort(key, dir);
		}
	};
	actions.clear_sort = [this]() {
		if (m_grid) {
			m_grid->ClearSort();
		}
	};
	actions.show_labels = [this](bool show) {
		if (m_grid) {
			m_grid->SetShowLabels(show);
		}
		g_settings.setInteger(Config::BRUSH_ITEMS_SHOW_LABELS, show ? 1 : 0);
	};
	actions.icon_size = [this](int size) {
		if (m_grid) {
			m_grid->SetIconSize(size);
		}
	};
	actions.brush_indicators = [this](bool dogEar) {
		if (m_grid) {
			m_grid->SetBrushIndicators(dogEar);
		}
		if (m_bottomCard) {
			m_bottomCard->SetIndicatorDogEar(dogEar);
		}
		if (m_toolbar) {
			m_toolbar->SetIndicatorState(dogEar);
		}
		g_settings.setInteger(Config::BRUSH_ITEMS_INDICATOR_DOGEAR, dogEar ? 1 : 0);
	};
	actions.grid_spacing = [this](int spacingPx) {
		if (m_grid) {
			m_grid->SetGridSpacing(spacingPx);
		}
		if (m_toolbar) {
			m_toolbar->SetGridSpacing(spacingPx);
		}
	};

	const unsigned parts = TilesetGridToolbar::Part_Sort |
	                       TilesetGridToolbar::Part_Labels |
	                       TilesetGridToolbar::Part_Size |
	                       TilesetGridToolbar::Part_Indicators |
	                       TilesetGridToolbar::Part_Spacing;
	const bool initShowLabels = g_settings.getBoolean(Config::BRUSH_ITEMS_SHOW_LABELS);
	const bool initDogEar = g_settings.getBoolean(Config::BRUSH_ITEMS_INDICATOR_DOGEAR);
	m_grid->SetShowLabels(initShowLabels);
	m_grid->SetBrushIndicators(initDogEar);

	m_toolbar = new TilesetGridToolbar(this, std::move(actions), parts);
	m_toolbar->SetState(initShowLabels, m_grid->GetIconSize(), m_grid->HasSort(), m_grid->GetSortKey(), m_grid->GetSortDirection(), false);
	m_toolbar->SetIndicatorState(initDogEar);
	mainSizer->Add(m_toolbar, wxSizerFlags().Expand());

	// Empty state banner
	m_emptyLabel = new wxStaticText(this, wxID_ANY, "Select a brush to view its items.", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
	m_emptyLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	mainSizer->Add(m_emptyLabel, wxSizerFlags().Expand().Border(wxTOP | wxBOTTOM, 24));

	mainSizer->Add(m_grid, wxSizerFlags(1).Expand());

	// Bottom tile square with brush name
	m_bottomCard = new BrushBottomCardWidget(this);
	if (m_grid) {
		m_bottomCard->SetIndicatorDogEar(m_grid->GetBrushIndicators());
	}
	m_bottomCard->Show(false);
	mainSizer->Add(m_bottomCard, wxSizerFlags().Expand());

	SetSizer(mainSizer);
}

void BrushItemsWindow::SetActiveBrush(const Brush* brush) {
	if (!brush) {
		m_currentSourceBrush = nullptr;
		m_lastSourceBrushName.clear();
		if (m_grid) {
			m_grid->SetItems({});
		}
		if (m_bottomCard) {
			m_bottomCard->SetBrush(nullptr);
			m_bottomCard->Show(false);
		}
		if (m_emptyLabel) {
			m_emptyLabel->SetLabelText("Select a brush to view its items.");
			m_emptyLabel->Show(true);
		}
		if (m_grid) m_grid->Show(false);
		Layout();
		return;
	}

	// If a RAWBrush was chosen, and it belongs to our currently active source brush,
	// keep the grid intact and only highlight the active item!
	if (const RAWBrush* raw = brush->as<RAWBrush>()) {
		const uint16_t raw_id = raw->getItemID();
		if (m_currentSourceBrush != nullptr && m_grid && m_grid->ContainsItem(raw_id)) {
			m_grid->SetSelectedItem(raw_id);
			if (m_bottomCard) {
				m_bottomCard->Refresh();
			}
			return;
		}
	}

	// If the brush is identical to current source brush, do not rebuild
	if (brush == m_currentSourceBrush && brush->getName() == m_lastSourceBrushName) {
		if (m_grid) {
			m_grid->SetSelectedItem(0);
		}
		if (m_bottomCard) {
			m_bottomCard->Refresh();
		}
		return;
	}

	m_currentSourceBrush = brush;
	m_lastSourceBrushName = brush->getName();

	std::vector<uint16_t> items;
	const_cast<Brush*>(brush)->getRelatedItems(items);

	// Deduplicate preserving insertion order
	std::vector<uint16_t> unique_items;
	std::unordered_set<uint16_t> seen;
	unique_items.reserve(items.size());
	for (uint16_t id : items) {
		if (id != 0 && seen.insert(id).second) {
			unique_items.push_back(id);
		}
	}

	const bool isComposite = !brush->is<RAWBrush>();
	const size_t count = unique_items.size();

	if (m_grid) {
		m_grid->SetItems(std::move(unique_items));
		if (const RAWBrush* raw = brush->as<RAWBrush>()) {
			m_grid->SetSelectedItem(raw->getItemID());
		} else {
			m_grid->SetSelectedItem(0);
		}
	}

	if (m_bottomCard) {
		m_bottomCard->SetBrush(isComposite ? brush : nullptr);
		m_bottomCard->Show(isComposite);
	}

	if (m_emptyLabel) {
		m_emptyLabel->SetLabelText("No items associated with this brush.");
		m_emptyLabel->Show(count == 0);
	}
	if (m_grid) {
		m_grid->Show(count > 0);
	}

	Layout();
}

void BrushItemsWindow::RefreshItems() {
	SetActiveBrush(m_currentSourceBrush);
}

void BrushItemsWindow::OnBottomCardClicked() {
	if (m_grid) {
		m_grid->SetSelectedItem(0);
	}
	if (m_bottomCard) {
		m_bottomCard->Refresh();
	}
}
