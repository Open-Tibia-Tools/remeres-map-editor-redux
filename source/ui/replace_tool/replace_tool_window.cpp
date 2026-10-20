#include "app/main.h"
#include "replace_tool_window.h"
#include "visual_similarity_service.h"
#include "ui/theme.h"
#include "editor/editor.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "app/managers/version_manager.h"
#include <algorithm> // For std::find
#include "ui/map_window.h"
#include "rendering/ui/map_display.h"
#include "rendering/core/floor_visibility_mode.h"
#include "util/image_manager.h"
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/srchctrl.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/artprov.h>

// Brush includes
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/auto_border.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/table/table_brush_items.h"
#include "brushes/carpet/carpet_brush_items.h"
#include "brushes/door/door_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "ui/replace_tool/card_panel.h"
#include "util/image_manager.h"

#include "ui/replace_tool/map_item_collector.h"

namespace {

// A compact icon-only button; the meaning lives in its tooltip.
wxButton* MakeIconButton(wxWindow* parent, std::string_view icon, const wxString& tooltip, const wxSize& size) {
	auto* button = new wxButton(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size, wxBU_NOTEXT | wxBU_EXACTFIT);
	button->SetBitmap(IMAGE_MANAGER.GetBitmap(icon, wxWindow::FromDIP(wxSize(16, 16), parent), Theme::Get(Theme::Role::Text)));
	button->SetToolTip(tooltip);
	return button;
}

std::vector<Position> collectViewportPositions() {
	MapTab* activeTab = g_gui.GetCurrentMapTab();
	return activeTab ? rme::replace_tool::MapItemCollector::CollectViewportPositions(activeTab->GetCanvas()) : std::vector<Position>{};
}

} // namespace

