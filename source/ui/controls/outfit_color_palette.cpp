#include "ui/controls/outfit_color_palette.h"
#include "rendering/core/outfit_colors.h"
#include "ui/theme.h"
#include "util/common.h"
#include "util/nvg_utils.h"

#include <nanovg.h>
#include <algorithm>
#include <format>
#include <wx/dcmemory.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

namespace {
	[[nodiscard]] wxColor swatchBorderColor(const wxColor& color) {
		const int luma = (static_cast<int>(color.Red()) * 299 + static_cast<int>(color.Green()) * 587 + static_cast<int>(color.Blue()) * 114) / 1000;
		return luma < 96 ? wxColour(200, 200, 200) : wxColour(48, 48, 48);
	}
}

OutfitColorPalette::OutfitColorPalette(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxBORDER_NONE) {
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_LEFT_DOWN, &OutfitColorPalette::OnMouse, this);
	Bind(wxEVT_MOTION, &OutfitColorPalette::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &OutfitColorPalette::OnLeave, this);
}

void OutfitColorPalette::SetSelectedColor(int index) {
	if (index >= 0 && index < static_cast<int>(TemplateOutfitLookupTableSize)) {
		m_selectedIndex = index;
		Refresh();
	}
}

void OutfitColorPalette::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	int cellSize = FromDIP(CELL_SIZE);
	int padding = FromDIP(PADDING);

	for (int i = 0; i < static_cast<int>(TemplateOutfitLookupTableSize); ++i) {
		int row = i / COLS;
		int col = i % COLS;

		float x = static_cast<float>(col * (cellSize + padding) + padding);
		float y = static_cast<float>(row * (cellSize + padding) + padding);

		uint32_t color = TemplateOutfitLookupTable[i];

		// Inner rect
		nvgBeginPath(vg);
		nvgRect(vg, x, y, static_cast<float>(cellSize), static_cast<float>(cellSize));
		nvgFillColor(vg, nvgRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
		nvgFill(vg);

		// Highlight
		if (i == m_selectedIndex) {
			float strokeWidth = std::max(1.0f, static_cast<float>(FromDIP(2)));
			float offset = strokeWidth / 2.0f + 0.5f; // Push out slightly

			nvgBeginPath(vg);
			nvgRect(vg, x - offset, y - offset, cellSize + offset * 2, cellSize + offset * 2);
			nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent)));
			nvgStrokeWidth(vg, strokeWidth);
			nvgStroke(vg);
		} else if (i == m_hoverIndex) {
			float strokeWidth = std::max(1.0f, static_cast<float>(FromDIP(1)));
			float offset = strokeWidth / 2.0f;

			nvgBeginPath(vg);
			nvgRect(vg, x - offset, y - offset, cellSize + offset * 2, cellSize + offset * 2);
			nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 128));
			nvgStrokeWidth(vg, strokeWidth);
			nvgStroke(vg);
		}
	}
}

wxSize OutfitColorPalette::DoGetBestClientSize() const {
	int cellSize = FromDIP(CELL_SIZE);
	int padding = FromDIP(PADDING);
	return wxSize(COLS * (cellSize + padding) + padding, ROWS * (cellSize + padding) + padding);
}

int OutfitColorPalette::HitTest(int x, int y) const {
	int cellSize = FromDIP(CELL_SIZE);
	int padding = FromDIP(PADDING);

	if (x < padding || y < padding) {
		return -1;
	}

	int offsetX = (x - padding) % (cellSize + padding);
	int offsetY = (y - padding) % (cellSize + padding);

	if (offsetX >= cellSize || offsetY >= cellSize) {
		return -1;
	}

	int col = (x - padding) / (cellSize + padding);
	int row = (y - padding) / (cellSize + padding);

	if (col >= 0 && col < COLS && row >= 0 && row < ROWS) {
		int index = row * COLS + col;
		if (index < static_cast<int>(TemplateOutfitLookupTableSize)) {
			return index;
		}
	}
	return -1;
}

