#include "app/main.h"
#include "ui/find_item_window_internal.h"

#include "app/settings.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "ui/theme.h"
#include "util/common.h"
#include "util/image_manager.h"

#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <algorithm>
#include <format>

std::optional<SessionFinderState> g_session_finder_state;

namespace {
	wxStaticBoxSizer* createStaticBox(wxWindow* parent, std::string_view title) {
		auto* group = new wxStaticBoxSizer(wxVERTICAL, parent, wxString::FromUTF8(title.data(), title.size()));
		group->GetStaticBox()->SetForegroundColour(Theme::Get(Theme::Role::Text));
		group->GetStaticBox()->SetFont(Theme::GetFont(9, true));
		return group;
	}

	wxStaticText* createHintText(wxWindow* parent, std::string_view text) {
		auto* label = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(text.data(), text.size()));
		label->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
		label->SetFont(Theme::GetFont(9, false));
		return label;
	}

	template <typename FilterEnum, size_t Count>
	wxGridSizer* addCheckboxGrid(wxWindow* parent, const std::array<std::pair<FilterEnum, std::string_view>, Count>& entries, std::array<wxCheckBox*, Count>& targets) {
		auto* grid = new wxGridSizer(0, 2, 6, 12);
		for (const auto& [filter, label] : entries) {
			auto* checkbox = new wxCheckBox(parent, wxID_ANY, wxString::FromUTF8(label.data(), label.size()));
			checkbox->SetFont(Theme::GetFont(9, false));
			checkbox->SetForegroundColour(Theme::Get(Theme::Role::Text));
			targets[findItemDialogIndex(filter)] = checkbox;
			grid->Add(checkbox, wxSizerFlags().Expand());
		}
		return grid;
	}
}

ForwardingSearchCtrl::ForwardingSearchCtrl(wxWindow* parent, wxWindowID id) :
	wxSearchCtrl(parent, id, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER) {
	Bind(wxEVT_KEY_DOWN, &ForwardingSearchCtrl::OnKeyDown, this);
}

void ForwardingSearchCtrl::OnKeyDown(wxKeyEvent& event) {
	switch (event.GetKeyCode()) {
		case WXK_UP:
		case WXK_DOWN:
		case WXK_PAGEUP:
		case WXK_PAGEDOWN:
		case WXK_HOME:
		case WXK_END:
			if (auto* top_level = wxGetTopLevelParent(this)) {
				if (top_level->GetEventHandler()->ProcessEvent(event)) {
					return;
				}
				return;
			}
			break;
		default:
			break;
	}

	event.Skip();
}