ReplaceToolWindow::ReplaceToolWindow(wxWindow* parent, Editor* editor) : wxDialog(parent, wxID_ANY, "Advanced Replace Tool", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
																		 editor(editor) {

	VisualSimilarityService::Get().StartIndexing();
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	InitLayout();

	UpdateSavedRulesList();

	Bind(wxEVT_CLOSE_WINDOW, &ReplaceToolWindow::OnClose, this);
	Bind(wxEVT_CHAR_HOOK, &ReplaceToolWindow::OnCharHook, this);

	// Restore window size (Physical pixels)
	int w = g_settings.getInteger(Config::REPLACE_TOOL_WINDOW_WIDTH);
	int h = g_settings.getInteger(Config::REPLACE_TOOL_WINDOW_HEIGHT);

	if (w < 600 || h < 400) {
		// Default size (Logical -> Physical)
		SetSize(FromDIP(wxSize(1120, 700)));
	} else {
		// Restored size (Already Physical)
		SetSize(wxSize(w, h));
	}
}

void ReplaceToolWindow::DetachEditor() {
	if (libraryPanel) {
		libraryPanel->DetachEditor();
	}
	editor = nullptr;
}

ReplaceToolWindow::~ReplaceToolWindow() {
	DetachEditor();
	// Unbind to be safe, though destructor usually handles cleanup
	Unbind(wxEVT_CLOSE_WINDOW, &ReplaceToolWindow::OnClose, this);
}

void ReplaceToolWindow::OnClose(wxCloseEvent& event) {
	if (!IsMaximized()) {
		// GetSize returns Physical pixels on Windows
		wxSize size = GetSize();
		g_settings.setInteger(Config::REPLACE_TOOL_WINDOW_WIDTH, size.GetWidth());
		g_settings.setInteger(Config::REPLACE_TOOL_WINDOW_HEIGHT, size.GetHeight());
		g_settings.save();
	}

	// Nothing is written automatically, so unsaved edits have to be mentioned
	// before the window goes away - and the user gets to keep it open.
	if (m_rulesDirty && event.CanVeto()) {
		wxString question = m_activeRuleSetName.empty()
			? wxString("The rules in the table were never saved to a rule set.\n\nSave them now?")
			: wxString::Format("Rule set \"%s\" has unsaved changes.\n\nSave before closing?", wxstr(m_activeRuleSetName));

		const int answer = wxMessageBox(question, "Unsaved rules", wxYES_NO | wxCANCEL | wxICON_WARNING, this);
		if (answer == wxCANCEL) {
			event.Veto();
			return;
		}
		if (answer == wxYES) {
			SaveActiveRuleSet();
			if (m_rulesDirty) {
				// The save was cancelled or failed; keep the window open.
				event.Veto();
				return;
			}
		}
	}

	// The owner holds a pointer to this dialog; let go of it before wx destroys
	// the window, so nobody destroys it twice.
	if (auto* owner = wxDynamicCast(GetParent(), MapWindow); owner != nullptr) {
		owner->ForgetReplaceItemsDialog();
	}

	Destroy();
}

void ReplaceToolWindow::InitializeWithIDs(const std::vector<uint16_t>& ids) {
	if (ids.empty()) {
		return;
	}

	// IDs are already sorted from std::set in MapMenuHandler, but sorting here ensures consistency
	std::vector<uint16_t> sorted(ids);
	std::sort(sorted.begin(), sorted.end());
	sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

	std::vector<RuleGroup> rules;
	rules.reserve(sorted.size());
	for (const uint16_t id : sorted) {
		if (id == 0) {
			continue;
		}
		RuleGroup rule;
		rule.sources.push_back(id);
		rules.push_back(std::move(rule));
	}

	ruleBuilder->SetRules(rules);
	m_activeRuleSetName = "";
}

void ReplaceToolWindow::InitLayout() {
	wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);
	int padding = Theme::Grid(2);

	// ---------------------------------------------------------
	// MAIN CONTENT ROW
	// ---------------------------------------------------------
	wxBoxSizer* mainRowSizer = new wxBoxSizer(wxHORIZONTAL);

	// ---------------------------------------------------------
	// COLUMN 1: Library Panel (Items & Brushes)
	// ---------------------------------------------------------
	CardPanel* col1Card = new CardPanel(this, wxID_ANY);
	col1Card->SetTitle("ITEM LIBRARY");

	libraryPanel = new LibraryPanel(col1Card, this, editor);
	col1Card->GetContentSizer()->Add(libraryPanel, wxSizerFlags(1).Expand().Border(wxALL, padding / 2));
	mainRowSizer->Add(col1Card, wxSizerFlags(3).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 3

	// ---------------------------------------------------------
	// COLUMN 2: Rule Builder
	// ---------------------------------------------------------
	CardPanel* col2Card = new CardPanel(this, wxID_ANY);
	col2Card->SetTitle("RULE BUILDER");

	// Plain wxButtons, matching the rest of the dialog: the rule table used to
	// paint its own save/clear buttons, which could not follow the theme and
	// whose save button was never wired to anything.
	ruleBuilder = new RuleBuilderPanel(col2Card, this);

	auto* ruleToolbar = new wxPanel(col2Card);
	auto* ruleToolbarSizer = new wxBoxSizer(wxHORIZONTAL);

	// The rule table's view options are the same icon toolbar the grids use, so
	// the labels and size controls look and behave identically everywhere.
	TilesetGridToolbar::Actions ruleViewActions;
	ruleViewActions.show_labels = [this](bool show) {
		ruleBuilder->SetShowLabels(show);
	};
	ruleViewActions.icon_size = [this](int size) {
		ruleBuilder->SetIconSize(size);
	};
	m_ruleViewToolbar = new TilesetGridToolbar(ruleToolbar, std::move(ruleViewActions), TilesetGridToolbar::Part_Labels | TilesetGridToolbar::Part_Size);
	m_ruleViewToolbar->SetState(ruleBuilder->GetShowLabels(), ruleBuilder->GetIconSize(), false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);
	ruleToolbarSizer->Add(m_ruleViewToolbar, 1, wxALIGN_CENTER_VERTICAL);

	// Icon buttons for the table's own actions, with the label in the tooltip.
	const wxSize iconButton = FromDIP(wxSize(34, 30));

	m_ruleUndoBtn = MakeIconButton(ruleToolbar, ICON_UNDO, "Undo (Ctrl+Z)", iconButton);
	m_ruleUndoBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ruleBuilder->Undo(); });
	m_ruleUndoBtn->Enable(false);
	ruleToolbarSizer->Add(m_ruleUndoBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, Theme::Grid(1));

	m_ruleRedoBtn = MakeIconButton(ruleToolbar, ICON_REDO, "Redo (Ctrl+Y)", iconButton);
	m_ruleRedoBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ruleBuilder->Redo(); });
	m_ruleRedoBtn->Enable(false);
	ruleToolbarSizer->Add(m_ruleRedoBtn, 0, wxALIGN_CENTER_VERTICAL);

	m_ruleSaveAsBtn = MakeIconButton(ruleToolbar, ICON_SAVE, "Save as new rule set (Ctrl+Shift+S)", iconButton);
	m_ruleSaveAsBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnSaveAsNewRule(); });
	ruleToolbarSizer->Add(m_ruleSaveAsBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, Theme::Grid(1));

	m_ruleClearBtn = MakeIconButton(ruleToolbar, ICON_BROOM, "Clear every rule", iconButton);
	m_ruleClearBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (ruleBuilder->GetRules().empty()) {
			return;
		}
		if (wxMessageBox("Clear every rule in the table?", "Clear rules", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) == wxYES) {
			ruleBuilder->Clear();
		}
	});
	ruleToolbarSizer->Add(m_ruleClearBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, Theme::Grid(2));

	ruleToolbar->SetSizer(ruleToolbarSizer);
	col2Card->GetContentSizer()->Add(ruleToolbar, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxTOP, padding));

	col2Card->GetContentSizer()->Add(ruleBuilder, wxSizerFlags(1).Expand().Border(wxALL, padding));
	col2Card->SetShowFooter(false); // No footer anymore

	mainRowSizer->Add(col2Card, wxSizerFlags(4).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 4

	// ---------------------------------------------------------
	// COLUMN 3: Similarity Engine
	// ---------------------------------------------------------
	CardPanel* col3Card = new CardPanel(this, wxID_ANY);
	col3Card->SetTitle("SMART SUGGESTIONS");

	m_suggestionsNotebook = new wxNotebook(col3Card, wxID_ANY);

	m_noFilterGrid = new ItemGridPanel(m_suggestionsNotebook, this);
	m_noFilterGrid->SetDraggable(true);
	m_noFilterGrid->ClearSort();

	m_similarGrid = new ItemGridPanel(m_suggestionsNotebook, this);
	m_similarGrid->SetDraggable(true);
	m_similarGrid->ClearSort();

	m_exactGrid = new ItemGridPanel(m_suggestionsNotebook, this);
	m_exactGrid->SetDraggable(true);
	m_exactGrid->ClearSort();

	m_suggestionsNotebook->AddPage(m_noFilterGrid, "No filter", true);
	m_suggestionsNotebook->AddPage(m_similarGrid, "Similar properties", false);
	m_suggestionsNotebook->AddPage(m_exactGrid, "Exact properties", false);
	m_suggestionsNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ReplaceToolWindow::OnSuggestionNotebookPageChanged, this);

	TilesetGridToolbar::Actions suggestionActions;
	suggestionActions.filter = [this](const std::string& query) {
		wxString q = wxstr(query);
		if (m_noFilterGrid) m_noFilterGrid->SetFilter(q);
		if (m_similarGrid) m_similarGrid->SetFilter(q);
		if (m_exactGrid) m_exactGrid->SetFilter(q);
	};
	suggestionActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		wxString q = wxstr(m_suggestionsToolbar ? m_suggestionsToolbar->GetFilterQuery() : "");
		if (m_noFilterGrid) { m_noFilterGrid->SetSort(key, dir); m_noFilterGrid->SetFilter(q); }
		if (m_similarGrid) { m_similarGrid->SetSort(key, dir); m_similarGrid->SetFilter(q); }
		if (m_exactGrid) { m_exactGrid->SetSort(key, dir); m_exactGrid->SetFilter(q); }
	};
	suggestionActions.clear_sort = [this]() {
		wxString q = wxstr(m_suggestionsToolbar ? m_suggestionsToolbar->GetFilterQuery() : "");
		if (m_noFilterGrid) { m_noFilterGrid->ClearSort(); m_noFilterGrid->SetFilter(q); }
		if (m_similarGrid) { m_similarGrid->ClearSort(); m_similarGrid->SetFilter(q); }
		if (m_exactGrid) { m_exactGrid->ClearSort(); m_exactGrid->SetFilter(q); }
	};
	suggestionActions.show_labels = [this](bool show) {
		if (m_noFilterGrid) m_noFilterGrid->SetShowLabels(show);
		if (m_similarGrid) m_similarGrid->SetShowLabels(show);
		if (m_exactGrid) m_exactGrid->SetShowLabels(show);
	};
	suggestionActions.icon_size = [this](int size) {
		if (m_noFilterGrid) m_noFilterGrid->SetIconSize(size);
		if (m_similarGrid) m_similarGrid->SetIconSize(size);
		if (m_exactGrid) m_exactGrid->SetIconSize(size);
	};

	m_suggestionsToolbar = new TilesetGridToolbar(col3Card, std::move(suggestionActions), TilesetGridToolbar::Part_Standard);
	m_suggestionsToolbar->SetState(m_noFilterGrid->GetShowLabels(), m_noFilterGrid->GetIconSize(), m_noFilterGrid->HasSort(), m_noFilterGrid->GetSortKey(), m_noFilterGrid->GetSortDirection(), false);

	col3Card->GetContentSizer()->Add(m_suggestionsToolbar, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxTOP, padding));
	col3Card->GetContentSizer()->Add(m_suggestionsNotebook, wxSizerFlags(1).Expand().Border(wxALL, padding));
	mainRowSizer->Add(col3Card, wxSizerFlags(3).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 3

	// ---------------------------------------------------------
	// COLUMN 4: Saved Rules
	// ---------------------------------------------------------
	CardPanel* savedRulesCard = new CardPanel(this, wxID_ANY);
	savedRulesCard->SetTitle("SAVED RULES");
	savedRulesCard->SetShowFooter(false);

	// Managing rule sets belongs next to the list, not in the footer.
	auto* savedRulesToolbar = new wxPanel(savedRulesCard);
	auto* savedRulesToolbarSizer = new wxBoxSizer(wxHORIZONTAL);
	const wxSize savedIconButton = FromDIP(wxSize(30, 26));

	m_saveActiveRuleBtn = MakeIconButton(savedRulesToolbar, ICON_SAVE, "Save rule set (Ctrl+S)", savedIconButton);
	m_saveActiveRuleBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveActiveRuleSet(); });
	savedRulesToolbarSizer->Add(m_saveActiveRuleBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_addRuleBtn = MakeIconButton(savedRulesToolbar, ICON_PLUS, "Save the current rules as a new rule set", savedIconButton);
	savedRulesToolbarSizer->Add(m_addRuleBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_editRuleBtn = MakeIconButton(savedRulesToolbar, ICON_PEN_TO_SQUARE, "Rename the selected rule set", savedIconButton);
	savedRulesToolbarSizer->Add(m_editRuleBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_deleteRuleBtn = MakeIconButton(savedRulesToolbar, ICON_TRASH_CAN, "Delete the selected rule set", savedIconButton);
	savedRulesToolbarSizer->Add(m_deleteRuleBtn, 0, wxALIGN_CENTER_VERTICAL);

	savedRulesToolbar->SetSizer(savedRulesToolbarSizer);
	savedRulesCard->GetContentSizer()->Add(savedRulesToolbar, wxSizerFlags(0).Border(wxLEFT | wxRIGHT | wxTOP, padding));

	savedRulesList = new RuleListControl(savedRulesCard, this);
	savedRulesCard->GetContentSizer()->Add(savedRulesList, wxSizerFlags(1).Expand().Border(wxALL, padding));

	mainRowSizer->Add(savedRulesCard, wxSizerFlags(2).Expand().Border(wxALL, padding / 2)); // Flex 2

	// ---------------------------------------------------------
	// FOOTER: scope and execution, flush right
	// ---------------------------------------------------------
	wxBoxSizer* footerSizer = newd wxBoxSizer(wxHORIZONTAL);

	footerSizer->AddStretchSpacer(1);

	wxStaticText* scopeLabel = newd wxStaticText(this, wxID_ANY, "SCOPE:");
	scopeLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	scopeLabel->SetFont(Theme::GetFont(9, true));
	footerSizer->Add(scopeLabel, wxSizerFlags(0).Center().Border(wxRIGHT, 5));

	wxArrayString scopes;
	scopes.Add("SELECTION");
	scopes.Add("VIEWPORT");
	scopes.Add("ALL MAP");
	m_scopeChoice = newd wxChoice(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(140, 24)), scopes);
	m_scopeChoice->SetSelection(0);
	if (editor && editor->selection.empty()) {
		m_scopeChoice->SetSelection(1); // Default to Viewport if no selection
	}
	footerSizer->Add(m_scopeChoice, wxSizerFlags(0).Center().Border(wxRIGHT, padding));

	m_executeBtn = newd wxButton(this, wxID_ANY, "EXECUTE REPLACE", wxDefaultPosition, FromDIP(wxSize(-1, 32)));
	m_executeBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLAY, wxSize(16, 16)));
	m_executeBtn->SetDefault();
	m_executeBtn->SetBackgroundColour(Theme::Get(Theme::Role::Accent)); // Blue
	m_executeBtn->SetForegroundColour(*wxWHITE);
	m_executeBtn->SetFont(Theme::GetFont(10, true));
	footerSizer->Add(m_executeBtn, wxSizerFlags(0).Center().Border(wxLEFT, padding));

	wxButton* closeBtn = newd wxButton(this, wxID_ANY, "CLOSE", wxDefaultPosition, FromDIP(wxSize(-1, 32)));
	closeBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	closeBtn->SetBackgroundColour(wxColour(120, 120, 120)); // Gray
	closeBtn->SetForegroundColour(*wxWHITE);
	closeBtn->SetFont(Theme::GetFont(10, true));
	footerSizer->Add(closeBtn, wxSizerFlags(0).Center().Border(wxLEFT, padding));

	// ---------------------------------------------------------
	// BINDINGS
	// ---------------------------------------------------------
	m_executeBtn->Bind(wxEVT_BUTTON, &ReplaceToolWindow::OnExecute, this);
	closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });

	m_addRuleBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		OnSaveAsNewRule();
	});

	m_editRuleBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (savedRulesList->GetSelectedIndex() != -1) {
			std::string oldName = savedRulesList->GetRuleSetNames()[savedRulesList->GetSelectedIndex()];
			wxString newName = wxGetTextFromUser("Enter new name for rule set:", "Edit Name", oldName);
			if (!newName.IsEmpty() && newName.ToStdString() != oldName) {
				OnRuleRenamed(oldName, newName.ToStdString());
			}
		}
	});

	m_deleteRuleBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (savedRulesList->GetSelectedIndex() != -1) {
			std::string name = savedRulesList->GetRuleSetNames()[savedRulesList->GetSelectedIndex()];
			wxMessageDialog dlg(this, "Are you sure you want to delete rule set '" + name + "'?", "Delete Rule Set", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
			if (dlg.ShowModal() == wxID_YES) {
				OnRuleDeleted(name);
			}
		}
	});

	// Add Main Row and footer to Root
	rootSizer->Add(mainRowSizer, wxSizerFlags(1).Expand().Border(wxALL, padding / 2));
	rootSizer->Add(footerSizer, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, padding));

	SetSizer(rootSizer);
	Layout();
}

