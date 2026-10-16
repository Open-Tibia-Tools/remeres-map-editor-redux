//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LOADING_MANAGER_H_
#define RME_LOADING_MANAGER_H_

#include "app/main.h"
#include <wx/progdlg.h>

class LoadingManager {
public:
	LoadingManager();
	~LoadingManager();

	/**
	 * Creates a progress bar with the specified message.
	 * The default scale is 0 - 100.
	 * @param title dialog title, defaults to "Loading" (saving uses "Saving").
	 */
	void CreateLoadBar(wxString message, bool canCancel = false, const wxString& title = "Loading");

	/**
	 * Sets how much of the load has completed, the scale can be set with
	 * SetLoadScale.
	 * If this returns false, the user has hit the quit button and you should
	 * abort the loading.
	 */
	bool SetLoadDone(int32_t done, const wxString& newMessage = "");

	/**
	 * Updates the progress message without changing the completion value.
	 * Meant for phases where no measurable progress is available; the bar
	 * keeps the current percentage (and is never closed by this call).
	 */
	void SetLoadText(const wxString& message);

	/**
	 * Pumps the dialog and refreshes the cancellation state without changing
	 * the progress. Long operations that do not report progress (auxiliary
	 * writes, final steps) call this to observe an Abort click, which is only
	 * reported through an update. Returns true when the user cancelled.
	 */
	bool PollCancellation();

	/**
	 * Sets the scale of the loading bar.
	 * Calling this with (50, 80) means that setting 50 as 'done',
	 * it will display as 0% loaded, 80 will display as 100% loaded.
	 */
	void SetLoadScale(int32_t from, int32_t to);

	/**
	 * Destroys (hides) the current loading bar.
	 */
	void DestroyLoadBar();
	bool IsLoading() const {
		return progressBar != nullptr;
	}

	/**
	 * True when the user pressed the abort button of the current progress bar.
	 * Reset when a new bar is created; long operations poll it to stop early.
	 */
	bool WasCancelled() const {
		return cancelled;
	}

private:
	wxString progressText;
	wxGenericProgressDialog* progressBar;
	bool cancelled;
	// Whether the current bar offers an Abort button; without it there is
	// nothing to poll and repeated values can skip the repaint.
	bool cancellable;

	int32_t progressFrom;
	int32_t progressTo;
	int32_t currentProgress;
};

extern LoadingManager g_loading;

#endif
