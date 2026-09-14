#include "ui/tool_options_surface.h"

#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/border/optional_border_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/flag/flag_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "game/sprites.h"
#include "palette/palette_window.h"
#include "rendering/core/game_sprite.h"
#include "ui/icons/sprite_icon_service.h"
#include "ui/gui.h"
#include "util/image_manager.h"

#include <algorithm>
#include <format>
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>

namespace {
	constexpr int BRUSH_ICON_SIZE = 32;
	constexpr int TOOL_COLUMNS = 6;
	constexpr int CREATURE_TOOL_COLUMNS = 2;
	constexpr int TOOL_BUTTON_SIZE = 34;
	constexpr int MODE_BUTTON_ICON_SIZE = 18;
	constexpr int MIN_AXIS_SIZE = 0;
	constexpr int MAX_AXIS_SIZE = 15;
	constexpr int MIN_THICKNESS = 1;
	constexpr int MAX_THICKNESS = 100;
	constexpr int MIN_SPAWN_TIME = 0;
	constexpr int MAX_SPAWN_TIME = 86400;
	const wxColour MODE_ON_COLOUR(102, 187, 106);
	const wxColour MODE_OFF_COLOUR(255, 214, 102);

	[[nodiscard]] int effectiveAxisSpan(int slider_value, bool exact) {
		return exact ? std::max(1, slider_value) : slider_value * 2 + 1;
	}

	void ConfigureFlatModeButton(wxBitmapToggleButton* button, const wxBitmap& bitmap) {
		if (!button) {
			return;
		}

		button->SetBitmap(bitmap);
		button->SetBitmapCurrent(bitmap);
		button->SetBitmapFocus(bitmap);
		button->SetBitmapPressed(bitmap);
	}

	void DetachWindowIfPresent(wxSizer* sizer, wxWindow* window) {
		if (!sizer || !window) {
			return;
		}

		sizer->Detach(window);
		window->Hide();
	}

	void AddWindowToSizer(wxSizer* sizer, wxWindow* window, int proportion, int flags, int border) {
		if (!sizer || !window) {
			return;
		}

		window->Show();
		sizer->Add(window, proportion, flags, border);
	}
}

ToolOptionsSurface::ToolOptionsSurface(wxWindow* parent) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL) {
	BuildUi();
	ReloadSettings();
}