void FindItemDialog::buildLayout() {
	SetMinSize(FromDIP(wxSize(1360, 860)));
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* body_sizer = new wxBoxSizer(wxHORIZONTAL);

	auto* left_panel = new wxPanel(this, wxID_ANY);
	left_panel->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	left_panel->SetMinSize(FromDIP(wxSize(360, -1)));
	auto* left_sizer = new wxBoxSizer(wxVERTICAL);

	auto* search_box = createStaticBox(left_panel, "Search");
	auto* search_box_sizer = new wxBoxSizer(wxHORIZONTAL);
	search_field_ = new ForwardingSearchCtrl(search_box->GetStaticBox(), wxID_ANY);
	search_field_->SetDescriptiveText("Name, SID, or CID...");
	search_field_->SetMinSize(FromDIP(wxSize(-1, 32)));
	search_box_sizer->Add(search_field_, wxSizerFlags(1).Expand().Border(wxALL, 8));
	reset_search_button_ = new wxBitmapButton(search_box->GetStaticBox(), wxID_ANY, IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	reset_search_button_->SetMinSize(FromDIP(wxSize(30, 30)));
	search_box_sizer->Add(reset_search_button_, wxSizerFlags().Border(wxTOP | wxRIGHT | wxBOTTOM, 8));
	search_box->Add(search_box_sizer, wxSizerFlags(1).Expand());
	left_sizer->Add(search_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 12));

	auto* filter_scroll = new wxScrolledWindow(left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
	filter_scroll->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	filter_scroll->SetScrollRate(0, FromDIP(18));

	auto* filter_scroll_sizer = new wxBoxSizer(wxVERTICAL);
	auto* or_box = createStaticBox(filter_scroll, "OR");
	or_box->Add(addCheckboxGrid(or_box->GetStaticBox(), kTypeCheckboxes, type_checkboxes_), wxSizerFlags().Expand().Border(wxALL, 8));
	filter_scroll_sizer->Add(or_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 4));

	auto* and_box = createStaticBox(filter_scroll, "AND");
	auto* and_grid = new wxGridSizer(0, 2, 6, 12);
	for (const auto& [property_filter, label] : kPropertyCheckboxes) {
		auto* checkbox = new wxCheckBox(and_box->GetStaticBox(), wxID_ANY, wxString::FromUTF8(label.data(), label.size()));
		checkbox->SetFont(Theme::GetFont(9, false));
		checkbox->SetForegroundColour(Theme::Get(Theme::Role::Text));
		property_checkboxes_[findItemDialogIndex(property_filter)] = checkbox;
		and_grid->Add(checkbox, wxSizerFlags().Expand());
	}
	for (const auto& [interaction_filter, label] : kInteractionCheckboxes) {
		auto* checkbox = new wxCheckBox(and_box->GetStaticBox(), wxID_ANY, wxString::FromUTF8(label.data(), label.size()));
		checkbox->SetFont(Theme::GetFont(9, false));
		checkbox->SetForegroundColour(Theme::Get(Theme::Role::Text));
		interaction_checkboxes_[findItemDialogIndex(interaction_filter)] = checkbox;
		and_grid->Add(checkbox, wxSizerFlags().Expand());
	}
	for (const auto& [visual_filter, label] : kVisualCheckboxes) {
		auto* checkbox = new wxCheckBox(and_box->GetStaticBox(), wxID_ANY, wxString::FromUTF8(label.data(), label.size()));
		checkbox->SetFont(Theme::GetFont(9, false));
		checkbox->SetForegroundColour(Theme::Get(Theme::Role::Text));
		visual_checkboxes_[findItemDialogIndex(visual_filter)] = checkbox;
		and_grid->Add(checkbox, wxSizerFlags().Expand());
	}
	and_box->Add(and_grid, wxSizerFlags().Expand().Border(wxALL, 8));
	filter_scroll_sizer->Add(and_box, wxSizerFlags().Expand().Border(wxALL, 4));

	auto* hints_box = createStaticBox(filter_scroll, "Hints");
	if (action_set_ == ActionSet::SearchAndSelect) {
		hints_box->Add(createHintText(hints_box->GetStaticBox(), "Double left click item to search the item on the map"), wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, 8));
		hints_box->Add(createHintText(hints_box->GetStaticBox(), "Double right click to select item in the tileset"), wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8));
	} else {
		hints_box->Add(createHintText(hints_box->GetStaticBox(), "Double left click item to confirm selection"), wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8));
	}
	filter_scroll_sizer->Add(hints_box, wxSizerFlags().Expand().Border(wxALL, 4));

	filter_scroll->SetSizer(filter_scroll_sizer);
	filter_scroll->FitInside();
	left_sizer->Add(filter_scroll, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 12));
	left_panel->SetSizer(left_sizer);
	body_sizer->Add(left_panel, wxSizerFlags().Expand().Border(wxLEFT | wxTOP | wxBOTTOM, 12));

	auto* right_panel = new wxPanel(this, wxID_ANY);
	right_panel->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	auto* right_sizer = new wxBoxSizer(wxVERTICAL);
	auto* results_box = createStaticBox(right_panel, "Results");
	auto* results_box_sizer = new wxBoxSizer(wxVERTICAL);

	TilesetGridToolbar::Actions toolbar_actions;
	toolbar_actions.list_mode = [this](bool listMode) {
		setResultViewMode(listMode ? AdvancedFinderResultViewMode::List : AdvancedFinderResultViewMode::Grid);
	};
	toolbar_actions.icon_size = [this](int size) {
		if (list_results_view_) list_results_view_->SetIconSize(size);
		if (grid_results_view_) grid_results_view_->SetIconSize(size);
		if (grid_toolbar_) grid_toolbar_->SetListMode(false);
		if (result_view_mode_ == AdvancedFinderResultViewMode::List) {
			setResultViewMode(AdvancedFinderResultViewMode::Grid);
		}
	};
	toolbar_actions.show_labels = [this](bool show) {
		if (list_results_view_) list_results_view_->SetShowLabels(show);
		if (grid_results_view_) grid_results_view_->SetShowLabels(show);
	};
	toolbar_actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		if (list_results_view_) list_results_view_->SetSort(key, dir);
		if (grid_results_view_) grid_results_view_->SetSort(key, dir);
	};
	toolbar_actions.clear_sort = [this]() {
		if (list_results_view_) list_results_view_->ClearSort();
		if (grid_results_view_) grid_results_view_->ClearSort();
	};
	toolbar_actions.brush_indicators = [this](bool dogEar) {
		if (list_results_view_) list_results_view_->SetBrushIndicators(dogEar);
		if (grid_results_view_) grid_results_view_->SetBrushIndicators(dogEar);
		if (grid_toolbar_) grid_toolbar_->SetIndicatorState(dogEar);
	};
	toolbar_actions.grid_spacing = [this](int spacingPx) {
		if (list_results_view_) list_results_view_->SetGridSpacing(spacingPx);
		if (grid_results_view_) grid_results_view_->SetGridSpacing(spacingPx);
		if (grid_toolbar_) grid_toolbar_->SetGridSpacing(spacingPx);
	};

	const unsigned toolbar_parts = TilesetGridToolbar::Part_Sort |
	                               TilesetGridToolbar::Part_Labels |
	                               TilesetGridToolbar::Part_Size |
	                               TilesetGridToolbar::Part_Indicators |
	                               TilesetGridToolbar::Part_Spacing;

	grid_toolbar_ = new TilesetGridToolbar(
		results_box->GetStaticBox(),
		std::move(toolbar_actions),
		toolbar_parts
	);
	grid_toolbar_->SetListMode(result_view_mode_ == AdvancedFinderResultViewMode::List);

	results_box_sizer->Add(grid_toolbar_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 4));

	results_notebook_ = new wxNotebook(results_box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
	results_notebook_->SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	list_results_view_ = new AdvancedFinderResultsView(results_notebook_, wxID_ANY);
	list_results_view_->SetMode(AdvancedFinderResultViewMode::List);
	list_results_view_->SetMinSize(FromDIP(wxSize(520, 520)));
	results_notebook_->AddPage(list_results_view_, "List", false);

	grid_results_view_ = new AdvancedFinderResultsView(results_notebook_, wxID_ANY);
	grid_results_view_->SetMode(AdvancedFinderResultViewMode::Grid);
	grid_results_view_->SetMinSize(FromDIP(wxSize(520, 520)));
	results_notebook_->AddPage(grid_results_view_, "Grid", true);

	results_box_sizer->Add(results_notebook_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP, 8));
	results_box->Add(results_box_sizer, wxSizerFlags(1).Expand());
	right_sizer->Add(results_box, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 12));
	right_panel->SetSizer(right_sizer);
	body_sizer->Add(right_panel, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 12));
	root_sizer->Add(body_sizer, wxSizerFlags(1).Expand());

	auto* footer_panel = new wxPanel(this, wxID_ANY);
	footer_panel->SetBackgroundColour(Theme::Get(Theme::Role::RaisedSurface));
	auto* footer_sizer = new wxBoxSizer(wxVERTICAL);
	auto* footer_line = new wxStaticLine(footer_panel, wxID_ANY);
	footer_sizer->Add(footer_line, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 0));
	auto* footer_buttons = new wxBoxSizer(wxHORIZONTAL);
	footer_buttons->AddStretchSpacer(1);
	result_count_label_ = new wxStaticText(footer_panel, wxID_ANY, "Results: 0 |");
	result_count_label_->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	result_count_label_->SetFont(Theme::GetFont(9, false));
	footer_buttons->Add(result_count_label_, wxSizerFlags().CenterVertical().Border(wxRIGHT, 8));
	if (action_set_ == ActionSet::SearchAndSelect) {
		search_map_button_ = new wxButton(footer_panel, wxID_FIND, "Search Map");
		footer_buttons->Add(search_map_button_, wxSizerFlags().Border(wxRIGHT, 8));

		select_item_button_ = new wxButton(footer_panel, wxID_OK, "Select Item");
		footer_buttons->Add(select_item_button_, wxSizerFlags().Border(wxRIGHT, 8));
	} else {
		ok_button_ = new wxButton(footer_panel, wxID_OK, "OK");
		footer_buttons->Add(ok_button_, wxSizerFlags().Border(wxRIGHT, 8));
	}

	cancel_button_ = new wxButton(footer_panel, wxID_CANCEL, "Cancel");
	footer_buttons->Add(cancel_button_, 0);
	footer_sizer->Add(footer_buttons, wxSizerFlags().Expand().Border(wxALL, 10));
	footer_panel->SetSizer(footer_sizer);
	root_sizer->Add(footer_panel, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 12));

	SetSizer(root_sizer);
	root_sizer->SetSizeHints(this);
}

