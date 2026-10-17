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
#include <wx/statline.h>
#include <wx/stattext.h>
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

namespace {

// Tiles currently visible in the active map viewport (all map positions at
// the current floor framed by the canvas view).
std::vector<Position> collectViewportPositions() {
	MapTab* activeTab = g_gui.GetCurrentMapTab();
	if (!activeTab || !activeTab->GetCanvas()) {
		return {};
	}

	MapCanvas* canvas = activeTab->GetCanvas();
	wxSize canvasSize = canvas->GetClientSize();

	int startX, startY, endX, endY;
	canvas->ScreenToMap(0, 0, &startX, &startY);
	canvas->ScreenToMap(canvasSize.x, canvasSize.y, &endX, &endY);

	// Ensure bounds are consistent
	if (startX > endX) {
		std::swap(startX, endX);
	}
	if (startY > endY) {
		std::swap(startY, endY);
	}

	const int z = canvas->GetFloor();

	std::vector<Position> positions;
	for (int x = startX; x <= endX; ++x) {
		for (int y = startY; y <= endY; ++y) {
			positions.push_back(Position(x, y, z));
		}
	}
	return positions;
}

} // namespace

ReplaceToolWindow::ReplaceToolWindow(wxWindow* parent, Editor* editor) : wxDialog(parent, wxID_ANY, "Advanced Replace Tool", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
																		 editor(editor) {

	VisualSimilarityService::Get().StartIndexing();
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	InitLayout();

	UpdateSavedRulesList();

	Bind(wxEVT_CLOSE_WINDOW, &ReplaceToolWindow::OnClose, this);

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

ReplaceToolWindow::~ReplaceToolWindow() {
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
	event.Skip(); // Allow window to close
}

void ReplaceToolWindow::InitializeWithIDs(const std::vector<uint16_t>& ids) {
	if (ids.empty()) {
		return;
	}

	std::vector<ReplacementRule> rules;
	for (uint16_t id : ids) {
		ReplacementRule rule;
		rule.fromId = id;
		rules.push_back(rule);
	}

	// IDs are already sorted from std::set in MapMenuHandler, but sorting here ensures consistency
	std::sort(rules.begin(), rules.end(), [](const ReplacementRule& a, const ReplacementRule& b) {
		return a.fromId < b.fromId;
	});

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

	libraryPanel = new LibraryPanel(col1Card, this);
	col1Card->GetContentSizer()->Add(libraryPanel, wxSizerFlags(1).Expand().Border(wxALL, padding / 2));
	mainRowSizer->Add(col1Card, wxSizerFlags(3).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 3

	// ---------------------------------------------------------
	// COLUMN 2: Rule Builder
	// ---------------------------------------------------------
	CardPanel* col2Card = new CardPanel(this, wxID_ANY);
	col2Card->SetTitle("RULE BUILDER");

	ruleBuilder = new RuleBuilderPanel(col2Card, this);
	col2Card->GetContentSizer()->Add(ruleBuilder, wxSizerFlags(1).Expand().Border(wxALL, padding));
	col2Card->SetShowFooter(false); // No footer anymore

	mainRowSizer->Add(col2Card, wxSizerFlags(4).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 4

	// ---------------------------------------------------------
	// COLUMN 3: Similarity Engine
	// ---------------------------------------------------------
	CardPanel* col3Card = new CardPanel(this, wxID_ANY);
	col3Card->SetTitle("SMART SUGGESTIONS");

	similarItemsGrid = new ItemGridPanel(col3Card, this);
	similarItemsGrid->SetDraggable(true);

	TilesetGridToolbar::Actions suggestionActions;
	suggestionActions.filter = [this](const std::string& query) {
		similarItemsGrid->SetFilter(wxstr(query));
	};
	suggestionActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		similarItemsGrid->SetSort(key, dir);
		similarItemsGrid->SetFilter(wxstr(m_suggestionsToolbar->GetFilterQuery()));
	};
	suggestionActions.clear_sort = [this]() {
		similarItemsGrid->ClearSort();
		similarItemsGrid->SetFilter(wxstr(m_suggestionsToolbar->GetFilterQuery()));
	};
	suggestionActions.show_labels = [this](bool show) {
		similarItemsGrid->SetShowLabels(show);
	};
	suggestionActions.icon_size = [this](int size) {
		similarItemsGrid->SetIconSize(size);
	};

	m_suggestionsToolbar = new TilesetGridToolbar(col3Card, std::move(suggestionActions));
	m_suggestionsToolbar->SetState(similarItemsGrid->GetShowLabels(), similarItemsGrid->GetIconSize(), false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);

	col3Card->GetContentSizer()->Add(m_suggestionsToolbar, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxTOP, padding));
	col3Card->GetContentSizer()->Add(similarItemsGrid, wxSizerFlags(1).Expand().Border(wxALL, padding));
	mainRowSizer->Add(col3Card, wxSizerFlags(3).Expand().Border(wxLEFT | wxRIGHT, padding / 2)); // Flex 3

	// ---------------------------------------------------------
	// COLUMN 4: Saved Rules
	// ---------------------------------------------------------
	CardPanel* savedRulesCard = new CardPanel(this, wxID_ANY);
	savedRulesCard->SetTitle("SAVED RULES");
	savedRulesCard->SetShowFooter(false);

	savedRulesList = new RuleListControl(savedRulesCard, this);
	savedRulesCard->GetContentSizer()->Add(savedRulesList, wxSizerFlags(1).Expand().Border(wxALL, padding));

	mainRowSizer->Add(savedRulesCard, wxSizerFlags(2).Expand().Border(wxALL, padding / 2)); // Flex 2

	// ---------------------------------------------------------
	// FOOTER: rule set management, scope and execution
	//
	// These used to take a whole column; in one row they free the width for
	// the library, the rule builder and the suggestions.
	// ---------------------------------------------------------
	wxBoxSizer* footerSizer = newd wxBoxSizer(wxHORIZONTAL);

	m_addRuleBtn = newd wxButton(this, wxID_ANY, "ADD", wxDefaultPosition, FromDIP(wxSize(-1, 28)));
	m_addRuleBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	m_addRuleBtn->SetBackgroundColour(wxColour(40, 180, 40)); // Green
	m_addRuleBtn->SetForegroundColour(*wxWHITE);
	m_addRuleBtn->SetFont(Theme::GetFont(9, true));

	m_editRuleBtn = newd wxButton(this, wxID_ANY, "EDIT", wxDefaultPosition, FromDIP(wxSize(-1, 28)));
	m_editRuleBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN_TO_SQUARE, wxSize(16, 16)));
	m_editRuleBtn->SetBackgroundColour(wxColour(80, 80, 80)); // Neutral Dark Gray
	m_editRuleBtn->SetForegroundColour(*wxWHITE);
	m_editRuleBtn->SetFont(Theme::GetFont(9, true));

	m_deleteRuleBtn = newd wxButton(this, wxID_ANY, "DEL", wxDefaultPosition, FromDIP(wxSize(-1, 28)));
	m_deleteRuleBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, wxSize(16, 16)));
	m_deleteRuleBtn->SetBackgroundColour(wxColour(180, 40, 40)); // Red
	m_deleteRuleBtn->SetForegroundColour(*wxWHITE);
	m_deleteRuleBtn->SetFont(Theme::GetFont(9, true));

	footerSizer->Add(m_addRuleBtn, wxSizerFlags(0).Center().Border(wxRIGHT, 2));
	footerSizer->Add(m_editRuleBtn, wxSizerFlags(0).Center().Border(wxRIGHT, 2));
	footerSizer->Add(m_deleteRuleBtn, wxSizerFlags(0).Center().Border(wxRIGHT, padding));

	m_addVisibleBtn = newd wxButton(this, wxID_ANY, "ADD VISIBLE FROM VIEWPORT", wxDefaultPosition, FromDIP(wxSize(-1, 28)));
	m_addVisibleBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS_SQUARE, wxSize(16, 16)));
	m_addVisibleBtn->SetBackgroundColour(wxColour(45, 120, 180)); // Sky Blue
	m_addVisibleBtn->SetForegroundColour(*wxWHITE);
	m_addVisibleBtn->SetFont(Theme::GetFont(9, true));
	footerSizer->Add(m_addVisibleBtn, wxSizerFlags(0).Center().Border(wxRIGHT, padding));

	wxStaticText* scopeLabel = newd wxStaticText(this, wxID_ANY, "SCOPE:");
	scopeLabel->SetForegroundColour(wxColour(180, 180, 180));
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
	footerSizer->Add(m_scopeChoice, wxSizerFlags(0).Center());

	footerSizer->AddStretchSpacer(1);

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
	m_addVisibleBtn->Bind(wxEVT_BUTTON, &ReplaceToolWindow::OnAddVisibleTiles, this);
	closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });

	m_addRuleBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		std::vector<ReplacementRule> rules = ruleBuilder->GetRules();
		if (rules.empty()) {
			return;
		}
		wxString name = wxGetTextFromUser("Enter name for this rule set:", "Add Rule", "");
		if (!name.IsEmpty()) {
			RuleSet rs;
			rs.name = name.ToStdString();
			rs.rules = rules;
			if (RuleManager::Get().SaveRuleSet(rs)) {
				UpdateSavedRulesList();
			}
		}
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
	if (itemId == 0) {
		return;
	}
	similarItemsGrid->SetItems(VisualSimilarityService::Get().FindSimilar(itemId));
	similarItemsGrid->SetFilter(wxstr(m_suggestionsToolbar->GetFilterQuery()));
}

