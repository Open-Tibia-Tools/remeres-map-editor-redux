#ifndef RME_UI_PROPERTIES_SPAWN_ALTERNATIVES_EDITOR_H_
#define RME_UI_PROPERTIES_SPAWN_ALTERNATIVES_EDITOR_H_

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <functional>
#include <vector>
#include <string>

class Creature;

struct SpawnMonsterVariant {
	std::string name;
	uint32_t chance = 0;
};

class SpawnAlternativesEditor : public wxPanel {
public:
	SpawnAlternativesEditor(wxWindow* parent);
	virtual ~SpawnAlternativesEditor() = default;

	void SetCreature(Creature* creature);
	void ApplyToCreature(Creature* creature);

	bool HasAlternatives() const {
		return variants.size() > 1;
	}

	const std::vector<SpawnMonsterVariant>& GetVariants() const {
		return variants;
	}

	void SetOnAlternativesChanged(std::function<void(bool)> cb) {
		on_alternatives_changed_cb = std::move(cb);
	}

	void SetOnDataChanged(std::function<void()> cb) {
		on_data_changed_cb = std::move(cb);
	}

private:
	void RefreshList();
	void UpdateTotalChanceLabel();
	void UpdateButtonStates();

	void OnItemSelected(wxListEvent& event);
	void OnItemDeselected(wxListEvent& event);
	void OnChanceChanged(wxSpinEvent& event);
	void OnClickAdd(wxCommandEvent& event);
	void OnClickRemove(wxCommandEvent& event);

	wxListCtrl* list_ctrl;
	wxButton* add_btn;
	wxButton* remove_btn;
	wxSpinCtrl* chance_spin;
	wxStaticText* total_chance_label;

	std::vector<SpawnMonsterVariant> variants;
	int selected_index = -1;

	std::function<void(bool)> on_alternatives_changed_cb;
	std::function<void()> on_data_changed_cb;
};

#endif // RME_UI_PROPERTIES_SPAWN_ALTERNATIVES_EDITOR_H_
