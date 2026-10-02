#ifndef RME_PREFERENCES_GRAPHICS_PAGE_H
#define RME_PREFERENCES_GRAPHICS_PAGE_H

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>

#include "preferences_page.h"

class GraphicsPage : public ScrollablePreferencesPage {
public:
	explicit GraphicsPage(wxWindow* parent);
	void Apply() override;

private:
	wxCheckBox* hide_items_when_zoomed_chkbox = nullptr;
	wxCheckBox* icon_selection_shadow_chkbox = nullptr;
	wxCheckBox* use_memcached_chkbox = nullptr;
	wxCheckBox* anti_aliasing_chkbox = nullptr;

	wxChoice* icon_background_choice = nullptr;
	wxChoice* screenshot_format_choice = nullptr;

	wxColourPickerCtrl* cursor_color_pick = nullptr;
	wxColourPickerCtrl* cursor_alt_color_pick = nullptr;

	wxDirPickerCtrl* screenshot_directory_picker = nullptr;

	wxChoice* vsync_choice = nullptr;
	wxCheckBox* show_fps_chkbox = nullptr;

	wxChoice* hardware_profile_choice = nullptr;

	// Zone & Overlay Appearance
	wxCheckBox* zone_borders_enabled_chkbox = nullptr;
	wxColourPickerCtrl* zone_border_color_pick = nullptr;

	wxColourPickerCtrl* zone_pz_color_pick = nullptr;
	wxSpinCtrl* zone_pz_opacity_spin = nullptr;

	wxColourPickerCtrl* zone_nopvp_color_pick = nullptr;
	wxSpinCtrl* zone_nopvp_opacity_spin = nullptr;

	wxColourPickerCtrl* zone_nologout_color_pick = nullptr;
	wxSpinCtrl* zone_nologout_opacity_spin = nullptr;

	wxColourPickerCtrl* zone_pvp_color_pick = nullptr;
	wxSpinCtrl* zone_pvp_opacity_spin = nullptr;

	wxColourPickerCtrl* zone_blocking_color_pick = nullptr;
	wxSpinCtrl* zone_blocking_opacity_spin = nullptr;

	wxColourPickerCtrl* zone_spawn_color_pick = nullptr;
	wxSpinCtrl* zone_spawn_opacity_spin = nullptr;

	wxColourPickerCtrl* house_active_color_pick = nullptr;
	wxSpinCtrl* house_active_opacity_spin = nullptr;

	wxColourPickerCtrl* house_inactive_color_pick = nullptr;
	wxSpinCtrl* house_inactive_opacity_spin = nullptr;

	wxButton* reset_zone_defaults_btn = nullptr;
};

#endif
