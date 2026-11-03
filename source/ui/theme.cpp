#include "ui/theme.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/border/optional_border_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/carpet/carpet_brush.h"

Theme::Type Theme::current_type = Theme::Type::System;

void Theme::setType(Theme::Type type) {
	current_type = type;

	if (wxTheApp) {
#if wxCHECK_VERSION(3, 3, 0)
		switch (type) {
			case Type::Dark:
				wxTheApp->SetAppearance(wxApp::Appearance::Dark);
#ifdef __WXMSW__
				wxTheApp->MSWEnableDarkMode(wxApp::DarkMode_Always);
#endif
				break;
			case Type::Light:
				wxTheApp->SetAppearance(wxApp::Appearance::Light);
				break;
			case Type::System:
			default:
				wxTheApp->SetAppearance(wxApp::Appearance::System);
#ifdef __WXMSW__
				wxTheApp->MSWEnableDarkMode(wxApp::DarkMode_Auto);
#endif
				break;
		}
#endif
		if (wxWindow* top = wxTheApp->GetTopWindow()) {
			top->Refresh();
		}
	}
}

Theme::Type Theme::getType() {
	return current_type;
}

wxColour Theme::Get(Role role) {
	bool is_dark;
	switch (current_type) {
		case Type::Dark:
			is_dark = true;
			break;
		case Type::Light:
			is_dark = false;
			break;
		case Type::System:
		default:
			is_dark = wxSystemSettings::GetAppearance().IsDark();
			break;
	}
	return is_dark ? GetDark(role) : GetLight(role);
}

wxColour Theme::GetDark(Role role) {
	switch (role) {
		case Role::Surface:
			return wxColour(45, 45, 48);
		case Role::Background:
			return wxColour(30, 30, 30);
		case Role::Header:
			return wxColour(25, 25, 25);
		case Role::Accent:
			return wxColour(0, 120, 215);
		case Role::AccentHover:
			return wxColour(0, 150, 255);
		case Role::Text:
			return wxColour(230, 230, 230);
		case Role::TextSubtle:
			return wxColour(150, 150, 150);
		case Role::TextOnAccent:
			return wxColour(255, 255, 255);
		case Role::Border:
			return wxColour(60, 60, 60);
		case Role::Selected:
			return wxColour(0, 120, 215, 60); // Alpha translucency
		case Role::Error:
			return wxColour(200, 50, 50);
		case Role::Warning:
			return wxColour(240, 185, 64);
		case Role::Success:
			return wxColour(96, 196, 142);
		case Role::CardBase:
			return wxColour(50, 50, 55);
		case Role::CardBaseHover:
			return wxColour(60, 60, 65);
		case Role::CardBorder:
			return wxColour(80, 80, 80);
		case Role::PanelBackground:
			return wxColour(26, 28, 33);
		case Role::RaisedSurface:
			return wxColour(36, 39, 46);
		case Role::FooterSurface:
			return wxColour(28, 30, 36);
		case Role::SelectionFill:
			return wxColour(58, 95, 146);
		case Role::PrimaryButton:
			return wxColour(53, 108, 178);
		case Role::PrimaryButtonHover:
			return wxColour(68, 124, 196);
		case Role::SecondaryButton:
			return wxColour(52, 56, 65);
		case Role::SecondaryButtonHover:
			return wxColour(62, 67, 78);
		case Role::ButtonDisabled:
			return wxColour(44, 47, 54);
		// Tooltip colors (vibrant on dark background)
		case Role::TooltipBg:
			return wxColour(30, 30, 35);
		case Role::TooltipLabel:
			return wxColour(220, 220, 220);
		case Role::TooltipActionId:
			return wxColour(255, 165, 0); // Orange
		case Role::TooltipUniqueId:
			return wxColour(100, 149, 237); // Cornflower Blue
		case Role::TooltipDoorId:
			return wxColour(0, 139, 139); // Dark Cyan
		case Role::TooltipDepotId:
			return wxColour(0, 206, 209); // Cyan
		case Role::TooltipTextValue:
			return wxColour(255, 215, 0); // Gold
		case Role::TooltipTeleport:
			return wxColour(186, 85, 211); // Medium Orchid
		case Role::TooltipWaypoint:
			return wxColour(0, 160, 255); // Vivid Blue
		case Role::TooltipBodyText:
			return wxColour(220, 220, 220); // Light Gray
		case Role::TooltipCountText:
			return wxColour(200, 200, 200); // Light Gray
		case Role::TooltipBorderWaypoint:
		case Role::TooltipBorderItem:
		case Role::TooltipBorderDoor:
		case Role::TooltipBorderTeleport:
		case Role::TooltipBorderText:
		case Role::TooltipBorderHouse:
		case Role::TooltipBorderZone:
		case Role::TooltipBorderTown:
			return wxColour(58, 58, 64); // Standardized Charcoal Gray
		case Role::TooltipWindowBg:
			return wxColour(24, 26, 30);
		case Role::TooltipWindowOutline:
			return wxColour(13, 14, 16);
		case Role::TooltipWindowBevelLight:
			return wxColour(48, 52, 60);
		case Role::TooltipWindowBevelDark:
			return wxColour(14, 15, 17);
		case Role::TooltipSlotBg:
			return wxColour(18, 20, 24);
		case Role::TooltipSlotBevelLight:
			return wxColour(52, 56, 64);
		case Role::TooltipSlotBevelDark:
			return wxColour(14, 15, 17);
		case Role::TooltipDividerLight:
			return wxColour(46, 50, 58);
		case Role::TooltipDividerDark:
			return wxColour(14, 15, 17);
		case Role::TooltipTextWellBg:
			return wxColour(16, 17, 20);
		case Role::TooltipTitleText:
			return wxColour(255, 255, 255);
		case Role::TooltipSubtitleText:
			return wxColour(160, 165, 175);
		case Role::ServerMarkerStart:
			return wxColour(255, 215, 0); // Gold
		case Role::ServerMarkerDepot:
			return wxColour(46, 204, 113); // Emerald
		case Role::ServerMarkerRaid:
			return wxColour(231, 76, 60); // Red
		case Role::ServerMarkerScript:
			return wxColour(171, 130, 255); // Lavender
		case Role::BrushCategoryGround:
			return wxColour(85, 230, 85);
		case Role::BrushCategoryWall:
			return wxColour(180, 185, 230);
		case Role::BrushCategoryWallDecoration:
			return wxColour(95, 175, 255);
		case Role::BrushCategoryDoor:
			return wxColour(255, 175, 55);
		case Role::BrushCategoryDoodad:
			return wxColour(255, 225, 45);
		case Role::BrushCategoryTable:
			return wxColour(225, 145, 70);
		case Role::BrushCategoryCarpet:
			return wxColour(225, 90, 235);
		case Role::BrushCategoryDefaultRaw:
			return wxColour(150, 155, 165);
		default:
			return *wxWHITE;
	}
}

