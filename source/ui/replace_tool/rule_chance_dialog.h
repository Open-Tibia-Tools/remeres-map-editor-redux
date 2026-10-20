#ifndef RME_RULE_CHANCE_DIALOG_H_
#define RME_RULE_CHANCE_DIALOG_H_

#include "app/main.h"
#include "ui/replace_tool/rule_manager.h"

#include <wx/dialog.h>

#include <vector>

class wxSlider;
class wxStaticText;

/**
 * Edits the chance of every target of one rule.
 *
 * Each target rolls its own chance, so the total is a budget rather than a
 * distribution: the dialog will not let the sliders add up past 100%, and the
 * remainder leaves matched items untouched.
 */
class RuleChanceDialog : public wxDialog {
public:
	RuleChanceDialog(wxWindow* parent, const std::vector<ReplacementTarget>& targets);

	std::vector<ReplacementTarget> GetTargets() const;

private:
	void OnSliderChanged(size_t index);
	void RefreshTotals();
	void DistributeEvenly();
	int ChanceOfOthers(size_t index) const;

	std::vector<ReplacementTarget> m_targets;
	std::vector<wxSlider*> m_sliders;
	std::vector<wxStaticText*> m_values;
	wxStaticText* m_totalLabel = nullptr;
};

#endif
