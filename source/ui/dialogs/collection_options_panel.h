#ifndef RME_UI_DIALOGS_COLLECTION_OPTIONS_PANEL_H_
#define RME_UI_DIALOGS_COLLECTION_OPTIONS_PANEL_H_

#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/button.h>

#include <string>
#include <vector>
#include <functional>

namespace rme::ui {

class CollectionOptionsPanel : public wxPanel {
public:
	struct Callbacks {
		std::function<void()> onAccumulateSelection;
		std::function<void()> onUndo;
		std::function<void()> onRedo;
		std::function<void()> onClearAll;
		std::function<void(const std::string& paletteName, const std::string& name, bool isNew, bool append, bool includeItems, bool includeBrushes)> onSave;
	};

	CollectionOptionsPanel(wxWindow* parent, Callbacks callbacks);
	~CollectionOptionsPanel() override = default;

	void UpdateCounts(size_t itemCount, size_t brushCount);
	void RefreshPalettes();
	void RefreshExistingTilesets();
	void EnableAccumulate(bool enable);
	void UpdateUndoRedo(bool canUndo, bool canRedo);

	[[nodiscard]] bool IsIncludeItems() const {
		return m_includeItemsCheck->GetValue();
	}
	[[nodiscard]] bool IsIncludeBrushes() const {
		return m_includeBrushesCheck->GetValue();
	}
	[[nodiscard]] std::string GetTargetPaletteName() const;
	[[nodiscard]] std::string GetTargetTilesetName() const;
	[[nodiscard]] bool IsCreateNew() const {
		return m_radioNew->GetValue();
	}

	void PerformSave();

private:
	void InitUI();
	void OnPaletteChanged(wxCommandEvent& event);
	void OnModeChanged(wxCommandEvent& event);
	void OnNameChanged(wxCommandEvent& event);
	void OnExistingChanged(wxCommandEvent& event);
	void UpdateFilePreview();
	std::string GenerateDefaultTilesetName() const;

	Callbacks m_callbacks;
	size_t m_itemCount = 0;
	size_t m_brushCount = 0;

	wxChoice* m_paletteChoice = nullptr;
	wxRadioButton* m_radioNew = nullptr;
	wxRadioButton* m_radioExisting = nullptr;
	wxTextCtrl* m_nameInput = nullptr;
	wxChoice* m_existingChoice = nullptr;
	wxCheckBox* m_includeItemsCheck = nullptr;
	wxCheckBox* m_includeBrushesCheck = nullptr;
	wxStaticText* m_filePathLabel = nullptr;
	wxButton* m_accumulateBtn = nullptr;
	wxButton* m_saveBtn = nullptr;
	wxButton* m_undoBtn = nullptr;
	wxButton* m_redoBtn = nullptr;
	wxButton* m_clearBtn = nullptr;
};

} // namespace rme::ui

#endif // RME_UI_DIALOGS_COLLECTION_OPTIONS_PANEL_H_
