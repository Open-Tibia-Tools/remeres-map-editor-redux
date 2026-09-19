//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <nanovg.h>

#include "ui/tile_properties/container_grid_canvas.h"
#include "ui/tile_properties/container_property_panel.h"
#include "game/complexitem.h"
#include "game/sprites.h"
#include "ui/gui.h"
#include "ui/theme.h"

ContainerGridCanvas::ContainerGridCanvas(wxWindow* parent, bool large) :
	NanoVGCanvas(parent, wxID_ANY, 0),
	m_container(nullptr),
	m_large(large),
	m_hover_index(-1),
	m_selected_index(-1),
	m_cols(0),
	m_rows(0),
	m_slot_size(large ? 38.0f : 22.0f),
	m_padding(1.0f) {

	Bind(wxEVT_MOTION, &ContainerGridCanvas::OnMotion, this);
	Bind(wxEVT_LEFT_DOWN, &ContainerGridCanvas::OnLeftDown, this);
	Bind(wxEVT_RIGHT_DOWN, &ContainerGridCanvas::OnRightDown, this);
	Bind(wxEVT_LEAVE_WINDOW, &ContainerGridCanvas::OnMouseLeave, this);

	RecalculateLayout();
}

ContainerGridCanvas::~ContainerGridCanvas() {
}

void ContainerGridCanvas::SetContainer(Item* container) {
	m_container = container;
	RecalculateLayout();
	Refresh();
}

void ContainerGridCanvas::SetLargeSprites(bool large) {
	if (m_large != large) {
		m_large = large;
		m_slot_size = (m_large ? 38.0f : 22.0f);
		RecalculateLayout();
		Refresh();
	}
}

void ContainerGridCanvas::SetSelectedIndex(int index) {
	if (m_selected_index != index) {
		m_selected_index = index;
		Refresh();
	}
}

void ContainerGridCanvas::RecalculateLayout() {
	if (!m_container || !m_container->asContainer()) {
		m_cols = 1;
		m_rows = 1;
	} else {
		Container* container = m_container->asContainer();
		int capacity = static_cast<int>(container->getVolume());

		m_cols = m_large ? 6 : 12;

		if (capacity > 0) {
			m_rows = (capacity + m_cols - 1) / m_cols;
		} else {
			m_rows = 1;
		}
	}

	SetMinSize(DoGetBestClientSize());
	GetParent()->Layout();
}

wxSize ContainerGridCanvas::DoGetBestClientSize() const {
	int width = static_cast<int>(std::ceil(m_cols * m_slot_size));
	int height = static_cast<int>(std::ceil(m_rows * m_slot_size));
	return FromDIP(wxSize(width, height));
}

int ContainerGridCanvas::HitTest(int x, int y) const {
	if (!m_container || !m_container->asContainer()) {
		return -1;
	}

	int capacity = static_cast<int>(m_container->asContainer()->getVolume());
	if (capacity == 0) {
		return -1;
	}

	float dip_x = x / GetDPIScaleFactor();
	float dip_y = y / GetDPIScaleFactor();

	int col = static_cast<int>(std::floor(dip_x / m_slot_size));
	int row = static_cast<int>(std::floor(dip_y / m_slot_size));

	if (col >= 0 && col < m_cols && row >= 0 && row < m_rows) {
		int index = row * m_cols + col;
		if (index < capacity) {
			return index;
		}
	}

	return -1;
}

void ContainerGridCanvas::OnMotion(wxMouseEvent& event) {
	int hit = HitTest(event.GetX(), event.GetY());
	if (hit != m_hover_index) {
		m_hover_index = hit;

		if (hit != -1 && m_container && m_container->asContainer()) {
			Container* container = m_container->asContainer();
			if (hit < static_cast<int>(container->getItemCount())) {
				Item* item = container->getItem(hit);
				if (item) {
					SetToolTip(wxstr(item->getName()));
				} else {
					SetToolTip("Empty Slot");
				}
			} else {
				SetToolTip("Empty Slot");
			}
		} else {
			SetToolTip("");
		}

		Refresh();
	}
}

void ContainerGridCanvas::OnMouseLeave(wxMouseEvent& WXUNUSED(event)) {
	if (m_hover_index != -1) {
		m_hover_index = -1;
		Refresh();
	}
}

