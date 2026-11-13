//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LAYOUT_MANAGER_H_
#define RME_LAYOUT_MANAGER_H_

#include "app/main.h"
#include <algorithm>
#include <optional>
#include <string>

namespace rme::ui::layout {

	struct PaneRect {
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	// Moves a pane rectangle inside a display rectangle.
	// Returns nullopt when the pane cannot fit on the display.
	constexpr std::optional<PaneRect> FitPaneInDisplay(PaneRect pane, PaneRect display) {
		if (pane.width > display.width || pane.height > display.height) {
			return std::nullopt;
		}
		pane.x = std::clamp(pane.x, display.x, display.x + display.width - pane.width);
		pane.y = std::clamp(pane.y, display.y, display.y + display.height - pane.height);
		return pane;
	}

} // namespace rme::ui::layout

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