void ReplaceToolWindow::OnLibraryItemSelected(uint16_t itemId) {
	m_currentSuggestionItemId = itemId;
	RefreshSuggestions();
}

void ReplaceToolWindow::OnSuggestionNotebookPageChanged(wxNotebookEvent& event) {
	int sel = event.GetSelection();
	if (sel == 0) {
		m_suggestionFilterMode = SuggestionFilterMode::None;
	} else if (sel == 1) {
		m_suggestionFilterMode = SuggestionFilterMode::Similar;
	} else if (sel == 2) {
		m_suggestionFilterMode = SuggestionFilterMode::Exact;
	}
	event.Skip();
}

void ReplaceToolWindow::RefreshSuggestions() {
	if (m_currentSuggestionItemId == 0 || m_currentSuggestionItemId == TRASH_ITEM_ID) {
		if (m_noFilterGrid) {
			m_noFilterGrid->SetItems({});
		}
		if (m_similarGrid) {
			m_similarGrid->SetItems({});
		}
		if (m_exactGrid) {
			m_exactGrid->SetItems({});
		}
		return;
	}

	if (m_noFilterGrid) {
		m_noFilterGrid->SetItems(VisualSimilarityService::Get().FindSimilar(m_currentSuggestionItemId, 300, SuggestionFilterMode::None));
	}
	if (m_similarGrid) {
		m_similarGrid->SetItems(VisualSimilarityService::Get().FindSimilar(m_currentSuggestionItemId, 300, SuggestionFilterMode::Similar));
	}
	if (m_exactGrid) {
		m_exactGrid->SetItems(VisualSimilarityService::Get().FindSimilar(m_currentSuggestionItemId, 300, SuggestionFilterMode::Exact));
	}

	if (m_suggestionsToolbar) {
		wxString q = wxstr(m_suggestionsToolbar->GetFilterQuery());
		if (m_noFilterGrid) {
			m_noFilterGrid->SetFilter(q);
		}
		if (m_similarGrid) {
			m_similarGrid->SetFilter(q);
		}
		if (m_exactGrid) {
			m_exactGrid->SetFilter(q);
		}
	}
}