void ContainerGridCanvas::OnLeftDown(wxMouseEvent& event) {
	int hit = HitTest(event.GetX(), event.GetY());
	if (hit != -1) {
		m_selected_index = hit;
		Refresh();

		wxCommandEvent clickEvent(wxEVT_BUTTON, GetId());
		clickEvent.SetEventObject(this);
		clickEvent.SetInt(hit);
		ProcessWindowEvent(clickEvent);
	}
}

void ContainerGridCanvas::OnRightDown(wxMouseEvent& event) {
	int hit = HitTest(event.GetX(), event.GetY());
	if (hit != -1) {
		m_selected_index = hit;
		Refresh();

		wxContextMenuEvent ctxEvent(wxEVT_CONTEXT_MENU, GetId(), ClientToScreen(event.GetPosition()));
		ctxEvent.SetEventObject(this);
		ProcessWindowEvent(ctxEvent);
	}
}

void ContainerGridCanvas::DrawSunkenBorder(NVGcontext* vg, float x, float y, float size_x, float size_y) {
	NVGcolor dark_highlight = nvgRGBA(212, 208, 200, 255);
	NVGcolor light_shadow = nvgRGBA(128, 128, 128, 255);
	NVGcolor highlight = nvgRGBA(255, 255, 255, 255);
	NVGcolor shadow = nvgRGBA(64, 64, 64, 255);

	nvgStrokeWidth(vg, 1.0f);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + 0.5f, y + size_y - 0.5f);
	nvgLineTo(vg, x + 0.5f, y + 0.5f);
	nvgLineTo(vg, x + size_x - 0.5f, y + 0.5f);
	nvgStrokeColor(vg, shadow);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + 1.5f, y + size_y - 1.5f);
	nvgLineTo(vg, x + 1.5f, y + 1.5f);
	nvgLineTo(vg, x + size_x - 1.5f, y + 1.5f);
	nvgStrokeColor(vg, light_shadow);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + size_x - 1.5f, y + 1.5f);
	nvgLineTo(vg, x + size_x - 1.5f, y + size_y - 1.5f);
	nvgLineTo(vg, x + 1.5f, y + size_y - 1.5f);
	nvgStrokeColor(vg, dark_highlight);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + size_x - 0.5f, y + 0.5f);
	nvgLineTo(vg, x + size_x - 0.5f, y + size_y - 0.5f);
	nvgLineTo(vg, x + 0.5f, y + size_y - 0.5f);
	nvgStrokeColor(vg, highlight);
	nvgStroke(vg);
}

void ContainerGridCanvas::DrawRaisedBorder(NVGcontext* vg, float x, float y, float size_x, float size_y) {
	NVGcolor dark_highlight = nvgRGBA(212, 208, 200, 255);
	NVGcolor light_shadow = nvgRGBA(128, 128, 128, 255);
	NVGcolor highlight = nvgRGBA(255, 255, 255, 255);
	NVGcolor shadow = nvgRGBA(64, 64, 64, 255);

	nvgStrokeWidth(vg, 1.0f);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + 0.5f, y + size_y - 0.5f);
	nvgLineTo(vg, x + 0.5f, y + 0.5f);
	nvgLineTo(vg, x + size_x - 0.5f, y + 0.5f);
	nvgStrokeColor(vg, highlight);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + 1.5f, y + size_y - 1.5f);
	nvgLineTo(vg, x + 1.5f, y + 1.5f);
	nvgLineTo(vg, x + size_x - 1.5f, y + 1.5f);
	nvgStrokeColor(vg, dark_highlight);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + size_x - 1.5f, y + 1.5f);
	nvgLineTo(vg, x + size_x - 1.5f, y + size_y - 1.5f);
	nvgLineTo(vg, x + 1.5f, y + size_y - 1.5f);
	nvgStrokeColor(vg, light_shadow);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + size_x - 0.5f, y + 0.5f);
	nvgLineTo(vg, x + size_x - 0.5f, y + size_y - 0.5f);
	nvgLineTo(vg, x + 0.5f, y + size_y - 0.5f);
	nvgStrokeColor(vg, shadow);
	nvgStroke(vg);
}

void ContainerGridCanvas::DrawHoverEffects(NVGcontext* vg, float x, float y, float size_x, float size_y) {
	nvgBeginPath(vg);
	nvgRect(vg, x, y, size_x, size_y);
	wxColour hoverCol = Theme::Get(Theme::Role::AccentHover);
	nvgFillColor(vg, nvgRGBA(hoverCol.Red(), hoverCol.Green(), hoverCol.Blue(), 64)); // Add some transparency
	nvgFill(vg);
}