void FindItemDialog::bindEvents() {
	search_field_->Bind(wxEVT_TEXT, &FindItemDialog::OnTextChanged, this);
	search_field_->Bind(wxEVT_TEXT_ENTER, &FindItemDialog::OnTextEnter, this);
	Bind(wxEVT_KEY_DOWN, &FindItemDialog::OnKeyDown, this);
	Bind(wxEVT_TIMER, &FindItemDialog::OnDeferredSelectTimer, this, deferred_select_timer_.GetId());

	for (wxCheckBox* checkbox : type_checkboxes_) {
		checkbox->Bind(wxEVT_CHECKBOX, &FindItemDialog::OnFilterChanged, this);
	}
	for (wxCheckBox* checkbox : property_checkboxes_) {
		checkbox->Bind(wxEVT_CHECKBOX, &FindItemDialog::OnFilterChanged, this);
	}
	for (wxCheckBox* checkbox : interaction_checkboxes_) {
		checkbox->Bind(wxEVT_CHECKBOX, &FindItemDialog::OnFilterChanged, this);
	}
	for (wxCheckBox* checkbox : visual_checkboxes_) {
		checkbox->Bind(wxEVT_CHECKBOX, &FindItemDialog::OnFilterChanged, this);
	}

	if (results_notebook_ != nullptr) {
		results_notebook_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &FindItemDialog::OnResultPageChanged, this);
	}
	if (list_results_view_ != nullptr) {
		list_results_view_->Bind(wxEVT_LISTBOX, &FindItemDialog::OnResultSelection, this);
		list_results_view_->Bind(wxEVT_LISTBOX_DCLICK, &FindItemDialog::OnResultActivate, this);
		list_results_view_->Bind(EVT_ADVANCED_FINDER_RESULT_RIGHT_ACTIVATE, &FindItemDialog::OnResultRightActivate, this);
	}
	if (grid_results_view_ != nullptr) {
		grid_results_view_->Bind(wxEVT_LISTBOX, &FindItemDialog::OnResultSelection, this);
		grid_results_view_->Bind(wxEVT_LISTBOX_DCLICK, &FindItemDialog::OnResultActivate, this);
		grid_results_view_->Bind(EVT_ADVANCED_FINDER_RESULT_RIGHT_ACTIVATE, &FindItemDialog::OnResultRightActivate, this);
	}

	if (search_map_button_ != nullptr) {
		search_map_button_->Bind(wxEVT_BUTTON, &FindItemDialog::OnSearchMap, this);
	}
	if (select_item_button_ != nullptr) {
		select_item_button_->Bind(wxEVT_BUTTON, &FindItemDialog::OnSelectItem, this);
	}
	if (ok_button_ != nullptr) {
		ok_button_->Bind(wxEVT_BUTTON, &FindItemDialog::OnSelectItem, this);
	}
	if (reset_search_button_ != nullptr) {
		reset_search_button_->Bind(wxEVT_BUTTON, &FindItemDialog::OnResetSearch, this);
	}
	cancel_button_->Bind(wxEVT_BUTTON, &FindItemDialog::OnCancel, this);
}