void ReplaceToolWindow::OnRuleSelected(const RuleSet& rs) {
	m_activeRuleSetName = rs.name;
	ruleBuilder->SetRules(rs.rules);
	// The table now matches the file it came from.
	m_rulesDirty = false;
	UpdateActiveRuleSetDisplay();

	const bool hasSource = !rs.rules.empty() && !rs.rules.front().sources.empty();
	if (hasSource) {
		m_currentSuggestionItemId = rs.rules.front().sources.front();
		RefreshSuggestions();
	} else {
		m_currentSuggestionItemId = 0;
		if (m_noFilterGrid) {
			m_noFilterGrid->SetItems({});
		}
		if (m_similarGrid) {
			m_similarGrid->SetItems({});
		}
		if (m_exactGrid) {
			m_exactGrid->SetItems({});
		}
	}
}

void ReplaceToolWindow::OnRuleDeleted(const std::string& name) {
	if (RuleManager::Get().DeleteRuleSet(name)) {
		// Otherwise a later save would write the deleted set straight back.
		if (m_activeRuleSetName == name) {
			m_activeRuleSetName.clear();
			m_rulesDirty = true;
		}
		UpdateSavedRulesList();
	}
}

void ReplaceToolWindow::OnRuleRenamed(const std::string& oldName, const std::string& newName) {
	std::vector<std::string> existing = RuleManager::Get().GetAvailableRuleSets();
	if (std::find(existing.begin(), existing.end(), newName) != existing.end()) {
		wxMessageBox("A rule set with this name already exists.", "Rename Error", wxOK | wxICON_ERROR);
		return;
	}

	RuleSet rs = RuleManager::Get().LoadRuleSet(oldName);
	if (!rs.name.empty()) {
		rs.name = newName;
		if (RuleManager::Get().SaveRuleSet(rs)) {
			RuleManager::Get().DeleteRuleSet(oldName);

			if (m_activeRuleSetName == oldName) {
				m_activeRuleSetName = newName;
			}
			UpdateSavedRulesList();
		} else {
			wxMessageBox("Failed to save the renamed rule set.", "Rename Error", wxOK | wxICON_ERROR);
		}
	}
}

