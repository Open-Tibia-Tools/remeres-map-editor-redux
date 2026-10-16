#ifndef RME_UI_DIALOGS_MONSTER_SELECT_DIALOG_H_
#define RME_UI_DIALOGS_MONSTER_SELECT_DIALOG_H_

#include <wx/dialog.h>
#include <wx/srchctrl.h>
#include <wx/listbox.h>
#include <string>
#include <vector>

class MonsterSelectDialog : public wxDialog {
public:
	MonsterSelectDialog(wxWindow* parent, const std::string& current_selection = "");
	virtual ~MonsterSelectDialog() = default;

	std::string GetSelectedMonster() const {
		return selected_monster;
	}

private:
	void PopulateList(const std::string& filter);
	void OnSearchText(wxCommandEvent& event);
	void OnListDClick(wxCommandEvent& event);
	void OnOK(wxCommandEvent& event);

	wxSearchCtrl* search_ctrl;
	wxListBox* monster_list;
	std::vector<std::string> all_monsters;
	std::string selected_monster;
};

#endif // RME_UI_DIALOGS_MONSTER_SELECT_DIALOG_H_
