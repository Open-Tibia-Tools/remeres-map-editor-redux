#ifndef RME_PALETTE_CONTROLS_PALETTE_SEARCH_CTRL_H_
#define RME_PALETTE_CONTROLS_PALETTE_SEARCH_CTRL_H_

#include <wx/srchctrl.h>

/**
 * @class PaletteSearchCtrl
 * @brief Unified search box for palettes and toolbars.
 *
 * Prevents main-frame keyboard accelerator interception so typing keys like
 * 'U', 'R', 'B', etc. filters the palette rather than triggering map shortcuts.
 * Includes built-in Escape key clearing, cancel button, and theme support.
 */
class PaletteSearchCtrl : public wxSearchCtrl {
public:
	PaletteSearchCtrl(wxWindow* parent, wxWindowID id = wxID_ANY,
		const wxString& value = wxEmptyString,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTE_PROCESS_ENTER);
	~PaletteSearchCtrl() override = default;

	bool IsTopNavigationDomain(NavigationKind kind) const override;

#ifdef __WXMSW__
	bool MSWTranslateMessage(WXMSG* msg) override;
	bool MSWShouldPreProcessMessage(WXMSG* msg) override;
#endif

	void ApplyTheme();
};

#endif // RME_PALETTE_CONTROLS_PALETTE_SEARCH_CTRL_H_
