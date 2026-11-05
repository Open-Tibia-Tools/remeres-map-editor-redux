#ifndef RME_MINIMAP_WINDOW_H_
#define RME_MINIMAP_WINDOW_H_

#include <wx/glcanvas.h>
#include <wx/panel.h>

#include <memory>

class Editor;
class MapCanvas;
class MapWindow;
class MinimapDrawer;
class MinimapCanvas;
class wxButton;
class wxFileName;
class wxPanel;
class wxStaticText;
class wxToggleButton;

struct MinimapViewportState;

class MinimapWindow : public wxPanel {
public:
	MinimapWindow(wxWindow* parent);
	~MinimapWindow() override;

	void DelayedUpdate();
	void RefreshMinimap(bool immediate = true);
	void SyncHeaderState();

	void StepZoom(int delta);
	void StepFloor(int delta);
	void SetShowAllFloors(bool show_all_floors);
	void JumpCameraTo(int map_x, int map_y);

	Editor* GetActiveEditor() const;
	MapCanvas* GetActiveCanvas() const;
	MapWindow* GetActiveMapWindow() const;
	MinimapViewportState* GetActiveViewportState() const;
	MinimapViewportState* EnsureActiveViewportState();

private:
	void OnClose(wxCloseEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnZoomOut(wxCommandEvent& event);
	void OnZoomIn(wxCommandEvent& event);
	void OnFloorUp(wxCommandEvent& event);
	void OnFloorDown(wxCommandEvent& event);
	void OnShowAllFloorsToggle(wxCommandEvent& event);
	void OnScreenshot(wxCommandEvent& event);
	void OnHelpEnter(wxMouseEvent& event);
	void OnHelpLeave(wxMouseEvent& event);
	void OnHelpClick(wxCommandEvent& event);
	void UpdateHelpVisibility();
	void PositionHelpPanel();

	MinimapCanvas* canvas_ = nullptr;
	wxButton* zoom_out_button_ = nullptr;
	wxButton* zoom_in_button_ = nullptr;
	wxButton* floor_up_button_ = nullptr;
	wxButton* floor_down_button_ = nullptr;
	wxButton* screenshot_button_ = nullptr;
	wxButton* help_button_ = nullptr;
	wxToggleButton* show_all_floors_button_ = nullptr;
	wxStaticText* zoom_label_ = nullptr;
	wxStaticText* floor_label_ = nullptr;
	wxPanel* help_panel_ = nullptr;
	wxStaticText* help_text_ = nullptr;

	bool help_hovered_ = false;
	bool help_pinned_ = false;
};

class MinimapCanvas : public wxGLCanvas {
public:
	explicit MinimapCanvas(MinimapWindow* parent);
	~MinimapCanvas() override;

	void DelayedUpdate();
	void NormalizeViewportState();
	[[nodiscard]] bool SaveCleanScreenshot(const wxFileName& file);

private:
	void OnPaint(wxPaintEvent& event);
	void OnEraseBackground(wxEraseEvent&) {
	}
	void OnDelayedUpdate(wxTimerEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnLeftDown(wxMouseEvent& event);
	void OnLeftUp(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
	void OnMouseEnter(wxMouseEvent& event);
	void OnMouseLeave(wxMouseEvent& event);
	void OnKey(wxKeyEvent& event);

	void ClampViewportState(MinimapViewportState& state) const;
	void StopInteraction();

	MinimapWindow* owner_ = nullptr;
	std::unique_ptr<MinimapDrawer> drawer;
	wxTimer update_timer;
	std::unique_ptr<wxGLContext> m_glContext;

	bool is_dragging_ = false;
	bool ctrl_click_pending_ = false;
	wxPoint drag_origin_;
	double drag_origin_center_x_ = 0.0;
	double drag_origin_center_y_ = 0.0;
};

#endif
