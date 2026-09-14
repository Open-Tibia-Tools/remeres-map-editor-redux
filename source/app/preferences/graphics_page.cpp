#include "app/preferences/graphics_page.h"

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

	auto* cursor_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Cursor",
		"Customize the map cursor colors used for drawing, houses, flags, and similar overlays."
	);
	cursor_color_pick = new wxColourPickerCtrl(
		cursor_section,
		wxID_ANY,
		wxColor(
			g_settings.getInteger(Config::CURSOR_RED),
			g_settings.getInteger(Config::CURSOR_GREEN),
			g_settings.getInteger(Config::CURSOR_BLUE),
			g_settings.getInteger(Config::CURSOR_ALPHA)
		)
	);
	PreferencesLayout::AddControlRow(
		cursor_section,
		"Primary cursor color",
		"Main drawing cursor color shown during regular placement and painting operations.",
		cursor_color_pick
	);
	cursor_alt_color_pick = new wxColourPickerCtrl(
		cursor_section,
		wxID_ANY,
		wxColor(
			g_settings.getInteger(Config::CURSOR_ALT_RED),
			g_settings.getInteger(Config::CURSOR_ALT_GREEN),
			g_settings.getInteger(Config::CURSOR_ALT_BLUE),
			g_settings.getInteger(Config::CURSOR_ALT_ALPHA)
		)
	);
	PreferencesLayout::AddControlRow(
		cursor_section,
		"Secondary cursor color",
		"Alternate cursor used for special overlays such as house and flag tools.",
		cursor_alt_color_pick
	);
	page_sizer->Add(cursor_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

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

	auto cursor_color = cursor_color_pick->GetColour();
	g_settings.setInteger(Config::CURSOR_RED, cursor_color.Red());
	g_settings.setInteger(Config::CURSOR_GREEN, cursor_color.Green());
	g_settings.setInteger(Config::CURSOR_BLUE, cursor_color.Blue());
	g_settings.setInteger(Config::CURSOR_ALPHA, cursor_color.Alpha());

	cursor_color = cursor_alt_color_pick->GetColour();
	g_settings.setInteger(Config::CURSOR_ALT_RED, cursor_color.Red());
	g_settings.setInteger(Config::CURSOR_ALT_GREEN, cursor_color.Green());
	g_settings.setInteger(Config::CURSOR_ALT_BLUE, cursor_color.Blue());
	g_settings.setInteger(Config::CURSOR_ALT_ALPHA, cursor_color.Alpha());

	g_settings.setInteger(Config::HIDE_ITEMS_WHEN_ZOOMED, hide_items_when_zoomed_chkbox->GetValue());
	const auto requested_vsync_mode = sanitizeVSyncMode(vsync_choice->GetSelection());
	const auto previous_vsync_mode = sanitizeVSyncMode(g_settings.getInteger(Config::VSYNC_MODE));
	g_settings.setInteger(Config::VSYNC_MODE, static_cast<int>(requested_vsync_mode));
	g_settings.setInteger(Config::SHOW_FPS_COUNTER, show_fps_chkbox->GetValue());

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
