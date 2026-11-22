//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_AUTOBORDER_DEBUG_WINDOW_H_
#define RME_UI_AUTOBORDER_DEBUG_WINDOW_H_

#include "brushes/ground/border_update_policy.h"
#include "map/position.h"

#include <wx/dialog.h>
#include <wx/timer.h>

#include <optional>

class Brush;
class Editor;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxCommandEvent;
class wxStaticText;
class wxTextCtrl;

// Non-modal window that shows, for the tile under the map cursor, what a border
// recalculation with the current ground brush decides: ownership, context brushes,
// the removal verdict of every border item, the generated pieces, the specific
// cases and the resulting stack. Everything is computed on a copy of the tile;
// only "Apply to this tile" changes the map, as one undoable action.
class AutoborderDebugWindow : public wxDialog {
public:
	// Shows the window, creating it on first use, or hides it when shown.
	static void Toggle(wxWindow* parent);

	~AutoborderDebugWindow() override;

private:
	explicit AutoborderDebugWindow(wxWindow* parent);

	void OnTimer(wxTimerEvent& event);
	void OnOptionChanged(wxCommandEvent& event);
	void OnLockChanged(wxCommandEvent& event);
	void OnApply(wxCommandEvent& event);
	void OnCopy(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);

	void StartTracking();
	void StopTracking();
	void UpdateReport();
	[[nodiscard]] std::optional<Position> TargetPosition() const;
	[[nodiscard]] BorderUpdate::Options CurrentOptions(const Brush* brush) const;
	void SyncChoicesFromOptions(const BorderUpdate::Options& options);

	static AutoborderDebugWindow* instance;

	wxCheckBox* follow_settings = nullptr;
	wxChoice* scope_choice = nullptr;
	wxChoice* placement_choice = nullptr;
	wxCheckBox* lock_tile = nullptr;
	wxButton* apply_button = nullptr;
	wxStaticText* summary = nullptr;
	wxTextCtrl* report = nullptr;
	wxTimer timer;

	std::optional<Position> locked_position;
	wxString last_report;
};

#endif // RME_UI_AUTOBORDER_DEBUG_WINDOW_H_