void ReplaceToolWindow::OnRuleSelected(const RuleSet& rs) {
	m_activeRuleSetName = rs.name;
	ruleBuilder->SetRules(rs.rules);
	if (!rs.rules.empty()) {
		similarItemsGrid->SetItems(VisualSimilarityService::Get().FindSimilar(rs.rules[0].fromId));
		similarItemsGrid->SetFilter(wxstr(m_suggestionsToolbar->GetFilterQuery()));
	} else {
		similarItemsGrid->SetItems({});
	}
}

void ReplaceToolWindow::OnRuleDeleted(const std::string& name) {
	if (RuleManager::Get().DeleteRuleSet(name)) {
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

void ReplaceToolWindow::OnRuleChanged() {
	if (!m_activeRuleSetName.empty()) {
		RuleSet rs;
		rs.name = m_activeRuleSetName;
		rs.rules = ruleBuilder->GetRules();
		RuleManager::Get().SaveRuleSet(rs);
	}
}

void ReplaceToolWindow::OnExecute(wxCommandEvent&) {
	std::vector<ReplacementRule> rules = ruleBuilder->GetRules();
	if (rules.empty()) {
		wxMessageBox("No rules defined. Add some rules first.", "Replace", wxOK | wxICON_INFORMATION);
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

	std::vector<uint32_t> counts = engine.CountMatchesByRule(editor, rules, scope, scope == ReplaceScope::Viewport ? &viewportPos : nullptr);

	uint32_t totalMatches = 0;
	for (uint32_t count : counts) {
		totalMatches += count;
	}

	if (totalMatches == 0) {
		wxMessageBox("No items matching the rules were found in the selected scope.", "Replace", wxOK | wxICON_INFORMATION);
		return;
	}

	wxString preview = "Up to " + wxString::Format("%u", totalMatches) + " item(s) can be replaced:\n\n";
	for (size_t index = 0; index < rules.size() && index < counts.size(); ++index) {
		if (counts[index] == 0) {
			continue;
		}
		preview += wxString::Format("Item %u: %u hit(s)\n", rules[index].fromId, counts[index]);
	}
	preview += "\nProbabilities below 100% leave some matches untouched.\nExecute the replacements?";

	int confirm = wxMessageBox(preview, "Confirm Bulk Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
	if (confirm != wxYES) {
		return;
	}

	engine.ExecuteReplacement(editor, rules, scope, scope == ReplaceScope::Viewport ? &viewportPos : nullptr);
}

void ReplaceToolWindow::OnAddVisibleTiles(wxCommandEvent&) {
	MapTab* activeTab = g_gui.GetCurrentMapTab();
	if (!activeTab || !activeTab->GetCanvas()) {
		return;
	}

	MapCanvas* canvas = activeTab->GetCanvas();
	wxSize canvasSize = canvas->GetClientSize();

	int startX, startY, endX, endY;
	canvas->ScreenToMap(0, 0, &startX, &startY);
	canvas->ScreenToMap(canvasSize.x, canvasSize.y, &endX, &endY);

	// Ensure bounds are consistent
	if (startX > endX) {
		std::swap(startX, endX);
	}
	if (startY > endY) {
		std::swap(startY, endY);
	}

	int z = canvas->GetFloor();

	std::set<uint16_t> uniqueIds;
	for (int x = startX; x <= endX; ++x) {
		for (int y = startY; y <= endY; ++y) {
			Tile* tile = editor->map.getTile(x, y, z);
			if (tile) {
				if (tile->ground) {
					uniqueIds.insert(tile->ground->getID());
				}
				for (const auto& item : tile->items) {
					uniqueIds.insert(item->getID());
				}
			}
		}
	}

	if (uniqueIds.empty()) {
		return;
	}

	std::vector<ReplacementRule> currentRules = ruleBuilder->GetRules();
	std::set<uint16_t> existingIds;
	for (const auto& r : currentRules) {
		existingIds.insert(r.fromId);
	}

	bool added = false;
	for (uint16_t id : uniqueIds) {
		if (id != 0 && existingIds.find(id) == existingIds.end()) {
			ReplacementRule nr;
			nr.fromId = id;
			currentRules.push_back(nr);
			added = true;
		}
	}

	if (added) {
		ruleBuilder->SetRules(currentRules);
	}
}

void ReplaceToolWindow::OnRuleItemSelected(uint16_t itemId) {
	OnLibraryItemSelected(itemId);
}

void ReplaceToolWindow::OnSaveRule() {
	std::vector<ReplacementRule> rules = ruleBuilder->GetRules();
	if (rules.empty()) {
		return;
	}

	wxString name = wxGetTextFromUser("Enter name for this rule set:", "Save Rule", "");
	if (name.IsEmpty()) {
		return;
	}

	std::vector<std::string> existing = RuleManager::Get().GetAvailableRuleSets();
	if (std::find(existing.begin(), existing.end(), name.ToStdString()) != existing.end()) {
		wxMessageBox("A rule set with this name already exists. Please choose a different name.", "Name Collision", wxOK | wxICON_ERROR);
		return;
	}

	RuleSet rs;
	rs.name = name.ToStdString();
	rs.rules = rules;

	if (RuleManager::Get().SaveRuleSet(rs)) {
		m_activeRuleSetName = rs.name;
		UpdateSavedRulesList();
	}
}

void ReplaceToolWindow::UpdateSavedRulesList() {
	savedRulesList->SetRuleSets(RuleManager::Get().GetAvailableRuleSets());
}

void ReplaceToolWindow::OnClearRules() {
	ruleBuilder->Clear();
	similarItemsGrid->SetItems({});
}

void ReplaceToolWindow::OnItemSelected(ItemGridPanel* source, uint16_t itemId) {
	if (source == similarItemsGrid) {
		return;
	}
	OnLibraryItemSelected(itemId);
}