void ReplaceToolWindow::UpdateActiveRuleSetDisplay() {
	// The saved rules list is where the state is shown: colour coded per set.
	if (savedRulesList) {
		savedRulesList->SetActiveRuleSet(m_activeRuleSetName, m_rulesDirty, m_rulesSaved);
	}
	if (m_ruleUndoBtn) {
		m_ruleUndoBtn->Enable(ruleBuilder->CanUndo());
	}
	if (m_ruleRedoBtn) {
		m_ruleRedoBtn->Enable(ruleBuilder->CanRedo());
	}
	if (m_ruleSaveAsBtn) {
		m_ruleSaveAsBtn->Enable(!ruleBuilder->GetRules().empty());
	}
	if (m_saveActiveRuleBtn) {
		m_saveActiveRuleBtn->Enable(!ruleBuilder->GetRules().empty());
	}
}

void ReplaceToolWindow::UpdateSavedRulesList() {
	savedRulesList->SetRuleSets(RuleManager::Get().GetAvailableRuleSets());
	UpdateActiveRuleSetDisplay();
}

void ReplaceToolWindow::SaveActiveRuleSet() {
	const std::vector<RuleGroup> rules = ruleBuilder->GetRules();
	if (rules.empty()) {
		wxMessageBox("There is nothing to save yet - build a rule first.", "Save rule set", wxOK | wxICON_INFORMATION, this);
		return;
	}

	// Without an active set there is nothing to write into, so ask for a name.
	if (m_activeRuleSetName.empty()) {
		const wxString name = wxGetTextFromUser("Name for the new rule set:", "Save as new rule set", "", this);
		SaveRulesAsNewRuleSet(name);
		return;
	}

	RuleSet ruleSet;
	ruleSet.name = m_activeRuleSetName;
	ruleSet.rules = rules;
	if (!RuleManager::Get().SaveRuleSet(ruleSet)) {
		wxMessageBox("Could not write the rule set to disk.", "Save rule set", wxOK | wxICON_ERROR, this);
		return;
	}

	m_rulesDirty = false;
	m_rulesSaved = true;
	UpdateSavedRulesList();
}

