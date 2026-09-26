#include "util/virtual_item_grid.h"
#include "util/nvg_utils.h"
#include "util/common.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/indicators/technical_item_registry.h"
#include "ui/theme.h"
#include <algorithm>
#include <format>
#include <cmath>

VirtualItemGrid::VirtualItemGrid(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxVSCROLL | wxWANTS_CHARS),
	m_itemSize(80), // Larger for text area
	m_padding(6),
	m_columns(1),
	m_selectedIndex(-1),
	m_hoverIndex(-1),
	m_animTimer(this) {
	Bind(wxEVT_SIZE, &VirtualItemGrid::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &VirtualItemGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &VirtualItemGrid::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &VirtualItemGrid::OnLeave, this);

	// Optional timer for smooth effects if needed
	// Bind(wxEVT_TIMER, &VirtualItemGrid::OnTimer, this);
}

VirtualItemGrid::~VirtualItemGrid() {
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

	m_columns = std::max(1, (width - m_padding) / (m_itemSize + m_padding));
	size_t count = GetItemCount();
	int rows = (count + m_columns - 1) / m_columns;
	int contentHeight = rows * (m_itemSize + m_padding) + m_padding;

	UpdateScrollbar(contentHeight);
}

wxSize VirtualItemGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(250, 400));
}

int VirtualItemGrid::HitTest(int x, int y) const {
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int col = (x - m_padding) / (m_itemSize + m_padding);
	int row = (realY - m_padding) / (m_itemSize + m_padding);

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
	int row = index / m_columns;
	int col = index % m_columns;
	return wxRect(
		m_padding + col * (m_itemSize + m_padding),
		m_padding + row * (m_itemSize + m_padding),
		m_itemSize,
		m_itemSize
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

	wxRect rect = GetItemRect(index);
	int scrollPos = GetScrollPosition();
	int clientH = GetClientSize().y;

	if (rect.y < scrollPos) {
		SetScrollPosition(rect.y - m_padding);
	} else if (rect.y + rect.height > scrollPos + clientH) {
		SetScrollPosition(rect.y + rect.height - clientH + m_padding);
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
			SetCursor(wxCursor(wxCURSOR_HAND));
			SetToolTip(GetItemName(m_hoverIndex));
		} else {
			SetCursor(wxNullCursor);
			UnsetToolTip();
		}
		Refresh();
	}
	event.Skip();
}

void VirtualItemGrid::OnLeave(wxMouseEvent& event) {
	if (m_hoverIndex != -1) {
		m_hoverIndex = -1;
		Refresh();
	}
	event.Skip();
}

