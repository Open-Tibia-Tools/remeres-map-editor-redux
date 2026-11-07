#ifndef RME_FIND_ITEM_WINDOW_H_
#define RME_FIND_ITEM_WINDOW_H_

#include "ui/find_item_window_model.h"
#include "ui/find_item_window_views.h"

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/timer.h>

#include <array>
#include <string>
#include <vector>

class Brush;
class CreatureBrush;
class TilesetGridToolbar;
class wxButton;
class wxBitmapButton;
class wxCheckBox;
class wxStaticText;
class wxSearchCtrl;
class wxNotebook;

class FindItemDialog : public wxDialog {
public:
	enum class ActionSet : uint8_t {
		ConfirmOnly = 0,
		SearchAndSelect,
	};

	enum class ResultAction : uint8_t {
		None = 0,
		ConfirmSelection,
		SearchMap,
		SearchFiltered,
		SelectItem,
	};

	FindItemDialog(
		wxWindow* parent,
		const wxString& title,
		bool onlyPickupables = false,
		ActionSet action_set = ActionSet::ConfirmOnly,
		AdvancedFinderDefaultAction default_action = AdvancedFinderDefaultAction::SelectItem,
		bool include_creatures = false
	);
	~FindItemDialog() override;

	[[nodiscard]] const Brush* getResult() const {
		return result_brush_;
	}

	[[nodiscard]] uint16_t getResultID() const {
		return result_id_;
	}

	[[nodiscard]] AdvancedFinderCatalogKind getResultKind() const {
		return result_kind_;
	}

	[[nodiscard]] CreatureBrush* getResultCreatureBrush() const {
		return result_creature_brush_;
	}

	[[nodiscard]] const std::string& getResultCreatureName() const {
		return result_creature_name_;
	}

	[[nodiscard]] ResultAction getResultAction() const {
		return result_action_;
	}

	[[nodiscard]] const std::vector<uint16_t>& getFilteredItemIDs() const {
		return filtered_item_ids_;
	}

	[[nodiscard]] const std::vector<const Brush*>& getFilteredCreatureBrushes() const {
		return filtered_creature_brushes_;
	}

private:
	static constexpr size_t kTypeCheckboxCount = static_cast<size_t>(AdvancedFinderTypeFilter::Count);
	static constexpr size_t kPropertyCheckboxCount = static_cast<size_t>(AdvancedFinderPropertyFilter::Count);
	static constexpr size_t kInteractionCheckboxCount = static_cast<size_t>(AdvancedFinderInteractionFilter::Count);
	static constexpr size_t kVisualCheckboxCount = static_cast<size_t>(AdvancedFinderVisualFilter::Count);

	void buildLayout();
	void bindEvents();
	void loadInitialState();
	void savePersistedState();
	void applyQueryToControls() const;
	void readQueryFromControls();
	void refreshResults();
	void updateButtons();
	void updateResultTitle(size_t count);
	void updateCurrentSelection();
	void triggerDefaultAction();
	void handlePositiveAction(ResultAction action);
	void setResultViewMode(AdvancedFinderResultViewMode mode);
	[[nodiscard]] AdvancedFinderResultsView* activeResultsView() const;
	void syncResultViews(AdvancedFinderResultsView* source_view);

	void OnFilterChanged(wxCommandEvent& event);
	void OnTextChanged(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void OnResultPageChanged(wxBookCtrlEvent& event);
	void OnResultSelection(wxCommandEvent& event);
	void OnResultActivate(wxCommandEvent& event);
	void OnResultRightActivate(wxCommandEvent& event);
	void OnSearchMap(wxCommandEvent& event);
	void OnSearchFiltered(wxCommandEvent& event);
	void OnSelectItem(wxCommandEvent& event);
	void OnResetSearch(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnTextEnter(wxCommandEvent& event);
	void OnDeferredSelectTimer(wxTimerEvent& event);

	wxSearchCtrl* search_field_ = nullptr;
	wxBitmapButton* reset_search_button_ = nullptr;
	std::array<wxCheckBox*, kTypeCheckboxCount> type_checkboxes_ {};
	std::array<wxCheckBox*, kPropertyCheckboxCount> property_checkboxes_ {};
	std::array<wxCheckBox*, kInteractionCheckboxCount> interaction_checkboxes_ {};
	std::array<wxCheckBox*, kVisualCheckboxCount> visual_checkboxes_ {};
	wxStaticText* result_count_label_ = nullptr;
	wxButton* search_filtered_button_ = nullptr;
	wxButton* search_map_button_ = nullptr;
	wxButton* select_item_button_ = nullptr;
	wxButton* ok_button_ = nullptr;
	wxButton* cancel_button_ = nullptr;
	wxNotebook* results_notebook_ = nullptr;
	AdvancedFinderResultsView* list_results_view_ = nullptr;
	AdvancedFinderResultsView* grid_results_view_ = nullptr;
	TilesetGridToolbar* grid_toolbar_ = nullptr;

	AdvancedFinderQuery query_;
	AdvancedFinderPersistedState persisted_state_;
	AdvancedFinderSelectionKey current_selection_;
	std::vector<AdvancedFinderCatalogRow> catalog_;
	std::vector<uint16_t> filtered_item_ids_;
	std::vector<const Brush*> filtered_creature_brushes_;
	bool has_filtered_results_ = false;

	ResultAction result_action_ = ResultAction::None;
	AdvancedFinderCatalogKind result_kind_ = AdvancedFinderCatalogKind::Item;
	Brush* result_brush_ = nullptr;
	CreatureBrush* result_creature_brush_ = nullptr;
	uint16_t result_id_ = 0;
	std::string result_creature_name_;
	AdvancedFinderResultViewMode result_view_mode_ = AdvancedFinderResultViewMode::Grid;
	wxTimer deferred_select_timer_ { this };

	bool only_pickupables_ = false;
	bool include_creatures_ = false;
	bool persist_shared_state_ = false;
	ActionSet action_set_ = ActionSet::ConfirmOnly;
	AdvancedFinderDefaultAction default_action_ = AdvancedFinderDefaultAction::SelectItem;
};

#endif