bool ReplaceToolWindow::SaveRulesAsNewRuleSet(const wxString& name) {
	const std::vector<RuleGroup> rules = ruleBuilder->GetRules();
	if (rules.empty() || name.IsEmpty()) {
		return false;
	}

	if (name.ToStdString().size() > 64) {
		wxMessageBox("That name is too long. Use up to 64 letters, digits, spaces, '-' or '_'.", "Save rule set", wxOK | wxICON_ERROR, this);
		return false;
	}

	const std::vector<std::string> existing = RuleManager::Get().GetAvailableRuleSets();
	if (std::find(existing.begin(), existing.end(), name.ToStdString()) != existing.end()) {
		wxMessageBox("A rule set with this name already exists. Please choose a different name.", "Name Collision", wxOK | wxICON_ERROR, this);
		return false;
	}

	RuleSet ruleSet;
	ruleSet.name = name.ToStdString();
	ruleSet.rules = rules;

	if (!RuleManager::Get().SaveRuleSet(ruleSet)) {
		wxMessageBox("Could not save the rule set. Use letters, digits, spaces, '-' or '_'.", "Save rule set", wxOK | wxICON_ERROR, this);
		return false;
	}

	// The new set is what the table is editing from now on, otherwise a later
	// save would be written back into the previous set.
	m_activeRuleSetName = ruleSet.name;
	m_rulesDirty = false;
	m_rulesSaved = true;
	UpdateSavedRulesList();
	return true;
}