void FindItemDialog::loadInitialState() {
	catalog_ = BuildAdvancedFinderCatalog(include_creatures_);
	query_ = {};
	current_selection_ = {};

	if (persist_shared_state_) {
		if (g_session_finder_state.has_value()) {
			persisted_state_ = g_session_finder_state->persisted;
			result_view_mode_ = g_session_finder_state->result_view_mode;
		} else {
			persisted_state_ = LoadAdvancedFinderPersistedState();
			result_view_mode_ = (persisted_state_.view_mode == 1) ? AdvancedFinderResultViewMode::Grid : AdvancedFinderResultViewMode::List;
		}
		query_ = persisted_state_.query;
		current_selection_ = persisted_state_.selection;
		if (persisted_state_.size.IsFullySpecified()) {
			SetSize(persisted_state_.size);
		}
		if (persisted_state_.position != wxDefaultPosition) {
			Move(persisted_state_.position);
		} else {
			Centre(wxBOTH);
		}
	} else {
		SetSize(FromDIP(wxSize(1500, 900)));
		Centre(wxBOTH);
	}

	if (only_pickupables_) {
		query_.interaction_mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Pickupable);
	}

	if (list_results_view_) {
		list_results_view_->SetIconSize(persisted_state_.icon_size);
		list_results_view_->SetGridSpacing(persisted_state_.grid_spacing);
		list_results_view_->SetBrushIndicators(persisted_state_.indicator_dogear);
		list_results_view_->SetShowLabels(persisted_state_.show_labels);
	}
	if (grid_results_view_) {
		grid_results_view_->SetIconSize(persisted_state_.icon_size);
		grid_results_view_->SetGridSpacing(persisted_state_.grid_spacing);
		grid_results_view_->SetBrushIndicators(persisted_state_.indicator_dogear);
		grid_results_view_->SetShowLabels(persisted_state_.show_labels);
	}
	if (grid_toolbar_) {
		grid_toolbar_->SetState(
			persisted_state_.show_labels,
			persisted_state_.icon_size,
			false,
			TilesetSortKey::ID,
			TilesetSortDirection::Ascending,
			false,
			result_view_mode_ == AdvancedFinderResultViewMode::List
		);
		grid_toolbar_->SetIndicatorState(persisted_state_.indicator_dogear);
		grid_toolbar_->SetGridSpacing(persisted_state_.grid_spacing);
	}

	applyQueryToControls();
	setResultViewMode(result_view_mode_);
	refreshResults();

	if (action_set_ == ActionSet::SearchAndSelect) {
		if (default_action_ == AdvancedFinderDefaultAction::SearchMap) {
			search_map_button_->SetDefault();
		} else {
			select_item_button_->SetDefault();
		}
	} else if (ok_button_ != nullptr) {
		ok_button_->SetDefault();
	}

	search_field_->SetFocus();
	search_field_->SetSelection(-1, -1);
}

