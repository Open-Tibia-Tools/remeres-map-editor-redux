//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_STATUS_MANAGER_H_
#define RME_STATUS_MANAGER_H_

#include "app/main.h"
#include <chrono>

class StatusManager {
public:
	void SetTitle(const wxString& title);
	void UpdateTitle();
	void SetStatusText(const wxString& text, int field = 1, int duration_ms = 3500);
	bool HasActiveMessage() const;

private:
	std::chrono::steady_clock::time_point message_expiry_{};
};

extern StatusManager g_status;

#endif
