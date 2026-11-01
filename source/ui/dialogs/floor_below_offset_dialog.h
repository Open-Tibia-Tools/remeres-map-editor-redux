#ifndef RME_FLOOR_BELOW_OFFSET_DIALOG_H
#define RME_FLOOR_BELOW_OFFSET_DIALOG_H

#include "app/main.h"

class wxSpinCtrl;

class FloorBelowOffsetDialog : public wxDialog {
public:
	explicit FloorBelowOffsetDialog(wxWindow* parent);
	~FloorBelowOffsetDialog() override = default;

private:
	void OnClickOK(wxCommandEvent& event);
	void OnClickCancel(wxCommandEvent& event);

	wxSpinCtrl* spin_x = nullptr;
	wxSpinCtrl* spin_y = nullptr;
};

#endif
