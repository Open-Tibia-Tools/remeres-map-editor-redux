#ifndef RME_UI_TOOL_OPTIONS_SURFACE_H_
#define RME_UI_TOOL_OPTIONS_SURFACE_H_

#include "app/main.h"
#include "palette/palette_common.h"

#include <string_view>
#include <vector>
#include <wx/wx.h>

class Brush;
class wxBitmapToggleButton;
class wxSlider;
class wxGridSizer;
class wxSpinCtrl;

class ToolOptionsSurface : public wxPanel {
public:
	ToolOptionsSurface(wxWindow* parent);
	~ToolOptionsSurface() override = default;

	void SetActiveBrush(Brush* brush);
	void UpdateBrushSize(BrushShape shape, int size);
	void ReloadSettings();
	void Clear();

private:
	enum class ToolButtonAction {
		SelectBrush,
		SelectCreature,
		SelectSpawn,
	};

	struct ToolButtonEntry {
		ToolButtonAction action = ToolButtonAction::SelectBrush;
		Brush* brush = nullptr;
		wxBitmapToggleButton* button = nullptr;
		std::string_view asset_path;
	};

	void BuildUi();
	void EnsureToolButtons();
	void RebuildToolButtons();
	void RefreshFromState();
	void UpdateSectionVisibility();
	void UpdateSizeLabels();
	void UpdateModeButtons();
	void SyncToolSelection();
	void SetMutatingUi(bool value);
	[[nodiscard]] bool IsMutatingUi() const;
	[[nodiscard]] bool IsCreatureToolMode() const;
	[[nodiscard]] bool HasBrushSizeControls() const;
	[[nodiscard]] bool HasThicknessControl() const;
	[[nodiscard]] bool HasPreviewBorderControl() const;
	[[nodiscard]] bool HasLockDoorsControl() const;
	[[nodiscard]] bool HasSpawnControls() const;
	[[nodiscard]] bool HasWeightControl() const;
	[[nodiscard]] Brush* GetSelectedCreatureBrush() const;
	[[nodiscard]] std::vector<Brush*> GetDefaultTools() const;
	[[nodiscard]] wxBitmap CreateToolBitmap(const ToolButtonEntry& entry) const;
	[[nodiscard]] wxBitmap CreateBrushBitmap(Brush* brush) const;
	[[nodiscard]] wxBitmap CreateModeBitmap(std::string_view assetPath, const wxColour& tint) const;

	void OnToolButton(wxCommandEvent& event);
	void OnSizeXChanged(wxCommandEvent& event);
	void OnSizeYChanged(wxCommandEvent& event);
	void OnExactToggled(wxCommandEvent& event);
	void OnAspectToggled(wxCommandEvent& event);
	void OnPreviewBorderToggled(wxCommandEvent& event);
	void OnLockDoorsToggled(wxCommandEvent& event);
	void OnThicknessChanged(wxCommandEvent& event);
	void OnPlaceSpawnWithCreatureToggled(wxCommandEvent& event);
	void OnSpawnTimeChanged(wxSpinEvent& event);
	void OnSpawnSizeChanged(wxSpinEvent& event);
	void OnWeightChanged(wxSpinEvent& event);

	Brush* active_brush = nullptr;
	bool mutating_ui = false;

	wxBoxSizer* main_sizer = nullptr;
	wxStaticBoxSizer* main_tools_sizer = nullptr;
	wxGridSizer* main_tools_grid = nullptr;
	wxStaticBoxSizer* size_sizer = nullptr;
	wxStaticBoxSizer* other_sizer = nullptr;
	wxPanel* thickness_panel = nullptr;
	wxPanel* spawn_time_panel = nullptr;
	wxPanel* weight_panel = nullptr;
	wxCheckBox* place_spawn_with_creature_checkbox = nullptr;
	wxPanel* spawn_size_panel = nullptr;

	wxSlider* size_x_slider = nullptr;
	wxSlider* size_y_slider = nullptr;
	wxStaticText* size_x_value = nullptr;
	wxStaticText* size_y_value = nullptr;
	wxBitmapToggleButton* exact_button = nullptr;
	wxBitmapToggleButton* aspect_button = nullptr;
	wxCheckBox* preview_border_checkbox = nullptr;
	wxCheckBox* lock_doors_checkbox = nullptr;
	wxStaticText* thickness_label = nullptr;
	wxSlider* thickness_slider = nullptr;
	wxStaticText* thickness_value = nullptr;
	wxStaticText* spawn_time_label = nullptr;
	wxSpinCtrl* spawn_time_spin = nullptr;
	wxStaticText* spawn_size_label = nullptr;
	wxSpinCtrl* spawn_size_spin = nullptr;
	wxStaticText* weight_label = nullptr;
	wxSpinCtrl* weight_spin = nullptr;

	std::vector<ToolButtonEntry> tool_buttons;
};

#endif
