//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_DIALOGS_REMOVE_UNREACHABLE_DIALOG_H_
#define RME_UI_DIALOGS_REMOVE_UNREACHABLE_DIALOG_H_

#include "app/main.h"
#include "editor/operations/unreachable_cleaner.h"
#include <wx/spinctrl.h>

class wxRadioButton;
class wxCheckBox;

class RemoveUnreachableDialog : public wxDialog {
public:
	explicit RemoveUnreachableDialog(wxWindow* parent);
	~RemoveUnreachableDialog() override = default;

	EditorOperations::UnreachableCleanerSettings GetSettings() const;

private:
	void OnViewportModeChanged(wxCommandEvent& event);
	void OnMultiFloorChanged(wxCommandEvent& event);
	void UpdateControlStates();

	wxRadioButton* radio_standard = nullptr;
	wxRadioButton* radio_custom = nullptr;
	wxSpinCtrl* custom_width_spin = nullptr;
	wxSpinCtrl* custom_height_spin = nullptr;
	wxCheckBox* multi_floor_checkbox = nullptr;
	wxRadioButton* radio_scope_all = nullptr;
	wxRadioButton* radio_scope_surface = nullptr;
	wxRadioButton* radio_scope_underground = nullptr;
	wxSpinCtrl* safety_margin_spin = nullptr;
	wxCheckBox* backup_checkbox = nullptr;
};

#endif // RME_UI_DIALOGS_REMOVE_UNREACHABLE_DIALOG_H_
