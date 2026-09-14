#ifndef RME_PREFERENCES_GRAPHICS_PAGE_H
#define RME_PREFERENCES_GRAPHICS_PAGE_H

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
};

#endif