void ToolOptionsSurface::BuildUi() {
	main_sizer = newd wxBoxSizer(wxVERTICAL);

	main_tools_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Main tools");
	main_tools_grid = newd wxGridSizer(0, TOOL_COLUMNS, FromDIP(4), FromDIP(4));
	main_tools_sizer->Add(main_tools_grid, 0, wxEXPAND | wxALL, FromDIP(4));
	main_sizer->Add(main_tools_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(4));

	size_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Size");

	const auto create_axis_row = [&](const wxString& label, wxSlider*& slider, wxStaticText*& value_label) {
		auto* row = newd wxBoxSizer(wxHORIZONTAL);
		const wxSize mode_button_size = FromDIP(wxSize(26, 26));
		if (label == "X") {
			exact_button = newd wxBitmapToggleButton(size_sizer->GetStaticBox(), wxID_ANY, wxBitmap(), wxDefaultPosition, mode_button_size, wxBU_EXACTFIT | wxBORDER_NONE);
			row->Add(exact_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
		} else {
			aspect_button = newd wxBitmapToggleButton(size_sizer->GetStaticBox(), wxID_ANY, wxBitmap(), wxDefaultPosition, mode_button_size, wxBU_EXACTFIT | wxBORDER_NONE);
			row->Add(aspect_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
		}
		row->Add(newd wxStaticText(size_sizer->GetStaticBox(), wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
		slider = newd wxSlider(size_sizer->GetStaticBox(), wxID_ANY, 0, MIN_AXIS_SIZE, MAX_AXIS_SIZE, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		row->Add(slider, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
		value_label = newd wxStaticText(size_sizer->GetStaticBox(), wxID_ANY, "1");
		row->Add(value_label, 0, wxALIGN_CENTER_VERTICAL);
		size_sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
	};

	create_axis_row("X", size_x_slider, size_x_value);
	create_axis_row("Y", size_y_slider, size_y_value);

	main_sizer->Add(size_sizer, 0, wxEXPAND | wxALL, FromDIP(4));

	other_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Other");

	thickness_panel = newd wxPanel(other_sizer->GetStaticBox(), wxID_ANY);
	auto* thickness_row = newd wxBoxSizer(wxHORIZONTAL);
	thickness_label = newd wxStaticText(thickness_panel, wxID_ANY, "Thickness");
	thickness_row->Add(thickness_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
	thickness_slider = newd wxSlider(thickness_panel, wxID_ANY, MAX_THICKNESS, MIN_THICKNESS, MAX_THICKNESS, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	thickness_row->Add(thickness_slider, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
	thickness_value = newd wxStaticText(thickness_panel, wxID_ANY, "100%");
	thickness_row->Add(thickness_value, 0, wxALIGN_CENTER_VERTICAL);
	thickness_panel->SetSizer(thickness_row);
	other_sizer->Add(thickness_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

	spawn_time_panel = newd wxPanel(other_sizer->GetStaticBox(), wxID_ANY);
	auto* spawn_time_row = newd wxBoxSizer(wxHORIZONTAL);
	spawn_time_label = newd wxStaticText(spawn_time_panel, wxID_ANY, "Spawntime");
	spawn_time_row->Add(spawn_time_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
	spawn_time_spin = newd wxSpinCtrl(
		spawn_time_panel,
		TOOL_OPTIONS_CREATURE_SPAWN_TIME,
		wxEmptyString,
		wxDefaultPosition,
		FromDIP(wxSize(72, 20)),
		wxSP_ARROW_KEYS,
		MIN_SPAWN_TIME,
		MAX_SPAWN_TIME,
		g_settings.getInteger(Config::DEFAULT_SPAWNTIME)
	);
	spawn_time_row->Add(spawn_time_spin, 0, wxALIGN_CENTER_VERTICAL);
	spawn_time_panel->SetSizer(spawn_time_row);
	other_sizer->Add(spawn_time_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

	place_spawn_with_creature_checkbox = newd wxCheckBox(other_sizer->GetStaticBox(), wxID_ANY, "Place spawn with creature");
	other_sizer->Add(place_spawn_with_creature_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

	spawn_size_panel = newd wxPanel(other_sizer->GetStaticBox(), wxID_ANY);
	auto* spawn_size_row = newd wxBoxSizer(wxHORIZONTAL);
	spawn_size_label = newd wxStaticText(spawn_size_panel, wxID_ANY, "Spawn Size");
	spawn_size_row->Add(spawn_size_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
	spawn_size_spin = newd wxSpinCtrl(
		spawn_size_panel,
		TOOL_OPTIONS_CREATURE_SPAWN_SIZE,
		wxEmptyString,
		wxDefaultPosition,
		FromDIP(wxSize(72, 20)),
		wxSP_ARROW_KEYS,
		1,
		g_settings.getInteger(Config::MAX_SPAWN_RADIUS),
		g_settings.getInteger(Config::CURRENT_SPAWN_RADIUS)
	);
	spawn_size_row->Add(spawn_size_spin, 0, wxALIGN_CENTER_VERTICAL);
	spawn_size_panel->SetSizer(spawn_size_row);
	other_sizer->Add(spawn_size_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

	preview_border_checkbox = newd wxCheckBox(other_sizer->GetStaticBox(), wxID_ANY, "Preview Border");
	lock_doors_checkbox = newd wxCheckBox(other_sizer->GetStaticBox(), wxID_ANY, "Lock Doors (Shift)");
	other_sizer->Add(preview_border_checkbox, 0, wxEXPAND | wxALL, FromDIP(6));
	other_sizer->Add(lock_doors_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));
	main_sizer->Add(other_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(4));

	SetSizer(main_sizer);

	size_x_slider->Bind(wxEVT_SLIDER, &ToolOptionsSurface::OnSizeXChanged, this);
	size_y_slider->Bind(wxEVT_SLIDER, &ToolOptionsSurface::OnSizeYChanged, this);
	exact_button->Bind(wxEVT_TOGGLEBUTTON, &ToolOptionsSurface::OnExactToggled, this);
	aspect_button->Bind(wxEVT_TOGGLEBUTTON, &ToolOptionsSurface::OnAspectToggled, this);
	preview_border_checkbox->Bind(wxEVT_CHECKBOX, &ToolOptionsSurface::OnPreviewBorderToggled, this);
	lock_doors_checkbox->Bind(wxEVT_CHECKBOX, &ToolOptionsSurface::OnLockDoorsToggled, this);
	thickness_slider->Bind(wxEVT_SLIDER, &ToolOptionsSurface::OnThicknessChanged, this);
	place_spawn_with_creature_checkbox->Bind(wxEVT_CHECKBOX, &ToolOptionsSurface::OnPlaceSpawnWithCreatureToggled, this);
	spawn_time_spin->Bind(wxEVT_SPINCTRL, &ToolOptionsSurface::OnSpawnTimeChanged, this);
	spawn_size_spin->Bind(wxEVT_SPINCTRL, &ToolOptionsSurface::OnSpawnSizeChanged, this);
}

void ToolOptionsSurface::SetActiveBrush(Brush* brush) {
	active_brush = brush;
	RefreshFromState();
}

void ToolOptionsSurface::UpdateBrushSize(BrushShape shape, int size) {
	(void)shape;
	(void)size;
	RefreshFromState();
}

void ToolOptionsSurface::ReloadSettings() {
	active_brush = g_gui.GetCurrentBrush();
	RefreshFromState();
}

void ToolOptionsSurface::Clear() {
	active_brush = nullptr;
	RefreshFromState();
}

void ToolOptionsSurface::EnsureToolButtons() {
	const Brush* creature_brush = GetSelectedCreatureBrush();
	const bool creature_mode = IsCreatureToolMode();
	const int desired_count = creature_mode ? 2 : static_cast<int>(GetDefaultTools().size());

	if (static_cast<int>(tool_buttons.size()) != desired_count) {
		RebuildToolButtons();
		return;
	}

	if (creature_mode) {
		if (tool_buttons.size() != 2 ||
			tool_buttons[0].action != ToolButtonAction::SelectCreature ||
			tool_buttons[0].brush != creature_brush ||
			tool_buttons[1].action != ToolButtonAction::SelectSpawn ||
			tool_buttons[1].brush != g_brush_manager.spawn_brush) {
			RebuildToolButtons();
		}
		return;
	}

	const auto default_tools = GetDefaultTools();
	for (size_t index = 0; index < default_tools.size(); ++index) {
		if (tool_buttons[index].action != ToolButtonAction::SelectBrush || tool_buttons[index].brush != default_tools[index]) {
			RebuildToolButtons();
			return;
		}
	}
}

void ToolOptionsSurface::RebuildToolButtons() {
	if (!main_tools_grid) {
		return;
	}

	for (auto& entry : tool_buttons) {
		if (entry.button) {
			main_tools_grid->Detach(entry.button);
			entry.button->Destroy();
		}
	}
	tool_buttons.clear();

	main_tools_grid->SetCols(IsCreatureToolMode() ? CREATURE_TOOL_COLUMNS : TOOL_COLUMNS);

	if (IsCreatureToolMode()) {
		tool_buttons.push_back(ToolButtonEntry {
			.action = ToolButtonAction::SelectCreature,
			.brush = GetSelectedCreatureBrush(),
			.asset_path = IMAGE_PLACE_CREATURE,
		});
		tool_buttons.push_back(ToolButtonEntry {
			.action = ToolButtonAction::SelectSpawn,
			.brush = g_brush_manager.spawn_brush,
			.asset_path = IMAGE_PLACE_SPAWN,
		});
	} else {
		for (Brush* brush : GetDefaultTools()) {
			tool_buttons.push_back(ToolButtonEntry {
				.action = ToolButtonAction::SelectBrush,
				.brush = brush,
			});
		}
	}

	for (auto& entry : tool_buttons) {
		const wxWindowID button_id = entry.action == ToolButtonAction::SelectCreature ? TOOL_OPTIONS_PLACE_CREATURE_BUTTON :
			entry.action == ToolButtonAction::SelectSpawn ? TOOL_OPTIONS_PLACE_SPAWN_BUTTON :
			wxID_ANY;
		auto* button = newd wxBitmapToggleButton(
			main_tools_sizer->GetStaticBox(),
			button_id,
			CreateToolBitmap(entry),
			wxDefaultPosition,
			FromDIP(wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE)),
			wxBU_EXACTFIT
		);
		button->SetMinSize(FromDIP(wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE)));
		button->SetMaxSize(FromDIP(wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE)));
		button->Bind(wxEVT_TOGGLEBUTTON, &ToolOptionsSurface::OnToolButton, this);
		main_tools_grid->Add(button, 0, wxALIGN_CENTER);
		entry.button = button;
	}

	SyncToolSelection();
	UpdateSectionVisibility();
	Layout();
}

void ToolOptionsSurface::RefreshFromState() {
	SetMutatingUi(true);

	active_brush = g_gui.GetCurrentBrush();
	EnsureToolButtons();

	const auto state = g_gui.GetBrushSizeState();
	const int size_min = state.exact ? 1 : 0;
	size_x_slider->SetRange(size_min, MAX_AXIS_SIZE);
	size_y_slider->SetRange(size_min, MAX_AXIS_SIZE);
	size_x_slider->SetValue(state.size_x);
	size_y_slider->SetValue(state.size_y);
	exact_button->SetValue(state.exact);
	aspect_button->SetValue(state.aspect_locked);
	preview_border_checkbox->SetValue(g_settings.getInteger(Config::SHOW_AUTOBORDER_PREVIEW));
	lock_doors_checkbox->SetValue(g_settings.getInteger(Config::DRAW_LOCKED_DOOR));
	const int thickness_percent = g_brush_manager.UseCustomThickness() ? std::clamp(static_cast<int>(g_brush_manager.GetCustomThicknessMod() * 100.0f), MIN_THICKNESS, MAX_THICKNESS) : MAX_THICKNESS;
	thickness_slider->SetValue(thickness_percent);
	place_spawn_with_creature_checkbox->SetValue(g_settings.getInteger(Config::AUTO_CREATE_SPAWN));
	spawn_time_spin->SetValue(std::max(MIN_SPAWN_TIME, g_settings.getInteger(Config::DEFAULT_SPAWNTIME)));
	spawn_size_spin->SetRange(1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS));
	spawn_size_spin->SetValue(std::clamp(g_settings.getInteger(Config::CURRENT_SPAWN_RADIUS), 1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS)));
	UpdateSizeLabels();
	UpdateModeButtons();
	thickness_value->SetLabel(std::format("{}%", thickness_slider->GetValue()));
	SyncToolSelection();
	SetMutatingUi(false);

	UpdateSectionVisibility();
	Layout();
}

void ToolOptionsSurface::UpdateSectionVisibility() {
	const bool show_tools = !tool_buttons.empty();
	const bool show_size = HasBrushSizeControls();
	const bool show_thickness = HasThicknessControl();
	const bool show_preview_border = HasPreviewBorderControl();
	const bool show_lock_doors = HasLockDoorsControl();
	const bool show_spawn_controls = HasSpawnControls();
	const bool show_other = show_thickness || show_preview_border || show_lock_doors || show_spawn_controls;

	main_sizer->Show(main_tools_sizer, show_tools, true);
	main_sizer->Show(size_sizer, show_size, true);

	other_sizer->Clear(false);
	DetachWindowIfPresent(other_sizer, thickness_panel);
	DetachWindowIfPresent(other_sizer, spawn_time_panel);
	DetachWindowIfPresent(other_sizer, place_spawn_with_creature_checkbox);
	DetachWindowIfPresent(other_sizer, spawn_size_panel);
	DetachWindowIfPresent(other_sizer, preview_border_checkbox);
	DetachWindowIfPresent(other_sizer, lock_doors_checkbox);

	if (show_thickness) {
		AddWindowToSizer(other_sizer, thickness_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
	}
	if (show_spawn_controls) {
		AddWindowToSizer(other_sizer, spawn_time_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
		AddWindowToSizer(other_sizer, spawn_size_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
		AddWindowToSizer(other_sizer, place_spawn_with_creature_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
	}
	if (show_preview_border) {
		AddWindowToSizer(other_sizer, preview_border_checkbox, 0, wxEXPAND | wxALL, FromDIP(6));
	}
	if (show_lock_doors) {
		AddWindowToSizer(other_sizer, lock_doors_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));
	}

	main_sizer->Show(other_sizer, show_other, true);
	other_sizer->Layout();
	main_sizer->Layout();
}

void ToolOptionsSurface::UpdateSizeLabels() {
	const bool exact = g_gui.IsExactBrushSize();
	size_x_value->SetLabel(std::format("{}", effectiveAxisSpan(g_gui.GetBrushSizeX(), exact)));
	size_y_value->SetLabel(std::format("{}", effectiveAxisSpan(g_gui.GetBrushSizeY(), exact)));
}

void ToolOptionsSurface::UpdateModeButtons() {
	if (exact_button) {
		const bool exact = g_gui.IsExactBrushSize();
		const auto bitmap = CreateModeBitmap("svg/solid/bullseye.svg", exact ? MODE_ON_COLOUR : MODE_OFF_COLOUR);
		ConfigureFlatModeButton(exact_button, bitmap);
	}

	if (aspect_button) {
		const bool connected = g_gui.IsBrushAspectRatioLocked();
		const auto bitmap = CreateModeBitmap(connected ? "svg/solid/link.svg" : "svg/solid/unlink.svg", connected ? MODE_ON_COLOUR : MODE_OFF_COLOUR);
		ConfigureFlatModeButton(aspect_button, bitmap);
	}
}

void ToolOptionsSurface::SyncToolSelection() {
	for (const auto& entry : tool_buttons) {
		if (!entry.button) {
			continue;
		}

		bool selected = false;
		switch (entry.action) {
			case ToolButtonAction::SelectBrush:
				selected = active_brush == entry.brush;
				break;
			case ToolButtonAction::SelectCreature:
				selected = active_brush && active_brush->is<CreatureBrush>();
				break;
			case ToolButtonAction::SelectSpawn:
				selected = active_brush == g_brush_manager.spawn_brush;
				break;
		}
		entry.button->SetValue(selected);
	}
}

void ToolOptionsSurface::SetMutatingUi(bool value) {
	mutating_ui = value;
}

bool ToolOptionsSurface::IsMutatingUi() const {
	return mutating_ui;
}

bool ToolOptionsSurface::IsCreatureToolMode() const {
	return active_brush && (active_brush->is<CreatureBrush>() || active_brush->is<SpawnBrush>());
}

bool ToolOptionsSurface::HasBrushSizeControls() const {
	if (!active_brush || HasSpawnControls()) {
		return false;
	}

	return !active_brush->is<WaypointBrush>() && !active_brush->is<HouseExitBrush>();
}

bool ToolOptionsSurface::HasThicknessControl() const {
	return active_brush && active_brush->is<DoodadBrush>();
}

bool ToolOptionsSurface::HasPreviewBorderControl() const {
	return active_brush && active_brush->needBorders();
}

bool ToolOptionsSurface::HasLockDoorsControl() const {
	return active_brush && active_brush->is<DoorBrush>();
}

bool ToolOptionsSurface::HasSpawnControls() const {
	return IsCreatureToolMode();
}

Brush* ToolOptionsSurface::GetSelectedCreatureBrush() const {
	if (auto* palette = g_gui.GetPalette()) {
		return palette->GetSelectedCreatureBrush();
	}
	return nullptr;
}

std::vector<Brush*> ToolOptionsSurface::GetDefaultTools() const {
	std::vector<Brush*> brushes;

	if (g_brush_manager.optional_brush) {
		brushes.push_back(g_brush_manager.optional_brush);
	}
	if (g_brush_manager.eraser) {
		brushes.push_back(g_brush_manager.eraser);
	}
	if (g_brush_manager.pz_brush) {
		brushes.push_back(g_brush_manager.pz_brush);
	}
	if (g_brush_manager.rook_brush) {
		brushes.push_back(g_brush_manager.rook_brush);
	}
	if (g_brush_manager.nolog_brush) {
		brushes.push_back(g_brush_manager.nolog_brush);
	}
	if (g_brush_manager.pvp_brush) {
		brushes.push_back(g_brush_manager.pvp_brush);
	}
	if (g_brush_manager.normal_door_brush) {
		brushes.push_back(g_brush_manager.normal_door_brush);
	}
	if (g_brush_manager.locked_door_brush) {
		brushes.push_back(g_brush_manager.locked_door_brush);
	}
	if (g_brush_manager.magic_door_brush) {
		brushes.push_back(g_brush_manager.magic_door_brush);
	}
	if (g_brush_manager.quest_door_brush) {
		brushes.push_back(g_brush_manager.quest_door_brush);
	}
	if (g_brush_manager.hatch_door_brush) {
		brushes.push_back(g_brush_manager.hatch_door_brush);
	}
	if (g_brush_manager.window_door_brush) {
		brushes.push_back(g_brush_manager.window_door_brush);
	}
	if (g_brush_manager.archway_door_brush) {
		brushes.push_back(g_brush_manager.archway_door_brush);
	}

	return brushes;
}

wxBitmap ToolOptionsSurface::CreateToolBitmap(const ToolButtonEntry& entry) const {
	if (!entry.asset_path.empty()) {
		return IMAGE_MANAGER.GetBitmap(entry.asset_path, FromDIP(wxSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE)));
	}
	return CreateBrushBitmap(entry.brush);
}

wxBitmap ToolOptionsSurface::CreateBrushBitmap(Brush* brush) const {
	if (!brush) {
		return wxBitmap(FromDIP(wxSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE)));
	}

	Sprite* sprite = brush->getSprite();
	if (!sprite && brush->getLookID() != 0) {
		sprite = g_gui.gfx.getSprite(brush->getLookID());
	}

	if (sprite) {
		wxBitmap bitmap(FromDIP(wxSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE)));
		wxMemoryDC dc(bitmap);
		dc.SetBackground(*wxWHITE_BRUSH);
		dc.Clear();
		const int x_offset = (bitmap.GetWidth() - BRUSH_ICON_SIZE) / 2;
		const int y_offset = (bitmap.GetHeight() - BRUSH_ICON_SIZE) / 2;
		SpriteIconService::DrawTo(sprite, &dc, SPRITE_SIZE_32x32, x_offset, y_offset, BRUSH_ICON_SIZE, BRUSH_ICON_SIZE);
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}

	wxBitmap bitmap(FromDIP(wxSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE)));
	wxMemoryDC dc(bitmap);
	dc.SetBackground(*wxLIGHT_GREY_BRUSH);
	dc.Clear();
	dc.DrawLabel(wxstr(brush->getName()).Left(1), wxRect(wxPoint(0, 0), bitmap.GetSize()), wxALIGN_CENTER);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}

wxBitmap ToolOptionsSurface::CreateModeBitmap(std::string_view assetPath, const wxColour& tint) const {
	return IMAGE_MANAGER.GetBitmap(assetPath, FromDIP(wxSize(MODE_BUTTON_ICON_SIZE, MODE_BUTTON_ICON_SIZE)), tint);
}

void ToolOptionsSurface::OnToolButton(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	auto* button = dynamic_cast<wxBitmapToggleButton*>(event.GetEventObject());
	if (!button) {
		return;
	}

	for (const auto& entry : tool_buttons) {
		if (entry.button != button) {
			continue;
		}

		switch (entry.action) {
			case ToolButtonAction::SelectBrush:
				if (entry.brush) {
					active_brush = entry.brush;
					g_gui.SelectBrush(entry.brush);
					g_gui.SetStatusText(std::format("Selected Tool: {}", entry.brush->getName()));
				}
				break;
			case ToolButtonAction::SelectCreature:
				if (Brush* creature_brush = GetSelectedCreatureBrush()) {
					active_brush = creature_brush;
					g_gui.SetSpawnTime(spawn_time_spin->GetValue());
					g_gui.SelectBrush(creature_brush);
					g_gui.SetStatusText("Selected Tool: Place Creature");
				}
				break;
			case ToolButtonAction::SelectSpawn:
				if (g_brush_manager.spawn_brush) {
					active_brush = g_brush_manager.spawn_brush;
					g_gui.SetSpawnTime(spawn_time_spin->GetValue());
					g_gui.SetBrushSize(spawn_size_spin->GetValue());
					g_gui.SelectBrush(g_brush_manager.spawn_brush);
					g_gui.SetStatusText("Selected Tool: Place Spawn");
				}
				break;
		}
		break;
	}

	SyncToolSelection();
}

void ToolOptionsSurface::OnSizeXChanged(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetBrushSizeX(event.GetInt());
	UpdateSizeLabels();
	g_gui.SetStatusText(std::format("Brush size X: {}", effectiveAxisSpan(g_gui.GetBrushSizeX(), g_gui.IsExactBrushSize())));
}

void ToolOptionsSurface::OnSizeYChanged(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetBrushSizeY(event.GetInt());
	UpdateSizeLabels();
	g_gui.SetStatusText(std::format("Brush size Y: {}", effectiveAxisSpan(g_gui.GetBrushSizeY(), g_gui.IsExactBrushSize())));
}

void ToolOptionsSurface::OnExactToggled(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetExactBrushSize(event.IsChecked());
	RefreshFromState();
}

void ToolOptionsSurface::OnAspectToggled(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetBrushAspectRatioLocked(event.IsChecked());
	RefreshFromState();
}

void ToolOptionsSurface::OnPreviewBorderToggled(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_settings.setInteger(Config::SHOW_AUTOBORDER_PREVIEW, event.IsChecked());
	g_gui.RefreshView();
}

void ToolOptionsSurface::OnLockDoorsToggled(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_settings.setInteger(Config::DRAW_LOCKED_DOOR, event.IsChecked());
	g_brush_manager.SetDoorLocked(event.IsChecked());
}

void ToolOptionsSurface::OnThicknessChanged(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	thickness_value->SetLabel(std::format("{}%", event.GetInt()));
	g_brush_manager.SetBrushThickness(true, event.GetInt(), 100);
}

void ToolOptionsSurface::OnPlaceSpawnWithCreatureToggled(wxCommandEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_settings.setInteger(Config::AUTO_CREATE_SPAWN, event.IsChecked());
}

void ToolOptionsSurface::OnSpawnTimeChanged(wxSpinEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetSpawnTime(event.GetPosition());
}

void ToolOptionsSurface::OnSpawnSizeChanged(wxSpinEvent& event) {
	if (IsMutatingUi()) {
		return;
	}

	g_gui.SetBrushSize(event.GetPosition());
}
