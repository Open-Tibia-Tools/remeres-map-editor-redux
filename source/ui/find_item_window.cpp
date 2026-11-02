#include "app/main.h"
#include "ui/find_item_window_internal.h"
#include "palette/controls/tileset_grid_toolbar.h"

#include "brushes/creature/creature_brush.h"
#include "brushes/raw/raw_brush.h"
#include "item_definitions/core/item_definition_store.h"
#include "ui/gui.h"
#include "util/common.h"
#include "util/image_manager.h"

#include <wx/button.h>

FindItemDialog::FindItemDialog(
	wxWindow* parent,
	const wxString& title,
	bool onlyPickupables,
	ActionSet action_set,
	AdvancedFinderDefaultAction default_action,
	bool include_creatures
) :
	wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	only_pickupables_(onlyPickupables),
	include_creatures_(include_creatures),
	persist_shared_state_(action_set == ActionSet::SearchAndSelect),
	action_set_(action_set),
	default_action_(default_action) {
	buildLayout();
	bindEvents();
	loadInitialState();
	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_SEARCH));
}

FindItemDialog::~FindItemDialog() {
	savePersistedState();
}

void FindItemDialog::refreshResults() {
	readQueryFromControls();

	auto filtered_rows = FilterAdvancedFinderCatalog(catalog_, query_);
	has_filtered_results_ = !filtered_rows.empty();
	if (filtered_rows.empty()) {
		list_results_view_->SetNoMatches("No results", "Try a different word or fewer filters.");
		grid_results_view_->SetNoMatches("No results", "Try a different word or fewer filters.");
		current_selection_ = {};
		updateResultTitle(0);
		updateButtons();
		return;
	}

	std::vector<const AdvancedFinderCatalogRow*> rows;
	rows.reserve(filtered_rows.size());
	for (const size_t filtered_index : filtered_rows) {
		rows.push_back(&catalog_[filtered_index]);
	}

	list_results_view_->SetRows(rows, current_selection_);
	grid_results_view_->SetRows(rows, current_selection_);

	if (auto* view = activeResultsView()) {
		if (const auto* row = view->GetSelectedRow()) {
			current_selection_ = MakeAdvancedFinderSelectionKey(*row);
		} else {
			current_selection_ = {};
		}
	} else {
		current_selection_ = {};
	}

	updateResultTitle(filtered_rows.size());
	updateButtons();
}

void FindItemDialog::updateButtons() {
	const bool has_selection = activeResultsView() != nullptr && activeResultsView()->GetSelectedRow() != nullptr;

	if (search_filtered_button_ != nullptr) {
		search_filtered_button_->Enable(has_filtered_results_);
	}
	if (search_map_button_ != nullptr) {
		search_map_button_->Enable(has_selection);
	}
	if (select_item_button_ != nullptr) {
		select_item_button_->Enable(has_selection);
	}
	if (ok_button_ != nullptr) {
		ok_button_->Enable(has_selection);
	}
}

void FindItemDialog::updateCurrentSelection() {
	if (const auto* view = activeResultsView(); view != nullptr && view->GetSelectedRow() != nullptr) {
		const auto* row = view->GetSelectedRow();
		current_selection_ = MakeAdvancedFinderSelectionKey(*row);
	} else {
		current_selection_ = {};
	}
}

void FindItemDialog::triggerDefaultAction() {
	if (action_set_ == ActionSet::SearchAndSelect) {
		handlePositiveAction(default_action_ == AdvancedFinderDefaultAction::SearchMap ? ResultAction::SearchMap : ResultAction::SelectItem);
	} else {
		handlePositiveAction(ResultAction::ConfirmSelection);
	}
}

void FindItemDialog::handlePositiveAction(ResultAction action) {
	if (deferred_select_timer_.IsRunning()) {
		deferred_select_timer_.Stop();
	}

	const AdvancedFinderCatalogRow* selected_row = activeResultsView() != nullptr ? activeResultsView()->GetSelectedRow() : nullptr;
	if (selected_row == nullptr) {
		return;
	}

	result_kind_ = selected_row->kind;
	result_brush_ = selected_row->brush;
	result_creature_brush_ = selected_row->creature_brush;
	result_id_ = selected_row->isItem() ? selected_row->server_id : 0;
	result_creature_name_ = selected_row->isCreature() ? selected_row->label : std::string {};
	result_action_ = action;
	updateCurrentSelection();

	EndModal(action == ResultAction::SearchMap ? wxID_FIND : wxID_OK);
}

void FindItemDialog::setResultViewMode(AdvancedFinderResultViewMode mode) {
	result_view_mode_ = mode;
	if (results_notebook_ != nullptr) {
		results_notebook_->ChangeSelection(findItemDialogIndex(mode));
	}
	if (grid_toolbar_ != nullptr) {
		grid_toolbar_->SetListMode(mode == AdvancedFinderResultViewMode::List);
	}
}

AdvancedFinderResultsView* FindItemDialog::activeResultsView() const {
	return result_view_mode_ == AdvancedFinderResultViewMode::Grid ? grid_results_view_ : list_results_view_;
}

void FindItemDialog::OnFilterChanged(wxCommandEvent& WXUNUSED(event)) {
	refreshResults();
}

void FindItemDialog::OnTextChanged(wxCommandEvent& WXUNUSED(event)) {
	refreshResults();
}