void ReplaceToolWindow::OnRuleChanged() {
	// Nothing is written to disk here: edits are the user's until they save.
	m_rulesDirty = true;
	m_rulesSaved = false;
	UpdateActiveRuleSetDisplay();
}

void ReplaceToolWindow::OnCharHook(wxKeyEvent& event) {
	if (event.ControlDown()) {
		// Let the search fields keep their own Ctrl+Z.
		if (wxDynamicCast(FindFocus(), wxTextCtrl) != nullptr || wxDynamicCast(FindFocus(), wxSearchCtrl) != nullptr) {
			event.Skip();
			return;
		}

		switch (event.GetKeyCode()) {
			case 'S':
				if (event.ShiftDown()) {
					OnSaveAsNewRule();
				} else {
					SaveActiveRuleSet();
				}
				return;
			case 'Z':
				if (event.ShiftDown()) {
					ruleBuilder->Redo();
				} else {
					ruleBuilder->Undo();
				}
				return;
			case 'Y':
				ruleBuilder->Redo();
				return;
			default:
				break;
		}
	}
	event.Skip();
}

void ReplaceToolWindow::OnExecute(wxCommandEvent&) {
	const std::vector<RuleGroup> groups = ruleBuilder->GetRules();
	if (groups.empty()) {
		wxMessageBox("No rules defined. Add some rules first.", "Replace", wxOK | wxICON_INFORMATION);
		return;
	}

	std::vector<size_t> ruleRows;
	const std::vector<ReplacementRule> rules = ExpandRuleGroups(groups, ruleRows);
	if (rules.empty()) {
		wxMessageBox("No rule is complete yet.\n\nA rule needs at least one source, and either a target or the delete mode.", "Replace", wxOK | wxICON_INFORMATION);
		return;
	}

	int scopeIdx = m_scopeChoice->GetSelection();
	ReplaceScope scope = ReplaceScope::Selection;
	if (scopeIdx == 1) {
		scope = ReplaceScope::Viewport;
	} else if (scopeIdx == 2) {
		scope = ReplaceScope::AllMap;
	}

	if (scope == ReplaceScope::Selection && editor->selection.empty()) {
		wxMessageBox("No selection active. Please select an area first or change scope.", "Replace", wxOK | wxICON_WARNING);
		return;
	}

	// Preview: count what each rule would touch before asking for confirmation.
	std::vector<Position> viewportPos;
	if (scope == ReplaceScope::Viewport) {
		viewportPos = collectViewportPositions();
	}

	const std::vector<uint32_t> counts = engine.CountMatchesByRule(editor, rules, scope, scope == ReplaceScope::Viewport ? &viewportPos : nullptr);

	std::vector<uint32_t> perRow(groups.size(), 0);
	for (size_t index = 0; index < counts.size() && index < ruleRows.size(); ++index) {
		perRow[ruleRows[index]] += counts[index];
	}

	uint32_t totalMatches = 0;
	for (const uint32_t count : perRow) {
		totalMatches += count;
	}

	if (totalMatches == 0) {
		wxMessageBox("No items matching the rules were found in the selected scope.", "Replace", wxOK | wxICON_INFORMATION);
		return;
	}

	wxString preview = wxString::Format("Up to %u item(s) can be replaced:\n\n", totalMatches);
	for (size_t row = 0; row < groups.size() && row < perRow.size(); ++row) {
		if (perRow[row] == 0) {
			continue;
		}
		const size_t sourceCount = groups[row].sources.size();
		preview += wxString::Format("Rule %u (%u source%s): %u hit(s)\n", static_cast<unsigned>(row + 1), static_cast<unsigned>(sourceCount), sourceCount == 1 ? "" : "s", perRow[row]);
	}
	preview += "\nChances add up: a rule changes that share of its matches and leaves the rest untouched.\nExecute the replacements?";

	if (wxMessageBox(preview, "Confirm Bulk Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING) != wxYES) {
		return;
	}

	engine.ExecuteReplacement(editor, rules, scope, scope == ReplaceScope::Viewport ? &viewportPos : nullptr);
}

void ReplaceToolWindow::OnAddItemsAsRules(const std::vector<uint16_t>& itemIds) {
	ruleBuilder->AddRulesWithSources(itemIds);
}

void ReplaceToolWindow::OnRuleItemSelected(uint16_t itemId) {
	OnLibraryItemSelected(itemId);
}

void ReplaceToolWindow::OnSaveRule() {
	if (ruleBuilder->GetRules().empty()) {
		return;
	}

	// Ctrl+S lands here too: it writes the active set, or asks for a name.
	SaveActiveRuleSet();
}

void ReplaceToolWindow::OnSaveAsNewRule() {
	if (ruleBuilder->GetRules().empty()) {
		wxMessageBox("There is nothing to save yet - build a rule first.", "Save as new rule set", wxOK | wxICON_INFORMATION, this);
		return;
	}
	const wxString name = wxGetTextFromUser("Name for the new rule set:", "Save as new rule set", "", this);
	SaveRulesAsNewRuleSet(name);
}

void ReplaceToolWindow::OnAutoMatchRules(Brush* brush) {
	if (!brush) {
		return;
	}
	const size_t matchedCount = ruleBuilder->AutoMatchUnmatchedRules(brush);
	if (matchedCount > 0) {
		wxMessageBox(wxString::Format("Successfully matched and filled %zu rule(s) from brush '%s'.", matchedCount, wxstr(brush->getName())), "Auto Match Rules", wxOK | wxICON_INFORMATION, this);
	} else {
		wxMessageBox(wxString::Format("No unmatched rules were compatible with brush '%s'.", wxstr(brush->getName())), "Auto Match Rules", wxOK | wxICON_INFORMATION, this);
	}
}

void ReplaceToolWindow::OnClearRules() {
	ruleBuilder->Clear();
	m_currentSuggestionItemId = 0;
	if (m_noFilterGrid) {
		m_noFilterGrid->SetItems({});
	}
	if (m_similarGrid) {
		m_similarGrid->SetItems({});
	}
	if (m_exactGrid) {
		m_exactGrid->SetItems({});
	}
}

void ReplaceToolWindow::OnItemSelected(ItemGridPanel* source, uint16_t itemId) {
	if (source == m_noFilterGrid || source == m_similarGrid || source == m_exactGrid) {
		return;
	}
	OnLibraryItemSelected(itemId);
}