void FindItemDialog::savePersistedState() {
	if (!persist_shared_state_) {
		return;
	}

	auto state = persisted_state_;
	state.query = query_;
	state.selection = current_selection_;
	state.position = GetPosition();
	state.size = GetSize();
	state.view_mode = (result_view_mode_ == AdvancedFinderResultViewMode::Grid) ? 1 : 0;
	if (grid_results_view_) {
		state.icon_size = grid_results_view_->GetIconSize();
		state.grid_spacing = grid_results_view_->GetGridSpacing();
		state.indicator_dogear = grid_results_view_->GetBrushIndicators();
		state.show_labels = grid_results_view_->GetShowLabels();
	}
	g_session_finder_state = SessionFinderState {
		.persisted = state,
		.result_view_mode = result_view_mode_,
	};

	SaveAdvancedFinderPersistedState(state);
}

void FindItemDialog::applyQueryToControls() const {
	search_field_->ChangeValue(wxstr(query_.text));

	for (const auto& [type_filter, _] : kTypeCheckboxes) {
		type_checkboxes_[findItemDialogIndex(type_filter)]->SetValue((query_.type_mask & advancedFinderBit(type_filter)) != 0);
	}
	for (const auto& [property_filter, _] : kPropertyCheckboxes) {
		property_checkboxes_[findItemDialogIndex(property_filter)]->SetValue((query_.property_mask & advancedFinderBit(property_filter)) != 0);
	}
	for (const auto& [interaction_filter, _] : kInteractionCheckboxes) {
		interaction_checkboxes_[findItemDialogIndex(interaction_filter)]->SetValue((query_.interaction_mask & advancedFinderBit(interaction_filter)) != 0);
	}
	for (const auto& [visual_filter, _] : kVisualCheckboxes) {
		visual_checkboxes_[findItemDialogIndex(visual_filter)]->SetValue((query_.visual_mask & advancedFinderBit(visual_filter)) != 0);
	}

	if (only_pickupables_) {
		auto* pickupable = interaction_checkboxes_[findItemDialogIndex(AdvancedFinderInteractionFilter::Pickupable)];
		pickupable->SetValue(true);
		pickupable->Enable(false);
	}

	if (!include_creatures_) {
		auto* creature_checkbox = type_checkboxes_[findItemDialogIndex(AdvancedFinderTypeFilter::Creature)];
		creature_checkbox->SetValue(false);
		creature_checkbox->Enable(false);
	}
}

