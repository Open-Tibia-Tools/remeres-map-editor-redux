#ifndef RME_PREFERENCES_MAP_COMPATIBILITY_PAGE_H
#define RME_PREFERENCES_MAP_COMPATIBILITY_PAGE_H

#include <wx/choice.h>

#include "preferences_page.h"

class MapCompatibilityPage : public ScrollablePreferencesPage {
public:
	explicit MapCompatibilityPage(wxWindow* parent);
	void Apply() override;

private:
	wxChoice* waypoint_storage_choice = nullptr;
};

#endif