void VirtualItemGrid::OnTimer(wxTimerEvent&) {
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

void VirtualItemGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	int rowHeight = m_itemSize + m_padding;
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

		wxRect r = GetItemRect(i);
		float x = static_cast<float>(r.x);
		float y = static_cast<float>(r.y);
		float w = static_cast<float>(r.width);
		float h = static_cast<float>(r.height);

		bool isSelected = (i == m_selectedIndex);
		bool isHovered = (i == m_hoverIndex);

		// Shadow / Glow
		if (isSelected) {
			NVGpaint shadowPaint = nvgBoxGradient(vg, x, y, w, h, 4.0f, 10.0f, nvgRGBA(100, 150, 255, 128), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 10, y - 10, w + 20, h + 20);
			nvgRoundedRect(vg, x, y, w, h, 4.0f);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, shadowPaint);
			nvgFill(vg);
		} else if (isHovered) {
			NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2, w, h, 4.0f, 6.0f, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 5, y - 5, w + 10, h + 10);
			nvgRoundedRect(vg, x, y, w, h, 4.0f);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, shadowPaint);
			nvgFill(vg);
		}

		// Card Background
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, 4.0f);
		if (isSelected) {
			wxColour selCol = Theme::Get(Theme::Role::Accent);
			nvgFillColor(vg, nvgRGBA(selCol.Red(), selCol.Green(), selCol.Blue(), 200));
		} else if (isHovered) {
			wxColour hoverCol = Theme::Get(Theme::Role::CardBaseHover);
			nvgFillColor(vg, nvgRGBA(hoverCol.Red(), hoverCol.Green(), hoverCol.Blue(), 255));
		} else {
			wxColour baseCol = Theme::Get(Theme::Role::CardBase);
			nvgFillColor(vg, nvgRGBA(baseCol.Red(), baseCol.Green(), baseCol.Blue(), 255));
		}
		nvgFill(vg);

		// Selection Border
		if (isSelected) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4.0f);
			wxColour accentCol = Theme::Get(Theme::Role::Accent);
			nvgStrokeColor(vg, nvgRGBA(accentCol.Red(), accentCol.Green(), accentCol.Blue(), 255));
			nvgStrokeWidth(vg, 2.0f);
			nvgStroke(vg);
		}

		// Draw Item Icon (Fixed 32x32 size for consistency)
		uint16_t id = GetItem(i);
		const auto tech = rme::rendering::TechnicalItemRegistry::Classify(id, 0);
		if (tech != rme::rendering::TileIndicatorType::None) {
			const float iconSize = 32.0f;
			const float bx = x + (w - iconSize) / 2.0f;
			const float by = y + 8.0f;
			const auto style = rme::rendering::GetIndicatorBadgeStyle(tech);

			// 1. Background wash
			nvgBeginPath(vg);
			nvgRoundedRect(vg, bx, by, iconSize, iconSize, 3.0f);
			nvgFillColor(vg, nvgRGBA(style.bg_r, style.bg_g, style.bg_b, style.bg_a));
			nvgFill(vg);

			// 2. Vibrant 1px border
			nvgBeginPath(vg);
			nvgRoundedRect(vg, bx + 0.5f, by + 0.5f, iconSize - 1.0f, iconSize - 1.0f, 3.0f);
			nvgStrokeColor(vg, nvgRGBA(style.border_r, style.border_g, style.border_b, 255));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);

			// 3. Crisp centered typography
			nvgFontSize(vg, 9.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

			const float cx = bx + iconSize * 0.5f;
			const float cy = by + iconSize * 0.5f;

			nvgFillColor(vg, nvgRGBA(style.outline_r, style.outline_g, style.outline_b, 240));
			nvgText(vg, cx + 1.0f, cy, style.text, nullptr);
			nvgText(vg, cx - 1.0f, cy, style.text, nullptr);
			nvgText(vg, cx, cy + 1.0f, style.text, nullptr);
			nvgText(vg, cx, cy - 1.0f, style.text, nullptr);

			nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
			nvgText(vg, cx, cy, style.text, nullptr);
		} else {
			int tex = GetOrCreateItemTexture(vg, id);
			if (tex > 0) {
				// Fixed 32x32 rendering area
				float iconSize = 32.0f;
				int tw, th;
				nvgImageSize(vg, tex, &tw, &th);

				float scale = iconSize / std::max(tw, th);
				if (scale > 1.0f && std::max(tw, th) >= 32) {
					scale = 1.0f;
				}

				float dw = tw * scale;
				float dh = th * scale;
				float dx = x + (w - dw) / 2.0f;
				float dy = y + 8.0f; // Top padding

				NVGpaint imgPaint = nvgImagePattern(vg, dx, dy, dw, dh, 0.0f, tex, 1.0f);
				nvgBeginPath(vg);
				nvgRect(vg, dx, dy, dw, dh);
				nvgFillPaint(vg, imgPaint);
				nvgFill(vg);
			}
		}

		// Draw Text (2 Lines)
		wxString name = GetItemName(i);
		if (!name.IsEmpty()) {
			nvgFontSize(vg, 12.0f); // Larger font
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			if (isSelected) {
				wxColour textOnAccent = Theme::Get(Theme::Role::TextOnAccent);
				nvgFillColor(vg, nvgRGBA(textOnAccent.Red(), textOnAccent.Green(), textOnAccent.Blue(), 255));
			} else {
				wxColour textCol = Theme::Get(Theme::Role::TextSubtle);
				nvgFillColor(vg, nvgRGBA(textCol.Red(), textCol.Green(), textCol.Blue(), 230));
			}

			float textY = y + 44.0f; // Below the 32px icon area + padding
			float maxWidth = w - 4.0f;

			// NvgTextBox approach for wrapping
			nvgTextBox(vg, x + 2, textY, maxWidth, name.mb_str(), nullptr);
		}
	}
}