void FindItemDialog::syncResultViews(AdvancedFinderResultsView* source_view) {
	if (source_view == nullptr) {
		return;
	}

	AdvancedFinderResultsView* target_view = source_view == list_results_view_ ? grid_results_view_ : list_results_view_;
	if (target_view != nullptr) {
		target_view->SetSelectionIndex(source_view->GetSelectionIndex());
	}

	if (const auto* row = source_view->GetSelectedRow()) {
		current_selection_ = MakeAdvancedFinderSelectionKey(*row);
	} else {
		current_selection_ = {};
	}

	updateButtons();
}

void FindItemDialog::OnResultPageChanged(wxBookCtrlEvent& event) {
	const auto page = event.GetSelection();
	if (page == wxNOT_FOUND) {
		event.Skip();
		return;
	}

	result_view_mode_ = page == 0 ? AdvancedFinderResultViewMode::List : AdvancedFinderResultViewMode::Grid;
	if (grid_toolbar_ != nullptr) {
		grid_toolbar_->SetListMode(result_view_mode_ == AdvancedFinderResultViewMode::List);
	}
	updateCurrentSelection();
	updateButtons();
	event.Skip();
}

void FindItemDialog::OnResultSelection(wxCommandEvent& event) {
	auto* source_view = dynamic_cast<AdvancedFinderResultsView*>(event.GetEventObject());
	if (source_view == nullptr) {
		return;
	}

	syncResultViews(source_view);
}

void FindItemDialog::OnResultActivate(wxCommandEvent& WXUNUSED(event)) {
	if (action_set_ == ActionSet::SearchAndSelect) {
		handlePositiveAction(ResultAction::SearchMap);
		return;
	}
	handlePositiveAction(ResultAction::ConfirmSelection);
}

void FindItemDialog::OnResultRightActivate(wxCommandEvent& WXUNUSED(event)) {
	if (action_set_ == ActionSet::SearchAndSelect) {
		if (deferred_select_timer_.IsRunning()) {
			deferred_select_timer_.Stop();
		}
		deferred_select_timer_.StartOnce(300);
		return;
	}
	handlePositiveAction(ResultAction::ConfirmSelection);
}

void FindItemDialog::OnSearchMap(wxCommandEvent& WXUNUSED(event)) {
	handlePositiveAction(ResultAction::SearchMap);
}

void FindItemDialog::OnSearchFiltered(wxCommandEvent& WXUNUSED(event)) {
	readQueryFromControls();
	auto filtered_rows = FilterAdvancedFinderCatalog(catalog_, query_);
	if (filtered_rows.empty()) {
		return;
	}

	filtered_item_ids_.clear();
	filtered_creature_brushes_.clear();

	for (const size_t filtered_index : filtered_rows) {
		if (filtered_index < catalog_.size()) {
			const auto& row = catalog_[filtered_index];
			if (row.isItem() && row.server_id > 0) {
				filtered_item_ids_.push_back(row.server_id);
			} else if (row.isCreature() && row.creature_brush != nullptr) {
				filtered_creature_brushes_.push_back(row.creature_brush);
			}
		}
	}

	if (filtered_item_ids_.empty() && filtered_creature_brushes_.empty()) {
		return;
	}

	result_action_ = ResultAction::SearchFiltered;
	EndModal(wxID_FIND);
}

void FindItemDialog::OnSelectItem(wxCommandEvent& WXUNUSED(event)) {
	handlePositiveAction(action_set_ == ActionSet::SearchAndSelect ? ResultAction::SelectItem : ResultAction::ConfirmSelection);
}

void FindItemDialog::OnResetSearch(wxCommandEvent& WXUNUSED(event)) {
	search_field_->Clear();

	for (wxCheckBox* checkbox : type_checkboxes_) {
		if (checkbox->IsEnabled()) {
			checkbox->SetValue(false);
		}
	}
	for (wxCheckBox* checkbox : property_checkboxes_) {
		if (checkbox->IsEnabled()) {
			checkbox->SetValue(false);
		}
	}
	for (wxCheckBox* checkbox : interaction_checkboxes_) {
		if (checkbox->IsEnabled()) {
			checkbox->SetValue(false);
		}
	}
	for (wxCheckBox* checkbox : visual_checkboxes_) {
		if (checkbox->IsEnabled()) {
			checkbox->SetValue(false);
		}
	}

	refreshResults();
	search_field_->SetFocus();
}

void FindItemDialog::OnDeferredSelectTimer(wxTimerEvent& WXUNUSED(event)) {
	if (IsModal()) {
		handlePositiveAction(ResultAction::SelectItem);
	}
}

void FindItemDialog::OnCancel(wxCommandEvent& WXUNUSED(event)) {
	result_action_ = ResultAction::None;
	EndModal(wxID_CANCEL);
}

void FindItemDialog::OnTextEnter(wxCommandEvent& WXUNUSED(event)) {
	triggerDefaultAction();
}

void FindItemDialog::OnKeyDown(wxKeyEvent& event) {
	if (event.GetEventObject() != search_field_) {
		event.Skip();
		return;
	}

	if (auto* view = activeResultsView(); view != nullptr) {
		wxKeyEvent forwarded(event);
		forwarded.SetEventObject(view);
		if (view->GetEventHandler()->ProcessEvent(forwarded)) {
			return;
		}
	}

	event.Skip();
}