void OutfitColorPalette::OnMouse(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != -1) {
		m_selectedIndex = index;
		Refresh();

		wxCommandEvent evt(wxEVT_BUTTON, GetId());
		evt.SetEventObject(this);
		evt.SetInt(index); // Pass the selected index
		ProcessEvent(evt);
	}
}

void OutfitColorPalette::OnMotion(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != m_hoverIndex) {
		m_hoverIndex = index;
		Refresh();
	}
	event.Skip();
}

void OutfitColorPalette::OnLeave(wxMouseEvent& event) {
	if (m_hoverIndex != -1) {
		m_hoverIndex = -1;
		Refresh();
	}
	event.Skip();
}

LightColorPickerDialog::LightColorPickerDialog(wxWindow* parent, int current_color) :
	wxDialog(parent, wxID_ANY, "Server Light Color", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	selected_color_(std::clamp(current_color, 0, static_cast<int>(rme::lighting::MAX_SERVER_LIGHT_COLOR))) {
	auto* main_sizer = new wxBoxSizer(wxVERTICAL);

	auto* title = new wxStaticText(this, wxID_ANY, "Choose the server light palette index used by OTClient.");
	main_sizer->Add(title, 0, wxALL, 10);

	palette_ = new LightColorPalette(this);
	palette_->SetSelectedColor(selected_color_);
	palette_->Bind(wxEVT_BUTTON, &LightColorPickerDialog::OnPaletteSelection, this);
	main_sizer->Add(palette_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

	auto* preview_sizer = new wxBoxSizer(wxHORIZONTAL);
	preview_bitmap_ = new wxStaticBitmap(this, wxID_ANY, CreateLightColorSwatchBitmap(this, selected_color_, wxSize(48, 48)));
	selection_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
	preview_sizer->Add(preview_bitmap_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
	preview_sizer->Add(selection_label_, 0, wxALIGN_CENTER_VERTICAL);
	main_sizer->Add(preview_sizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

	main_sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	SetSizerAndFit(main_sizer);
	UpdateSelection(selected_color_);
	CenterOnParent();
}

void LightColorPickerDialog::UpdateSelection(int color_index) {
	selected_color_ = std::clamp(color_index, 0, static_cast<int>(rme::lighting::MAX_SERVER_LIGHT_COLOR));
	if (palette_) {
		palette_->SetSelectedColor(selected_color_);
	}
	if (preview_bitmap_) {
		preview_bitmap_->SetBitmap(CreateLightColorSwatchBitmap(this, selected_color_, wxSize(48, 48)));
	}
	if (selection_label_) {
		const wxColor color = colorFromEightBit(selected_color_);
		selection_label_->SetLabelText(wxstr(std::format("Index {} | RGB({}, {}, {})", selected_color_, color.Red(), color.Green(), color.Blue())));
	}
}

void LightColorPickerDialog::OnPaletteSelection(wxCommandEvent& event) {
	UpdateSelection(event.GetInt());
}

wxBitmap CreateLightColorSwatchBitmap(const wxWindow* window, int color_index, const wxSize& size) {
	wxBitmap bitmap(window->FromDIP(size));
	wxMemoryDC dc(bitmap);
	const wxRect rect(wxPoint(0, 0), bitmap.GetSize());
	const wxColor fill_color = colorFromEightBit(color_index);
	dc.SetBackground(wxBrush(fill_color));
	dc.Clear();
	dc.SetPen(wxPen(swatchBorderColor(fill_color)));
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}

LightColorPalette::LightColorPalette(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxBORDER_NONE) {
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_LEFT_DOWN, &LightColorPalette::OnMouse, this);
	Bind(wxEVT_MOTION, &LightColorPalette::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &LightColorPalette::OnLeave, this);
}

void LightColorPalette::SetSelectedColor(int index) {
	if (index >= 0 && index < rme::lighting::LIGHT_PALETTE_SIZE) {
		selected_index_ = index;
		Refresh();
	}
}

void LightColorPalette::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	const float cell_size = static_cast<float>(FromDIP(CELL_SIZE));
	const float padding = static_cast<float>(FromDIP(PADDING));

	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::PanelBackground)));
	nvgFill(vg);

	for (int index = 0; index < rme::lighting::LIGHT_PALETTE_SIZE; ++index) {
		const int row = index / COLS;
		const int col = index % COLS;
		const float x = col * (cell_size + padding) + padding;
		const float y = row * (cell_size + padding) + padding;
		const wxColor color = colorFromEightBit(index);
		const wxColor border = swatchBorderColor(color);

		nvgBeginPath(vg);
		nvgRect(vg, x, y, cell_size, cell_size);
		nvgFillColor(vg, nvgRGBA(color.Red(), color.Green(), color.Blue(), 255));
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgRect(vg, x + 0.5f, y + 0.5f, cell_size - 1.0f, cell_size - 1.0f);
		nvgStrokeColor(vg, nvgRGBA(border.Red(), border.Green(), border.Blue(), 160));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		if (index == selected_index_) {
			const float stroke_width = std::max(1.0f, static_cast<float>(FromDIP(2)));
			const float offset = stroke_width / 2.0f + 0.5f;

			nvgBeginPath(vg);
			nvgRect(vg, x - offset, y - offset, cell_size + offset * 2.0f, cell_size + offset * 2.0f);
			nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent)));
			nvgStrokeWidth(vg, stroke_width);
			nvgStroke(vg);
		} else if (index == hover_index_) {
			const float stroke_width = std::max(1.0f, static_cast<float>(FromDIP(1)));
			const float offset = stroke_width / 2.0f;

			nvgBeginPath(vg);
			nvgRect(vg, x - offset, y - offset, cell_size + offset * 2.0f, cell_size + offset * 2.0f);
			nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 128));
			nvgStrokeWidth(vg, stroke_width);
			nvgStroke(vg);
		}
	}
}