wxColour Theme::GetLight(Role role) {
	switch (role) {
		case Role::Surface:
			return wxColour(240, 240, 240); // Standard dialog background
		case Role::Background:
			return *wxWHITE; // Lists, entries
		case Role::Header:
			return wxColour(220, 220, 220);
		case Role::Accent:
			return wxColour(0, 120, 215); // Keep blue
		case Role::AccentHover:
			return wxColour(0, 100, 180);
		case Role::Text:
			return *wxBLACK;
		case Role::TextSubtle:
			return wxColour(100, 100, 100);
		case Role::TextOnAccent:
			return *wxWHITE;
		case Role::Border:
			return wxColour(200, 200, 200);
		case Role::Selected:
			return wxColour(0, 120, 215, 60);
		case Role::Error:
			return wxColour(200, 0, 0);
		case Role::Warning:
			return wxColour(180, 120, 0);
		case Role::Success:
			return wxColour(32, 132, 76);
		case Role::CardBase:
			return *wxWHITE;
		case Role::CardBaseHover:
			return wxColour(245, 245, 245);
		case Role::CardBorder:
			return wxColour(200, 200, 200);
		case Role::PanelBackground:
			return wxColour(248, 249, 251);
		case Role::RaisedSurface:
			return *wxWHITE;
		case Role::FooterSurface:
			return wxColour(238, 240, 244);
		case Role::SelectionFill:
			return wxColour(209, 225, 245);
		case Role::PrimaryButton:
			return wxColour(45, 102, 173);
		case Role::PrimaryButtonHover:
			return wxColour(32, 90, 164);
		case Role::SecondaryButton:
			return wxColour(236, 239, 244);
		case Role::SecondaryButtonHover:
			return wxColour(225, 229, 235);
		case Role::ButtonDisabled:
			return wxColour(229, 232, 236);
		// Tooltip colors (readable on light background)
		case Role::TooltipBg:
			return wxColour(240, 240, 240);
		case Role::TooltipLabel:
			return wxColour(80, 80, 80);
		case Role::TooltipActionId:
			return wxColour(180, 100, 0); // Dark Orange
		case Role::TooltipUniqueId:
			return wxColour(30, 80, 180); // Dark Blue
		case Role::TooltipDoorId:
			return wxColour(0, 100, 100); // Teal
		case Role::TooltipDepotId:
			return wxColour(0, 130, 130); // Deep Teal
		case Role::TooltipTextValue:
			return wxColour(50, 50, 50); // Near Black
		case Role::TooltipTeleport:
			return wxColour(120, 40, 160); // Dark Purple
		case Role::TooltipWaypoint:
			return wxColour(20, 130, 20); // Dark Green
		case Role::TooltipBodyText:
			return wxColour(30, 30, 30); // Near Black
		case Role::TooltipCountText:
			return wxColour(60, 60, 60); // Dark Gray
		case Role::TooltipBorderWaypoint:
		case Role::TooltipBorderItem:
		case Role::TooltipBorderDoor:
		case Role::TooltipBorderTeleport:
		case Role::TooltipBorderText:
		case Role::TooltipBorderHouse:
		case Role::TooltipBorderZone:
		case Role::TooltipBorderTown:
			return wxColour(80, 80, 88); // Standardized Charcoal Gray (Light)
		case Role::TooltipWindowBg:
			return wxColour(235, 236, 240);
		case Role::TooltipWindowOutline:
			return wxColour(120, 122, 130);
		case Role::TooltipWindowBevelLight:
			return wxColour(255, 255, 255);
		case Role::TooltipWindowBevelDark:
			return wxColour(170, 172, 180);
		case Role::TooltipSlotBg:
			return wxColour(215, 216, 222);
		case Role::TooltipSlotBevelLight:
			return wxColour(255, 255, 255);
		case Role::TooltipSlotBevelDark:
			return wxColour(160, 162, 170);
		case Role::TooltipDividerLight:
			return wxColour(255, 255, 255);
		case Role::TooltipDividerDark:
			return wxColour(170, 172, 180);
		case Role::TooltipTextWellBg:
			return wxColour(225, 226, 230);
		case Role::TooltipTitleText:
			return wxColour(20, 20, 24);
		case Role::TooltipSubtitleText:
			return wxColour(90, 95, 105);
		case Role::ServerMarkerStart:
			return wxColour(255, 215, 0); // Gold
		case Role::ServerMarkerDepot:
			return wxColour(46, 204, 113); // Emerald
		case Role::ServerMarkerRaid:
			return wxColour(231, 76, 60); // Red
		case Role::ServerMarkerScript:
			return wxColour(171, 130, 255); // Lavender
		case Role::BrushCategoryGround:
			return wxColour(30, 155, 40);
		case Role::BrushCategoryWall:
			return wxColour(110, 115, 165);
		case Role::BrushCategoryWallDecoration:
			return wxColour(30, 125, 235);
		case Role::BrushCategoryDoor:
			return wxColour(215, 100, 10);
		case Role::BrushCategoryDoodad:
			return wxColour(185, 125, 0);
		case Role::BrushCategoryTable:
			return wxColour(165, 95, 30);
		case Role::BrushCategoryCarpet:
			return wxColour(175, 40, 195);
		case Role::BrushCategoryDefaultRaw:
			return wxColour(120, 125, 135);
		default:
			return *wxBLACK;
	}
}

Theme::Role Theme::GetBrushCategoryRole(const Brush* brush) {
	if (!brush) {
		return Role::BrushCategoryDefaultRaw;
	}
	if (brush->is<GroundBrush>() || brush->is<OptionalBorderBrush>()) {
		return Role::BrushCategoryGround;
	}
	if (brush->is<WallDecorationBrush>()) {
		return Role::BrushCategoryWallDecoration;
	}
	if (brush->is<WallBrush>()) {
		return Role::BrushCategoryWall;
	}
	if (brush->is<DoorBrush>()) {
		return Role::BrushCategoryDoor;
	}
	if (brush->is<DoodadBrush>()) {
		return Role::BrushCategoryDoodad;
	}
	if (brush->is<TableBrush>()) {
		return Role::BrushCategoryTable;
	}
	if (brush->is<CarpetBrush>()) {
		return Role::BrushCategoryCarpet;
	}
	return Role::BrushCategoryDefaultRaw;
}
