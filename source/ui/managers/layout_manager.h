//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LAYOUT_MANAGER_H_
#define RME_LAYOUT_MANAGER_H_

#include "app/main.h"
#include <string>

class PaletteWindow;
class wxAuiPaneInfo;

class LayoutManager {
public:
	LayoutManager();
	~LayoutManager();

	/**
	 * Saves the perspective to the configuration file
	 * This is the position of all windows etc. in the editor
	 */
	void SavePerspective();

	/**
	 * Loads the stored perspective from the configuration file
	 */
	void LoadPerspective();

	static void EnsurePaneOnScreen(wxAuiPaneInfo& info);
	static void UpdatePaletteActivePage(PaletteWindow* palette, const std::string& page_name);

	[[nodiscard]] bool IsLoadingPerspective() const noexcept { return m_isLoadingPerspective; }

private:
	bool m_isLoadingPerspective = false;
};

extern LayoutManager g_layout;

#endif
