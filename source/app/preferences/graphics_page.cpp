#include "app/preferences/graphics_page.h"
#include "rendering/core/hardware_profile.h"
#include <algorithm>
#include <cmath>
#include <format>

#include "app/main.h"
#include "app/preferences/preferences_layout.h"
#include "app/settings.h"
#include "ui/gui.h"
#include "ui/managers/vsync_policy.h"

GraphicsPage::GraphicsPage(wxWindow* parent) : ScrollablePreferencesPage(parent) {
	auto* page_sizer = GetPageSizer();

	auto* rendering_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Rendering",
		"These controls change how the map view is drawn and filtered while you work."
	);
	hide_items_when_zoomed_chkbox = PreferencesLayout::AddCheckBoxRow(
		rendering_section,
		"Hide loose items when zoomed out",
		"Reduce clutter in distant views by hiding loose items once the editor is heavily zoomed out.",
		g_settings.getBoolean(Config::HIDE_ITEMS_WHEN_ZOOMED)
	);
	anti_aliasing_chkbox = PreferencesLayout::AddCheckBoxRow(
		rendering_section,
		"Enable anti-aliasing",
		"Smooth map rendering using linear interpolation so scaled views appear less jagged.",
		g_settings.getBoolean(Config::ANTI_ALIASING)
	);
	page_sizer->Add(rendering_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	auto* palette_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Palette Appearance",
		"These options affect how icons and selections look in palette-style UI panels."
	);
	icon_selection_shadow_chkbox = PreferencesLayout::AddCheckBoxRow(
		palette_section,
		"Use icon selection shadow",
		"Shade selected palette entries more strongly so the current selection stands out.",
		g_settings.getBoolean(Config::USE_GUI_SELECTION_SHADOW)
	);
	icon_background_choice = new wxChoice(palette_section, wxID_ANY);
	icon_background_choice->Append("Black background");
	icon_background_choice->Append("Gray background");
	icon_background_choice->Append("White background");
	if (g_settings.getInteger(Config::ICON_BACKGROUND) == 255) {
		icon_background_choice->SetSelection(2);
	} else if (g_settings.getInteger(Config::ICON_BACKGROUND) == 88) {
		icon_background_choice->SetSelection(1);
	} else {
		icon_background_choice->SetSelection(0);
	}
	PreferencesLayout::AddControlRow(
		palette_section,
		"Icon background color",
		"Choose a neutral icon backdrop that makes palette assets easier to scan across dialogs and browsers.",
		icon_background_choice
	);
	page_sizer->Add(palette_section, 0, wxEXPAND | wxALL, FromDIP(10));

	auto create_color_opacity_control = [&](
		wxWindow* parent_win,
		wxColourPickerCtrl*& out_picker,
		wxSpinCtrl*& out_spin,
		wxChoice*& out_choice,
		Config::Key red_key,
		Config::Key green_key,
		Config::Key blue_key,
		Config::Key alpha_key,
		Config::Key mode_key
	) -> wxWindow* {
		auto* container = new wxPanel(parent_win, wxID_ANY);
		auto* sizer = new wxBoxSizer(wxHORIZONTAL);

		wxColour initial_color(
			g_settings.getInteger(red_key),
			g_settings.getInteger(green_key),
			g_settings.getInteger(blue_key)
		);
		out_picker = new wxColourPickerCtrl(container, wxID_ANY, initial_color);

		int current_alpha = g_settings.getInteger(alpha_key);
		int current_opacity = std::clamp(static_cast<int>(std::round((current_alpha * 100.0f) / 255.0f)), 0, 100);

		out_spin = new wxSpinCtrl(container, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(65, -1)), wxSP_ARROW_KEYS, 0, 100, current_opacity);

		wxArrayString blend_modes;
		blend_modes.Add("Alpha Blend");
		blend_modes.Add("Multiplicative");
		out_choice = new wxChoice(container, wxID_ANY, wxDefaultPosition, wxDefaultSize, blend_modes);
		out_choice->SetSelection(std::clamp(g_settings.getInteger(mode_key), 0, 1));

		sizer->Add(out_picker, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
		sizer->Add(new wxStaticText(container, wxID_ANY, "Opacity:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
		sizer->Add(out_spin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
		sizer->Add(new wxStaticText(container, wxID_ANY, "%"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
		sizer->Add(new wxStaticText(container, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
		sizer->Add(out_choice, 0, wxALIGN_CENTER_VERTICAL);

		container->SetSizerAndFit(sizer);
		return container;
	};

	auto* cursor_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Cursor",
		"Customize the map cursor colors used for drawing, houses, flags, and similar overlays."
	);
	PreferencesLayout::AddControlRow(
		cursor_section,
		"Primary cursor color",
		"Color, opacity, and blend mode for the main drawing cursor.",
		create_color_opacity_control(cursor_section, cursor_color_pick, cursor_opacity_spin, cursor_blend_choice,
			Config::CURSOR_RED, Config::CURSOR_GREEN, Config::CURSOR_BLUE, Config::CURSOR_ALPHA, Config::CURSOR_BLEND_MODE)
	);
	PreferencesLayout::AddControlRow(
		cursor_section,
		"Secondary cursor color",
		"Color, opacity, and blend mode for alternate cursors (house, flag tools).",
		create_color_opacity_control(cursor_section, cursor_alt_color_pick, cursor_alt_opacity_spin, cursor_alt_blend_choice,
			Config::CURSOR_ALT_RED, Config::CURSOR_ALT_GREEN, Config::CURSOR_ALT_BLUE, Config::CURSOR_ALT_ALPHA, Config::CURSOR_ALT_BLEND_MODE)
	);
	page_sizer->Add(cursor_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* zone_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Zone & Overlay Appearance",
		"Customize the colors, fill opacities, and perimeter borders for special zones, pathing, spawns, and houses."
	);

	zone_borders_enabled_chkbox = PreferencesLayout::AddCheckBoxRow(
		zone_section,
		"Draw outer zone borders",
		"Render high-contrast 2px borders around outer perimeters of special zones, blocking tiles, and spawns.",
		g_settings.getBoolean(Config::ZONE_BORDERS_ENABLED)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Zone border color",
		"Outline color, opacity, and blend mode drawn along outer boundaries when zone borders are enabled.",
		create_color_opacity_control(zone_section, zone_border_color_pick, zone_border_opacity_spin, zone_border_blend_choice,
			Config::ZONE_BORDER_COLOR_R, Config::ZONE_BORDER_COLOR_G, Config::ZONE_BORDER_COLOR_B, Config::ZONE_BORDER_COLOR_A, Config::ZONE_BORDER_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Protection Zone",
		"Color, opacity, and blend mode for Protection Zone (PZ) tiles.",
		create_color_opacity_control(zone_section, zone_pz_color_pick, zone_pz_opacity_spin, zone_pz_blend_choice,
			Config::ZONE_PZ_COLOR_R, Config::ZONE_PZ_COLOR_G, Config::ZONE_PZ_COLOR_B, Config::ZONE_PZ_COLOR_A, Config::ZONE_PZ_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Non-PvP Zone",
		"Color, opacity, and blend mode for Non-PvP zone tiles.",
		create_color_opacity_control(zone_section, zone_nopvp_color_pick, zone_nopvp_opacity_spin, zone_nopvp_blend_choice,
			Config::ZONE_NOPVP_COLOR_R, Config::ZONE_NOPVP_COLOR_G, Config::ZONE_NOPVP_COLOR_B, Config::ZONE_NOPVP_COLOR_A, Config::ZONE_NOPVP_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"No-Logout Zone",
		"Color, opacity, and blend mode for No-Logout zone tiles.",
		create_color_opacity_control(zone_section, zone_nologout_color_pick, zone_nologout_opacity_spin, zone_nologout_blend_choice,
			Config::ZONE_NOLOGOUT_COLOR_R, Config::ZONE_NOLOGOUT_COLOR_G, Config::ZONE_NOLOGOUT_COLOR_B, Config::ZONE_NOLOGOUT_COLOR_A, Config::ZONE_NOLOGOUT_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"PvP Zone",
		"Color, opacity, and blend mode for PvP zone tiles.",
		create_color_opacity_control(zone_section, zone_pvp_color_pick, zone_pvp_opacity_spin, zone_pvp_blend_choice,
			Config::ZONE_PVP_COLOR_R, Config::ZONE_PVP_COLOR_G, Config::ZONE_PVP_COLOR_B, Config::ZONE_PVP_COLOR_A, Config::ZONE_PVP_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Blocking / Pathing",
		"Color, opacity, and blend mode for unwalkable and blocking collision tiles.",
		create_color_opacity_control(zone_section, zone_blocking_color_pick, zone_blocking_opacity_spin, zone_blocking_blend_choice,
			Config::ZONE_BLOCKING_COLOR_R, Config::ZONE_BLOCKING_COLOR_G, Config::ZONE_BLOCKING_COLOR_B, Config::ZONE_BLOCKING_COLOR_A, Config::ZONE_BLOCKING_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Spawn Radius",
		"Color, opacity, and blend mode for monster and NPC spawn radius tiles.",
		create_color_opacity_control(zone_section, zone_spawn_color_pick, zone_spawn_opacity_spin, zone_spawn_blend_choice,
			Config::ZONE_SPAWN_COLOR_R, Config::ZONE_SPAWN_COLOR_G, Config::ZONE_SPAWN_COLOR_B, Config::ZONE_SPAWN_COLOR_A, Config::ZONE_SPAWN_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Selected House",
		"Color, opacity, and blend mode for the currently selected/edited house tiles.",
		create_color_opacity_control(zone_section, house_active_color_pick, house_active_opacity_spin, house_active_blend_choice,
			Config::HOUSE_ACTIVE_COLOR_R, Config::HOUSE_ACTIVE_COLOR_G, Config::HOUSE_ACTIVE_COLOR_B, Config::HOUSE_ACTIVE_COLOR_A, Config::HOUSE_ACTIVE_BLEND_MODE)
	);

	PreferencesLayout::AddControlRow(
		zone_section,
		"Other Houses",
		"Color, opacity, and blend mode for other (inactive) house tiles.",
		create_color_opacity_control(zone_section, house_inactive_color_pick, house_inactive_opacity_spin, house_inactive_blend_choice,
			Config::HOUSE_INACTIVE_COLOR_R, Config::HOUSE_INACTIVE_COLOR_G, Config::HOUSE_INACTIVE_COLOR_B, Config::HOUSE_INACTIVE_COLOR_A, Config::HOUSE_INACTIVE_BLEND_MODE)
	);

	reset_zone_defaults_btn = new wxButton(zone_section, wxID_ANY, "Reset Overlays to Defaults");
	reset_zone_defaults_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		zone_borders_enabled_chkbox->SetValue(true);
		zone_border_color_pick->SetColour(wxColour(13, 13, 18));
		zone_border_opacity_spin->SetValue(98);
		zone_border_blend_choice->SetSelection(0);

		zone_pz_color_pick->SetColour(wxColour(20, 117, 255));
		zone_pz_opacity_spin->SetValue(48);
		zone_pz_blend_choice->SetSelection(0);

		zone_nopvp_color_pick->SetColour(wxColour(0, 219, 92));
		zone_nopvp_opacity_spin->SetValue(46);
		zone_nopvp_blend_choice->SetSelection(0);

		zone_nologout_color_pick->SetColour(wxColour(255, 122, 0));
		zone_nologout_opacity_spin->SetValue(48);
		zone_nologout_blend_choice->SetSelection(0);

		zone_pvp_color_pick->SetColour(wxColour(245, 26, 51));
		zone_pvp_opacity_spin->SetValue(48);
		zone_pvp_blend_choice->SetSelection(0);

		zone_blocking_color_pick->SetColour(wxColour(0, 0, 0));
		zone_blocking_opacity_spin->SetValue(50);
		zone_blocking_blend_choice->SetSelection(1);

		zone_spawn_color_pick->SetColour(wxColour(242, 26, 242));
		zone_spawn_opacity_spin->SetValue(44);
		zone_spawn_blend_choice->SetSelection(1);

		house_active_color_pick->SetColour(wxColour(89, 191, 13));
		house_active_opacity_spin->SetValue(52);
		house_active_blend_choice->SetSelection(1);

		house_inactive_color_pick->SetColour(wxColour(92, 56, 166));
		house_inactive_opacity_spin->SetValue(52);
		house_inactive_blend_choice->SetSelection(1);
	});
	PreferencesLayout::AddControlRow(
		zone_section,
		"Restore Defaults",
		"Revert all zone, blocking, spawn, and house overlay colors and opacities to calibrated factory defaults.",
		reset_zone_defaults_btn
	);

	page_sizer->Add(zone_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* screenshot_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Screenshots",
		"Configure where screenshots are saved and which file format is used when capturing the viewport."
	);
	screenshot_directory_picker = new wxDirPickerCtrl(screenshot_section, wxID_ANY, wxstr(g_settings.getString(Config::SCREENSHOT_DIRECTORY)));
	PreferencesLayout::AddControlRow(
		screenshot_section,
		"Screenshot directory",
		"Folder where screenshots taken from the editor are stored.",
		screenshot_directory_picker,
		true
	);
	screenshot_format_choice = new wxChoice(screenshot_section, wxID_ANY);
	screenshot_format_choice->Append("PNG");
	screenshot_format_choice->Append("JPG");
	screenshot_format_choice->Append("TGA");
	screenshot_format_choice->Append("BMP");
	const auto screenshot_format = g_settings.getString(Config::SCREENSHOT_FORMAT);
	if (screenshot_format == "jpg") {
		screenshot_format_choice->SetSelection(1);
	} else if (screenshot_format == "tga") {
		screenshot_format_choice->SetSelection(2);
	} else if (screenshot_format == "bmp") {
		screenshot_format_choice->SetSelection(3);
	} else {
		screenshot_format_choice->SetSelection(0);
	}
	PreferencesLayout::AddControlRow(
		screenshot_section,
		"Screenshot format",
		"File type used when you capture the map view with F11.",
		screenshot_format_choice
	);
	page_sizer->Add(screenshot_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* performance_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Performance",
		"Manage sprite caching and frame pacing to balance memory use, speed, and responsiveness."
	);
	use_memcached_chkbox = PreferencesLayout::AddCheckBoxRow(
		performance_section,
		"Cache sprites in memory",
		"Load sprites into memory up front for faster browsing and rendering at the cost of higher RAM use.",
		g_settings.getBoolean(Config::USE_MEMCACHED_SPRITES)
	);
	PreferencesLayout::AddNotice(
		performance_section,
		"Changing sprite caching requires an application restart before the new loading mode takes effect.",
		Theme::Role::Warning
	);
	vsync_choice = new wxChoice(performance_section, wxID_ANY);
	vsync_choice->Append("Off");
	vsync_choice->Append("On");
	vsync_choice->Append("Adaptive");
	vsync_choice->SetSelection(static_cast<int>(sanitizeVSyncMode(g_settings.getInteger(Config::VSYNC_MODE))));
	PreferencesLayout::AddControlRow(
		performance_section,
		"VSync",
		"Reduce tearing by synchronizing buffer swaps to the display refresh rate. Adaptive mode may fall back to standard vSync depending on the driver.",
		vsync_choice
	);
	show_fps_chkbox = PreferencesLayout::AddCheckBoxRow(
		performance_section,
		"Show FPS counter",
		"Display the current frame rate in the editor status area while you work.",
		g_settings.getBoolean(Config::SHOW_FPS_COUNTER)
	);
	page_sizer->Add(performance_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* hardware_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Hardware & Performance Profile",
		"Automatically configure or manually simulate memory and rendering budgets (e.g. to test low-end systems on high-end hardware)."
	);

	const auto& specs = HardwareProfileManager::get().getSpecs();
	const auto detected_tier = HardwareProfileManager::get().getDetectedTier();

	std::string detected_desc = std::format(
		"Detected: {} CPU threads | {:.1f} GB RAM | {} ({} MB Dedicated VRAM) | Recommended: {}",
		specs.cpu_cores,
		specs.total_ram_mb / 1024.0,
		specs.gpu_renderer.empty() ? "GPU" : specs.gpu_renderer,
		specs.dedicated_vram_mb,
		HardwareProfileManager::getTierName(detected_tier)
	);
	PreferencesLayout::AddNotice(
		hardware_section,
		detected_desc,
		Theme::Role::TextSubtle
	);

	hardware_profile_choice = new wxChoice(hardware_section, wxID_ANY);
	hardware_profile_choice->Append(wxString::Format("Auto-Detect (Recommended: %s)", wxString(HardwareProfileManager::getTierName(detected_tier).data())));
	hardware_profile_choice->Append("Low-End (Power Saver / 2 GB VRAM / 2 Threads)");
	hardware_profile_choice->Append("Medium (Balanced / 4-6 GB VRAM / 4 Threads)");
	hardware_profile_choice->Append("High (High-Performance / 8+ GB VRAM / Max Threads)");

	const int current_profile_mode = g_settings.getInteger(Config::HARDWARE_PROFILE_MODE);
	hardware_profile_choice->SetSelection(std::clamp(current_profile_mode, 0, 3));

	PreferencesLayout::AddControlRow(
		hardware_section,
		"Optimization Profile",
		"Select 'Low-End' to simulate 2 GB VRAM ceilings (~180 MB chunk VBOs, 2 threads). Takes effect immediately, trimming GPU memory live.",
		hardware_profile_choice
	);
	page_sizer->Add(hardware_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	FinishLayout();
}

void GraphicsPage::Apply() {
	bool must_restart = false;
	g_settings.setInteger(Config::USE_GUI_SELECTION_SHADOW, icon_selection_shadow_chkbox->GetValue());
	if (g_settings.getBoolean(Config::USE_MEMCACHED_SPRITES) != use_memcached_chkbox->GetValue()) {
		must_restart = true;
	}
	g_settings.setInteger(Config::USE_MEMCACHED_SPRITES_TO_SAVE, use_memcached_chkbox->GetValue());

	g_settings.setInteger(Config::ANTI_ALIASING, anti_aliasing_chkbox->GetValue());

	if (icon_background_choice->GetSelection() == 0) {
		if (g_settings.getInteger(Config::ICON_BACKGROUND) != 0) {
			g_gui.gfx.cleanSoftwareSprites();
		}
		g_settings.setInteger(Config::ICON_BACKGROUND, 0);
	} else if (icon_background_choice->GetSelection() == 1) {
		if (g_settings.getInteger(Config::ICON_BACKGROUND) != 88) {
			g_gui.gfx.cleanSoftwareSprites();
		}
		g_settings.setInteger(Config::ICON_BACKGROUND, 88);
	} else {
		if (g_settings.getInteger(Config::ICON_BACKGROUND) != 255) {
			g_gui.gfx.cleanSoftwareSprites();
		}
		g_settings.setInteger(Config::ICON_BACKGROUND, 255);
	}

	g_settings.setString(Config::SCREENSHOT_DIRECTORY, nstr(screenshot_directory_picker->GetPath()));

	const auto new_format = nstr(screenshot_format_choice->GetStringSelection());
	if (new_format == "PNG") {
		g_settings.setString(Config::SCREENSHOT_FORMAT, "png");
	} else if (new_format == "TGA") {
		g_settings.setString(Config::SCREENSHOT_FORMAT, "tga");
	} else if (new_format == "JPG") {
		g_settings.setString(Config::SCREENSHOT_FORMAT, "jpg");
	} else if (new_format == "BMP") {
		g_settings.setString(Config::SCREENSHOT_FORMAT, "bmp");
	}

	g_settings.setInteger(Config::ZONE_BORDERS_ENABLED, zone_borders_enabled_chkbox->GetValue());

	auto save_color_opacity_mode = [](
		wxColourPickerCtrl* picker,
		wxSpinCtrl* spin,
		wxChoice* choice,
		Config::Key r_key,
		Config::Key g_key,
		Config::Key b_key,
		Config::Key a_key,
		Config::Key mode_key
	) {
		auto c = picker->GetColour();
		int alpha = std::clamp(static_cast<int>(std::round((spin->GetValue() * 255.0f) / 100.0f)), 0, 255);
		g_settings.setInteger(r_key, c.Red());
		g_settings.setInteger(g_key, c.Green());
		g_settings.setInteger(b_key, c.Blue());
		g_settings.setInteger(a_key, alpha);
		g_settings.setInteger(mode_key, choice->GetSelection());
	};

	save_color_opacity_mode(cursor_color_pick, cursor_opacity_spin, cursor_blend_choice, Config::CURSOR_RED, Config::CURSOR_GREEN, Config::CURSOR_BLUE, Config::CURSOR_ALPHA, Config::CURSOR_BLEND_MODE);
	save_color_opacity_mode(cursor_alt_color_pick, cursor_alt_opacity_spin, cursor_alt_blend_choice, Config::CURSOR_ALT_RED, Config::CURSOR_ALT_GREEN, Config::CURSOR_ALT_BLUE, Config::CURSOR_ALT_ALPHA, Config::CURSOR_ALT_BLEND_MODE);
	save_color_opacity_mode(zone_border_color_pick, zone_border_opacity_spin, zone_border_blend_choice, Config::ZONE_BORDER_COLOR_R, Config::ZONE_BORDER_COLOR_G, Config::ZONE_BORDER_COLOR_B, Config::ZONE_BORDER_COLOR_A, Config::ZONE_BORDER_BLEND_MODE);

	save_color_opacity_mode(zone_pz_color_pick, zone_pz_opacity_spin, zone_pz_blend_choice, Config::ZONE_PZ_COLOR_R, Config::ZONE_PZ_COLOR_G, Config::ZONE_PZ_COLOR_B, Config::ZONE_PZ_COLOR_A, Config::ZONE_PZ_BLEND_MODE);
	save_color_opacity_mode(zone_nopvp_color_pick, zone_nopvp_opacity_spin, zone_nopvp_blend_choice, Config::ZONE_NOPVP_COLOR_R, Config::ZONE_NOPVP_COLOR_G, Config::ZONE_NOPVP_COLOR_B, Config::ZONE_NOPVP_COLOR_A, Config::ZONE_NOPVP_BLEND_MODE);
	save_color_opacity_mode(zone_nologout_color_pick, zone_nologout_opacity_spin, zone_nologout_blend_choice, Config::ZONE_NOLOGOUT_COLOR_R, Config::ZONE_NOLOGOUT_COLOR_G, Config::ZONE_NOLOGOUT_COLOR_B, Config::ZONE_NOLOGOUT_COLOR_A, Config::ZONE_NOLOGOUT_BLEND_MODE);
	save_color_opacity_mode(zone_pvp_color_pick, zone_pvp_opacity_spin, zone_pvp_blend_choice, Config::ZONE_PVP_COLOR_R, Config::ZONE_PVP_COLOR_G, Config::ZONE_PVP_COLOR_B, Config::ZONE_PVP_COLOR_A, Config::ZONE_PVP_BLEND_MODE);
	save_color_opacity_mode(zone_blocking_color_pick, zone_blocking_opacity_spin, zone_blocking_blend_choice, Config::ZONE_BLOCKING_COLOR_R, Config::ZONE_BLOCKING_COLOR_G, Config::ZONE_BLOCKING_COLOR_B, Config::ZONE_BLOCKING_COLOR_A, Config::ZONE_BLOCKING_BLEND_MODE);
	save_color_opacity_mode(zone_spawn_color_pick, zone_spawn_opacity_spin, zone_spawn_blend_choice, Config::ZONE_SPAWN_COLOR_R, Config::ZONE_SPAWN_COLOR_G, Config::ZONE_SPAWN_COLOR_B, Config::ZONE_SPAWN_COLOR_A, Config::ZONE_SPAWN_BLEND_MODE);
	save_color_opacity_mode(house_active_color_pick, house_active_opacity_spin, house_active_blend_choice, Config::HOUSE_ACTIVE_COLOR_R, Config::HOUSE_ACTIVE_COLOR_G, Config::HOUSE_ACTIVE_COLOR_B, Config::HOUSE_ACTIVE_COLOR_A, Config::HOUSE_ACTIVE_BLEND_MODE);
	save_color_opacity_mode(house_inactive_color_pick, house_inactive_opacity_spin, house_inactive_blend_choice, Config::HOUSE_INACTIVE_COLOR_R, Config::HOUSE_INACTIVE_COLOR_G, Config::HOUSE_INACTIVE_COLOR_B, Config::HOUSE_INACTIVE_COLOR_A, Config::HOUSE_INACTIVE_BLEND_MODE);

	g_gui.RefreshView();

	g_settings.setInteger(Config::HIDE_ITEMS_WHEN_ZOOMED, hide_items_when_zoomed_chkbox->GetValue());
	const auto requested_vsync_mode = sanitizeVSyncMode(vsync_choice->GetSelection());
	const auto previous_vsync_mode = sanitizeVSyncMode(g_settings.getInteger(Config::VSYNC_MODE));
	g_settings.setInteger(Config::VSYNC_MODE, static_cast<int>(requested_vsync_mode));
	g_settings.setInteger(Config::SHOW_FPS_COUNTER, show_fps_chkbox->GetValue());

	if (hardware_profile_choice) {
		const int chosen_profile_mode_int = hardware_profile_choice->GetSelection();
		const auto new_profile_mode = static_cast<HardwareProfileMode>(chosen_profile_mode_int);
		const auto old_profile_mode = static_cast<HardwareProfileMode>(g_settings.getInteger(Config::HARDWARE_PROFILE_MODE));

		if (new_profile_mode != old_profile_mode) {
			g_settings.setInteger(Config::HARDWARE_PROFILE_MODE, chosen_profile_mode_int);
			HardwareProfileManager::get().setProfileMode(new_profile_mode);
			spdlog::info("[GraphicsPage] Hardware optimization profile changed: {} -> {}",
				HardwareProfileManager::getModeName(old_profile_mode),
				HardwareProfileManager::getModeName(new_profile_mode));
		}
	}

	if (requested_vsync_mode != previous_vsync_mode) {
		const auto vsync_summary = g_gl_context.ReapplyVSyncToRegisteredCanvases();
		if (vsync_summary.adaptive_fallback) {
			wxMessageBox(
				"Adaptive VSync is not supported by the active OpenGL driver. Standard VSync has been enabled for this session instead.",
				"Adaptive VSync Unavailable",
				wxOK | wxICON_WARNING
			);
		} else if (vsync_summary.apply_failed) {
			wxMessageBox(
				"The selected VSync mode could not be applied on the active OpenGL canvases. Rendering will continue using the driver default.",
				"VSync Apply Failed",
				wxOK | wxICON_WARNING
			);
		}
	}

	if (must_restart) {
		wxMessageBox("Some changes require a restart of the application to take effect.", "Restart Required", wxOK | wxICON_INFORMATION);
	}
}
