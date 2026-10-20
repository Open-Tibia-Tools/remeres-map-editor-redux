#include "app/main.h"
#include "ui/replace_tool/rule_chance_dialog.h"
#include "ui/theme.h"
#include "item_definitions/core/item_definition_store.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include <algorithm>
#include <numeric>
#include <vector>

namespace {

	constexpr int kMaxChance = kChanceMax;

	wxString TargetLabel(uint16_t id) {
		const auto definition = g_item_definitions.get(id);
		const std::string name = definition ? std::string(definition.name()) : std::string();
		return name.empty() ? wxstr(std::format("{}", id)) : wxstr(std::format("{} - {}", id, name));
	}

} // namespace

RuleChanceDialog::RuleChanceDialog(wxWindow* parent, const std::vector<ReplacementTarget>& targets) :
	wxDialog(parent, wxID_ANY, "Replace chances", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
	m_targets(targets) {

	int initialSum = 0;
	for (const auto& target : m_targets) {
		initialSum += target.probability;
	}
	if (initialSum > kMaxChance && initialSum > 0) {
		for (auto& target : m_targets) {
			target.probability = std::clamp((target.probability * kMaxChance) / initialSum, 0, kMaxChance);
		}
	}

	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	auto* root = new wxBoxSizer(wxVERTICAL);
	const int padding = Theme::Grid(2);

	auto* grid = new wxFlexGridSizer(3, Theme::Grid(2), Theme::Grid(1));
	grid->AddGrowableCol(1, 1);

	for (size_t i = 0; i < m_targets.size(); ++i) {
		auto* label = new wxStaticText(this, wxID_ANY, TargetLabel(m_targets[i].id));
		label->SetForegroundColour(Theme::Get(Theme::Role::Text));
		label->SetFont(Theme::GetFont(9, false));
		grid->Add(label, wxSizerFlags(0).CenterVertical());

		auto* slider = new wxSlider(this, wxID_ANY, m_targets[i].probability, 0, kMaxChance, wxDefaultPosition, FromDIP(wxSize(220, -1)));
		slider->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& event) {
			OnSliderChanged(static_cast<size_t>(i));
			event.Skip();
		});
		m_sliders.push_back(slider);
		grid->Add(slider, wxSizerFlags(1).CenterVertical().Expand());

		auto* value = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(48, -1)), wxALIGN_RIGHT);
		value->SetForegroundColour(Theme::Get(Theme::Role::Text));
		value->SetFont(Theme::GetFont(9, true));
		m_values.push_back(value);
		grid->Add(value, wxSizerFlags(0).CenterVertical());
	}

	root->Add(grid, wxSizerFlags(1).Expand().Border(wxALL, padding));

	m_totalLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
	m_totalLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	m_totalLabel->SetFont(Theme::GetFont(8, false));
	root->Add(m_totalLabel, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, padding));

	auto* buttons = new wxBoxSizer(wxHORIZONTAL);
	auto* evenly = new wxButton(this, wxID_ANY, "Distribute evenly");
	evenly->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DistributeEvenly(); });
	buttons->Add(evenly, wxSizerFlags(0).CenterVertical());

	buttons->AddStretchSpacer(1);

	auto* cancel = new wxButton(this, wxID_CANCEL, "Cancel");
	buttons->Add(cancel, wxSizerFlags(0).CenterVertical().Border(wxRIGHT, Theme::Grid(1)));
	auto* ok = new wxButton(this, wxID_OK, "OK");
	ok->SetDefault();
	buttons->Add(ok, wxSizerFlags(0).CenterVertical());

	root->Add(buttons, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, padding));

	SetSizerAndFit(root);
	RefreshTotals();
	Centre();
}

std::vector<ReplacementTarget> RuleChanceDialog::GetTargets() const {
	std::vector<ReplacementTarget> result = m_targets;
	for (size_t i = 0; i < result.size() && i < m_sliders.size(); ++i) {
		result[i].probability = m_sliders[i]->GetValue();
	}
	return result;
}

void RuleChanceDialog::OnSliderChanged(size_t index) {
	if (index >= m_sliders.size()) {
		return;
	}

	// Every slider spans the same 0..100 range, so equal values sit at equal
	// positions. The budget is enforced by clamping the value instead.
	const int ceiling = std::max(0, kMaxChance - ChanceOfOthers(index));
	if (m_sliders[index]->GetValue() > ceiling) {
		m_sliders[index]->SetValue(ceiling);
	}

	RefreshTotals();
}

void RuleChanceDialog::RefreshTotals() {
	int total = 0;
	for (size_t i = 0; i < m_sliders.size(); ++i) {
		const int value = m_sliders[i]->GetValue();
		total += value;
		if (i < m_values.size()) {
			m_values[i]->SetLabel(wxstr(FormatChance(value)));
		}
	}

	if (!m_totalLabel) {
		return;
	}

	wxString text = wxstr(std::format("Total {}", FormatChance(total)));
	if (total < kMaxChance) {
		text += wxstr(std::format(" - {} of matches change, {} left to give.", FormatChance(total), FormatChance(kMaxChance - total)));
	} else {
		text += " - every match changes, split between the targets above.";
	}
	m_totalLabel->SetLabel(text);
	Layout();
}

int RuleChanceDialog::ChanceOfOthers(size_t index) const {
	int sum = 0;
	for (size_t i = 0; i < m_sliders.size(); ++i) {
		if (i != index) {
			sum += m_sliders[i]->GetValue();
		}
	}
	return sum;
}

void RuleChanceDialog::DistributeEvenly() {
	if (m_sliders.empty()) {
		return;
	}

	const int step = kMaxChance / static_cast<int>(m_sliders.size());
	int remainder = kMaxChance - step * static_cast<int>(m_sliders.size());
	for (wxSlider* slider : m_sliders) {
		slider->SetValue(step + (remainder-- > 0 ? 1 : 0));
	}
	RefreshTotals();
}
