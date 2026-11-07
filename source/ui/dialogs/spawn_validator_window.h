#ifndef RME_UI_DIALOGS_SPAWN_VALIDATOR_WINDOW_H_
#define RME_UI_DIALOGS_SPAWN_VALIDATOR_WINDOW_H_

#include "app/main.h"
#include "editor/operations/spawn_validator.h"
#include "util/nanovg_table_ctrl.h"
#include "palette/controls/palette_search_ctrl.h"

#include <wx/dialog.h>
#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <vector>
#include <optional>

class Editor;

class SpawnValidatorWindow : public wxDialog {
public:
	SpawnValidatorWindow(wxWindow* parent, Editor* editor);
	~SpawnValidatorWindow() override;

	void DetachEditor();

private:
	void InitLayout();
	void OnClose(wxCloseEvent& event);
	void OnCharHook(wxKeyEvent& event);

	// Actions
	void OnScanMap(wxCommandEvent& event);
	void OnJumpToSelected();
	void OnEditSelected();
	void OnRemoveSelected();
	void OnRemoveAll();
	void OnSearchFilterChanged(wxCommandEvent& event);
	void OnKindFilterChanged(wxCommandEvent& event);

	void RefreshTable();
	void UpdateButtonStates();
	void RescanMap();
	[[nodiscard]] std::vector<const EditorOperations::SpawnIssue*> GetFilteredIssues() const;

	Editor* editor = nullptr;
	std::vector<EditorOperations::SpawnIssue> current_issues;
	int selected_issue_id = -1;
	bool has_scanned = false;
	std::optional<EditorOperations::SpawnValidationSettings> last_settings;

	// UI Controls - Left Panel
	wxCheckBox* chk_invalid_time = nullptr;
	wxCheckBox* chk_unknown_creature = nullptr;
	wxCheckBox* chk_blocked_tile = nullptr;
	wxCheckBox* chk_empty_tile = nullptr;
	wxCheckBox* chk_unwalkable_zone = nullptr;
	wxRadioBox* radio_kind_filter = nullptr;
	PaletteSearchCtrl* search_ctrl = nullptr;

	wxButton* scan_button = nullptr;
	wxButton* jump_button = nullptr;
	wxButton* edit_button = nullptr;
	wxButton* remove_button = nullptr;
	wxButton* remove_all_button = nullptr;
	wxStaticText* status_label = nullptr;

	// UI Controls - Right Panel
	NanoVGTableCtrl* issues_table = nullptr;
};

#endif // RME_UI_DIALOGS_SPAWN_VALIDATOR_WINDOW_H_