void FindItemDialog::readQueryFromControls() {
	query_.text = nstr(search_field_->GetValue());

	query_.type_mask = 0;
	query_.property_mask = 0;
	query_.interaction_mask = 0;
	query_.visual_mask = 0;

	for (const auto& [type_filter, _] : kTypeCheckboxes) {
		if (type_checkboxes_[findItemDialogIndex(type_filter)]->GetValue()) {
			query_.type_mask |= advancedFinderBit(type_filter);
		}
	}
	for (const auto& [property_filter, _] : kPropertyCheckboxes) {
		if (property_checkboxes_[findItemDialogIndex(property_filter)]->GetValue()) {
			query_.property_mask |= advancedFinderBit(property_filter);
		}
	}
	for (const auto& [interaction_filter, _] : kInteractionCheckboxes) {
		if (interaction_checkboxes_[findItemDialogIndex(interaction_filter)]->GetValue()) {
			query_.interaction_mask |= advancedFinderBit(interaction_filter);
		}
	}
	for (const auto& [visual_filter, _] : kVisualCheckboxes) {
		if (visual_checkboxes_[findItemDialogIndex(visual_filter)]->GetValue()) {
			query_.visual_mask |= advancedFinderBit(visual_filter);
		}
	}

	if (only_pickupables_) {
		query_.interaction_mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Pickupable);
	}
	if (!include_creatures_) {
		query_.type_mask &= ~advancedFinderBit(AdvancedFinderTypeFilter::Creature);
	}
}

void FindItemDialog::updateResultTitle(size_t count) {
	if (result_count_label_ != nullptr) {
		result_count_label_->SetLabelText(wxString::FromUTF8(std::format("Results: {} |", count)));
		result_count_label_->SetMinSize(result_count_label_->GetBestSize());
		if (wxWindow* parent = result_count_label_->GetParent(); parent != nullptr) {
			parent->Layout();
		}
		Layout();
	}
}