wxSize LightColorPalette::DoGetBestClientSize() const {
	const int cell_size = FromDIP(CELL_SIZE);
	const int padding = FromDIP(PADDING);
	return {
		COLS * (cell_size + padding) + padding,
		ROWS * (cell_size + padding) + padding
	};
}

int LightColorPalette::HitTest(int x, int y) const {
	const int cell_size = FromDIP(CELL_SIZE);
	const int padding = FromDIP(PADDING);
	if (x < padding || y < padding) {
		return -1;
	}

	const int offset_x = (x - padding) % (cell_size + padding);
	const int offset_y = (y - padding) % (cell_size + padding);
	if (offset_x >= cell_size || offset_y >= cell_size) {
		return -1;
	}

	const int col = (x - padding) / (cell_size + padding);
	const int row = (y - padding) / (cell_size + padding);
	if (col < 0 || col >= COLS || row < 0 || row >= ROWS) {
		return -1;
	}

	const int index = row * COLS + col;
	return index < rme::lighting::LIGHT_PALETTE_SIZE ? index : -1;
}

void LightColorPalette::OnMouse(wxMouseEvent& event) {
	if (const int index = HitTest(event.GetX(), event.GetY()); index != -1) {
		selected_index_ = index;
		Refresh();

		wxCommandEvent selection_event(wxEVT_BUTTON, GetId());
		selection_event.SetEventObject(this);
		selection_event.SetInt(index);
		ProcessEvent(selection_event);
	}
}

void LightColorPalette::OnMotion(wxMouseEvent& event) {
	if (const int index = HitTest(event.GetX(), event.GetY()); index != hover_index_) {
		hover_index_ = index;
		Refresh();
	}
	event.Skip();
}

void LightColorPalette::OnLeave(wxMouseEvent& event) {
	if (hover_index_ != -1) {
		hover_index_ = -1;
		Refresh();
	}
	event.Skip();
}
