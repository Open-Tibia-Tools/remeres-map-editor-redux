#ifndef RME_PREFERENCES_EDITOR_PAGE_H
#define RME_PREFERENCES_EDITOR_PAGE_H

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "preferences_page.h"

class EditorPage : public ScrollablePreferencesPage {
public:
	explicit EditorPage(wxWindow* parent);
	void Apply() override;

private:
	wxChoice* floor_visibility_mode_choice = nullptr;
	wxSlider* ghost_alpha_slider = nullptr;
	wxStaticText* ghost_alpha_value_label = nullptr;
	wxCheckBox* group_actions_chkbox = nullptr;
	wxCheckBox* duplicate_id_warn_chkbox = nullptr;
	wxCheckBox* missing_items_warn_chkbox = nullptr;
	wxCheckBox* house_remove_chkbox = nullptr;
	wxCheckBox* auto_assign_doors_chkbox = nullptr;
	wxCheckBox* doodad_erase_same_chkbox = nullptr;
	wxCheckBox* eraser_leave_unique_chkbox = nullptr;
	wxCheckBox* auto_create_spawn_chkbox = nullptr;
	wxCheckBox* allow_multiple_orderitems_chkbox = nullptr;
	wxCheckBox* merge_move_chkbox = nullptr;
	wxCheckBox* merge_paste_chkbox = nullptr;
};

#endif
