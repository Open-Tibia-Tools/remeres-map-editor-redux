//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/dialogs/server_number_search_dialog.h"

#include "ui/theme.h"
#include "util/image_manager.h"

#include <wx/spinctrl.h>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <optional>
#include <string_view>

namespace {

	using rme::srv::ItemAttribute;

	struct SearchKind {
		std::string_view label;
		std::string_view title; // "Quest 64"
		ItemAttribute attribute;
		std::optional<ItemAttribute> second_attribute;
		bool include_scripts;
	};

	// Quest numbers sit on quest chests and quest doors, and moveuse.dat
	// rules set or test them; a key number opens the doors whose keyhole
	// has the same number.
	constexpr std::array<SearchKind, 3> kKinds { {
		{ "Quest number (quest chests, quest doors and scripts)", "Quest", ItemAttribute::ChestQuestNumber, ItemAttribute::DoorQuestNumber, true },
		{ "Key number (keys and the doors they open)", "Key", ItemAttribute::KeyNumber, ItemAttribute::KeyholeNumber, false },
		{ "Door level (level doors)", "Door level", ItemAttribute::DoorLevel, std::nullopt, false },
	} };

	// Kept for the next search in this session.
	int g_last_kind = 0;
	int g_last_value = 1;

} // namespace

ServerNumberSearchDialog::ServerNumberSearchDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Find Quest, Key or Level", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	auto* top_sizer = new wxBoxSizer(wxVERTICAL);

	auto* grid_sizer = new wxFlexGridSizer(2, Theme::Grid(2), Theme::Grid(3));
	grid_sizer->AddGrowableCol(1);

	kind_choice = new wxChoice(this, wxID_ANY);
	for (const SearchKind& kind : kKinds) {
		kind_choice->Append(wxstr(kind.label));
	}
	kind_choice->SetSelection(g_last_kind);

	value_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, std::numeric_limits<int>::max(), g_last_value);

	grid_sizer->Add(new wxStaticText(this, wxID_ANY, "Find:"), wxSizerFlags().CenterVertical());
	grid_sizer->Add(kind_choice, wxSizerFlags().Expand());
	grid_sizer->Add(new wxStaticText(this, wxID_ANY, "Number:"), wxSizerFlags().CenterVertical());
	grid_sizer->Add(value_spin, wxSizerFlags().Expand());
	top_sizer->Add(grid_sizer, wxSizerFlags().Expand().Border(wxALL, Theme::Grid(4)));

	auto* button_sizer = new wxBoxSizer(wxHORIZONTAL);
	auto* ok_button = new wxButton(this, wxID_OK, "Search");
	ok_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_SEARCH));
	ok_button->SetDefault();
	auto* cancel_button = new wxButton(this, wxID_CANCEL, "Cancel");
	cancel_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	button_sizer->Add(ok_button, wxSizerFlags().Center().Border(wxRIGHT, Theme::Grid(2)));
	button_sizer->Add(cancel_button, wxSizerFlags().Center());
	top_sizer->Add(button_sizer, wxSizerFlags().Right().Border(wxLEFT | wxRIGHT | wxBOTTOM, Theme::Grid(4)));

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);
	value_spin->SetFocus();

	Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
		g_last_kind = kind_choice->GetSelection();
		g_last_value = value_spin->GetValue();
		event.Skip();
	}, wxID_OK);
}

ServerNumberFilter ServerNumberSearchDialog::getFilter() const {
	const SearchKind& kind = kKinds[static_cast<size_t>(std::max(0, kind_choice->GetSelection()))];
	ServerNumberFilter filter;
	filter.attributes.push_back(kind.attribute);
	if (kind.second_attribute) {
		filter.attributes.push_back(*kind.second_attribute);
	}
	filter.value = value_spin->GetValue();
	filter.include_scripts = kind.include_scripts;
	return filter;
}

std::string ServerNumberSearchDialog::getTitle() const {
	const SearchKind& kind = kKinds[static_cast<size_t>(std::max(0, kind_choice->GetSelection()))];
	return std::format("Find {} {} (Map)", kind.title, value_spin->GetValue());
}
