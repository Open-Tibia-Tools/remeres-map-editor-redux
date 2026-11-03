#ifndef RME_THEME_H_
#define RME_THEME_H_

#include <wx/wx.h>
#include <wx/settings.h>

class Brush;

class Theme {
public:
	enum class Role {
		Surface, // Main panel background
		Background, // Darker backgrounds (grids, lists)
		Header, // Even darker for headers
		Accent, // Primary action / Highlight (Blue)
		AccentHover, // Lighter accent
		Text, // Primary text
		TextSubtle, // De-emphasized text
		TextOnAccent, // Text color when on accented background (usually white)
		Border, // Outline/Separator colors
		Selected, // Selection fill
		Error, // Warning/Error states
		Warning,
		Success,
		CardBase, // Rule card background
		CardBaseHover, // Rule card hover background
		CardBorder, // Rule card default border
		PanelBackground,
		RaisedSurface,
		FooterSurface,
		SelectionFill,
		PrimaryButton,
		PrimaryButtonHover,
		SecondaryButton,
		SecondaryButtonHover,
		ButtonDisabled,

		// Tooltip field value colors
		TooltipBg, // Tooltip background
		TooltipLabel, // Tooltip label text
		TooltipActionId, // Action ID value
		TooltipUniqueId, // Unique ID value
		TooltipDoorId, // Door ID value
		TooltipTextValue, // Readable text (signs, books)
		TooltipTeleport, // Teleport destination
		TooltipWaypoint, // Waypoint name
		TooltipBodyText, // Description / general body text
		TooltipCountText, // Container item count text

		TooltipBorderWaypoint,
		TooltipBorderItem,
		TooltipBorderDoor,
		TooltipBorderTeleport,
		TooltipBorderText,

		// Server markers on sector maps (see game/srv_markers.h): tile frame, badge and tooltip border
		ServerMarkerStart,
		ServerMarkerDepot,
		ServerMarkerRaid,
		ServerMarkerScript,

		// Brush Category Colors
		BrushCategoryGround,
		BrushCategoryWall,
		BrushCategoryWallDecoration,
		BrushCategoryDoor,
		BrushCategoryDoodad,
		BrushCategoryTable,
		BrushCategoryCarpet,
		BrushCategoryDefaultRaw
	};

	enum class Type {
		System = 0,
		Dark = 1,
		Light = 2
	};

	static void setType(Type type);
	static Type getType();

	static wxColour Get(Role role);
	static Role GetBrushCategoryRole(const Brush* brush);
	static wxColour GetBrushColour(const Brush* brush) {
		return Get(GetBrushCategoryRole(brush));
	}

	static int Grid(int units) {
		return wxWindow::FromDIP(units * 4, nullptr);
	}

	static wxFont GetFont(int pointSize = 9, bool bold = false) {
		wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
		font.SetPointSize(wxWindow::FromDIP(pointSize, nullptr));
		if (bold) {
			font.MakeBold();
		}
		return font;
	}

private:
	static Type current_type;
	static wxColour GetDark(Role role);
	static wxColour GetLight(Role role);
};

#endif
