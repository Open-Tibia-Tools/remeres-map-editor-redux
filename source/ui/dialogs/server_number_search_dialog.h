//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_DIALOGS_SERVER_NUMBER_SEARCH_DIALOG_H_
#define RME_UI_DIALOGS_SERVER_NUMBER_SEARCH_DIALOG_H_

#include "app/main.h"
#include "editor/operations/search_query.h"

#include <string>

class wxChoice;
class wxSpinCtrl;

// Asks for a quest number, key number or door level to find on a sector map
// (Search > Find Quest, Key or Level...).
class ServerNumberSearchDialog : public wxDialog {
public:
	explicit ServerNumberSearchDialog(wxWindow* parent);

	[[nodiscard]] ServerNumberFilter getFilter() const;
	[[nodiscard]] std::string getTitle() const;

private:
	wxChoice* kind_choice = nullptr;
	wxSpinCtrl* value_spin = nullptr;
};

#endif