void ContainerGridCanvas::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	if (g_gui.gfx.isUnloaded()) {
		return;
	}

	if (!m_container || !m_container->asContainer()) {
		return;
	}

	Container* container = m_container->asContainer();
	int capacity = static_cast<int>(container->getVolume());

	// Fill background
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, width, height);
	// We use the panel background color to blend seamlessly
	wxColour bgColour = GetParent()->GetBackgroundColour();
	nvgFillColor(vg, nvgRGBA(bgColour.Red(), bgColour.Green(), bgColour.Blue(), 255));
	nvgFill(vg);

	int img_size = m_large ? 32 : 16;
	float btn_size_x = m_slot_size - m_padding * 2;
	float btn_size_y = m_slot_size - m_padding * 2;

	float offset_x = (btn_size_x - img_size) / 2.0f;
	float offset_y = (btn_size_y - img_size) / 2.0f;

	for (int index = 0; index < capacity; ++index) {
		int col = index % m_cols;
		int row = index / m_cols;

		float slot_x = col * m_slot_size + m_padding;
		float slot_y = row * m_slot_size + m_padding;

		// 1. Draw Slot Background (Black DCButton style backdrop)
		nvgBeginPath(vg);
		nvgRect(vg, slot_x, slot_y, btn_size_x, btn_size_y);
		nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
		nvgFill(vg);

		bool is_selected = (index == m_selected_index);
		bool is_hovered = (index == m_hover_index);

		// 2. Draw Borders (Raised unselected, Sunken selected)
		if (is_selected) {
			DrawSunkenBorder(vg, slot_x, slot_y, btn_size_x, btn_size_y);
		} else {
			DrawRaisedBorder(vg, slot_x, slot_y, btn_size_x, btn_size_y);
		}

		if (is_hovered && !is_selected) {
			DrawHoverEffects(vg, slot_x, slot_y, btn_size_x, btn_size_y);
		}

		// 3. Draw Sprite content if available
		Item* item = nullptr;
		if (index < static_cast<int>(container->getItemCount())) {
			item = container->getItem(index);
		}

		if (item) {
			Sprite* sprite = g_gui.gfx.getSprite(item->getClientID());
			if (sprite) {
				int tex = GetOrCreateSpriteTexture(vg, sprite);
				if (tex > 0) {
					NVGpaint imgPaint = nvgImagePattern(vg, slot_x + offset_x, slot_y + offset_y, img_size, img_size, 0, tex, 1.0f);
					nvgBeginPath(vg);
					nvgRect(vg, slot_x + offset_x, slot_y + offset_y, img_size, img_size);
					nvgFillPaint(vg, imgPaint);
					nvgFill(vg);

					// Overlays for selected
					if (is_selected && g_settings.getInteger(Config::USE_GUI_SELECTION_SHADOW)) {
						int overlayTex = GetOrCreateEditorIconTexture(vg, EDITOR_SPRITE_SELECTION_MARKER, (img_size <= 16) ? SPRITE_SIZE_16x16 : SPRITE_SIZE_32x32);
						if (overlayTex > 0) {
							NVGpaint ovPaint = nvgImagePattern(vg, slot_x + offset_x, slot_y + offset_y, img_size, img_size, 0, overlayTex, 1.0f);
							nvgBeginPath(vg);
							nvgRect(vg, slot_x + offset_x, slot_y + offset_y, img_size, img_size);
							nvgFillPaint(vg, ovPaint);
							nvgFill(vg);
						}
					}

					// Draw Count if > 1
					if (item->getCount() > 1 && item->isStackable()) {
						std::string countStr = std::to_string(item->getCount());
						nvgFontSize(vg, 10.0f);
						nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);

						float text_x = slot_x + btn_size_x - offset_x + 1.0f;
						float text_y = slot_y + btn_size_y - offset_y + 1.0f;

						// Shadow
						nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
						nvgText(vg, text_x + 1.0f, text_y + 1.0f, countStr.c_str(), nullptr);

						// Text
						wxColour countCol = Theme::Get(Theme::Role::TooltipCountText);
						nvgFillColor(vg, nvgRGBA(countCol.Red(), countCol.Green(), countCol.Blue(), 255));
						nvgText(vg, text_x, text_y, countStr.c_str(), nullptr);
					}
				}
			}
		}
	}
}
