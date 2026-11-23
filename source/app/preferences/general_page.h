#ifndef RME_PREFERENCES_GENERAL_PAGE_H
#define RME_PREFERENCES_GENERAL_PAGE_H

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "preferences_page.h"

class GeneralPage : public ScrollablePreferencesPage {
public:
	explicit GeneralPage(wxWindow* parent);
	void Apply() override;

private:
	void UpdatePositionPreview();

	wxCheckBox* show_welcome_dialog_chkbox = nullptr;
	wxCheckBox* always_make_backup_chkbox = nullptr;
	wxCheckBox* update_check_on_startup_chkbox = nullptr;
	wxCheckBox* only_one_instance_chkbox = nullptr;
	wxCheckBox* enable_tileset_editing_chkbox = nullptr;
	wxCheckBox* show_load_warnings_dialog_chkbox = nullptr;

	wxSpinCtrl* undo_size_spin = nullptr;
	wxSpinCtrl* undo_mem_size_spin = nullptr;
	wxSpinCtrl* worker_threads_spin = nullptr;
	wxSpinCtrl* replace_size_spin = nullptr;

	wxChoice* position_format_choice = nullptr;
	wxStaticText* position_preview_label = nullptr;
};

#endif
