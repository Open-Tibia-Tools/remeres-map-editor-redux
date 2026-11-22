#ifndef RME_UI_CONTEXT_MENU_PRESENTATION_H_
#define RME_UI_CONTEXT_MENU_PRESENTATION_H_

#include <optional>
#include <string_view>

namespace ContextMenu {

	// Presentation of one MAP_POPUP_MENU_* command. Shared labels, help texts and icons live here so
	// both context menus stay identical where they overlap; a menu may override row-scoped wording.
	// Each model only maps its own command to the shared menu id.
	struct EntryPresentation {
		int id = 0;
		const char* label = "";
		const char* help = "";
		std::string_view icon;
	};

	// Returns the presentation for `menu_id`, or std::nullopt when the entry has no fixed
	// presentation (door open/close depends on the door state and is built by each menu).
	[[nodiscard]] std::optional<EntryPresentation> PresentMenuEntry(int menu_id);

} // namespace ContextMenu

#endif
