#ifndef RME_REPLACE_TOOL_WINDOW_H_
#define RME_REPLACE_TOOL_WINDOW_H_

#include "app/main.h"
#include "ui/replace_tool/library_panel.h"
#include "ui/replace_tool/rule_manager.h"
#include "ui/replace_tool/replacement_engine.h"
#include "ui/replace_tool/rule_list_control.h"
#include "ui/replace_tool/rule_builder_panel.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "ui/replace_tool/suggestion_filter_mode.h"
#include <wx/srchctrl.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <map>

class Editor;

class ReplaceToolWindow : public wxDialog, public LibraryPanel::Listener, public RuleListControl::Listener, public RuleBuilderPanel::Listener, public ItemGridPanel::Listener {
public:
	ReplaceToolWindow(wxWindow* parent, Editor* editor);
	virtual ~ReplaceToolWindow();

	void DetachEditor();

	void InitializeWithIDs(const std::vector<uint16_t>& ids);

	// LibraryPanel::Listener
	void OnLibraryItemSelected(uint16_t itemId) override;
	void OnAddItemsAsRules(const std::vector<uint16_t>& itemIds) override;
	void OnAutoMatchRules(Brush* brush) override;

	// RuleListControl::Listener
	virtual void OnRuleSelected(const RuleSet& rs) override;
	virtual void OnRuleDeleted(const std::string& name) override;
	virtual void OnRuleRenamed(const std::string& oldName, const std::string& newName) override;
	// RuleBuilderPanel::Listener
	virtual void OnRuleChanged() override;
	virtual void OnClearRules() override;
	virtual void OnSaveRule() override;
	void OnSaveAsNewRule();
	virtual void OnRuleItemSelected(uint16_t itemId) override;

	// ItemGridPanel::Listener
	virtual void OnItemSelected(ItemGridPanel* source, uint16_t itemId) override;

	void OnClose(wxCloseEvent& event);

private:
	void InitLayout();
	void UpdateSavedRulesList();

	// Event Handlers
	void OnExecute(wxCommandEvent& event);
	void OnAutoSave(wxTimerEvent& event);

	/** Saves the rule table as a new rule set and makes it the active one. */
	bool SaveRulesAsNewRuleSet(const wxString& name);
	/** Writes the table into the active rule set; asks for a name when none is active. */
	void SaveActiveRuleSet();
	/** Keeps the toolbar label, the saved rules list and the buttons in step. */
	void UpdateActiveRuleSetDisplay();
	void RefreshSuggestions();
	void OnCharHook(wxKeyEvent& event);
	void OnSuggestionNotebookPageChanged(wxNotebookEvent& event);

	uint16_t m_currentSuggestionItemId = 0;
	SuggestionFilterMode m_suggestionFilterMode = SuggestionFilterMode::None;

	Editor* editor;

	LibraryPanel* libraryPanel;
	wxNotebook* m_suggestionsNotebook = nullptr;
	ItemGridPanel* m_noFilterGrid = nullptr;
	ItemGridPanel* m_similarGrid = nullptr;
	ItemGridPanel* m_exactGrid = nullptr;
	TilesetGridToolbar* m_suggestionsToolbar = nullptr;
	RuleBuilderPanel* ruleBuilder;
	RuleListControl* savedRulesList;
	// wxButton* m_saveBtn; // Removed
	wxButton* m_executeBtn;
	TilesetGridToolbar* m_ruleViewToolbar = nullptr;
	wxButton* m_ruleUndoBtn = nullptr;
	wxButton* m_ruleRedoBtn = nullptr;
	wxButton* m_ruleSaveAsBtn = nullptr;
	wxButton* m_ruleClearBtn = nullptr;
	wxButton* m_saveActiveRuleBtn = nullptr;
	wxButton* m_addRuleBtn;
	wxButton* m_editRuleBtn;
	wxButton* m_deleteRuleBtn;
	wxChoice* m_scopeChoice;

	ReplacementEngine engine;

	std::string m_activeRuleSetName;
	/** Edits made since the active set was saved or loaded. */
	bool m_rulesDirty = false;
	/** True once the table has been written to the active set since it changed. */
	bool m_rulesSaved = false;
	wxTimer m_autoSaveTimer;
};

#endif
