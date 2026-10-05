#include "palette/hardcoded_palette_registry.h"

#include "app/main.h"
#include "palette/house/house_palette.h"
#include "palette/palette_waypoints.h"
#include "palette/palette_zones.h"

namespace {

PalettePanel* createHousePanel(wxWindow* parent) {
	return newd HousePalette(parent);
}

PalettePanel* createWaypointPanel(wxWindow* parent) {
	return newd WaypointPalettePanel(parent);
}

void updateHouseMap(PalettePanel* panel, Map* map) {
	if (auto* housePanel = dynamic_cast<HousePalette*>(panel)) {
		housePanel->SetMap(map);
	}
}

void updateWaypointMap(PalettePanel* panel, Map* map) {
	if (auto* waypointPanel = dynamic_cast<WaypointPalettePanel*>(panel)) {
		waypointPanel->SetMap(map);
	}
}

PalettePanel* createZonePanel(wxWindow* parent) {
	return newd ZonePalettePanel(parent);
}

void updateZoneMap(PalettePanel* panel, Map* map) {
	if (auto* zonePanel = dynamic_cast<ZonePalettePanel*>(panel)) {
		zonePanel->SetMap(map);
	}
}

} // namespace

const std::vector<HardcodedPaletteProvider>& GetHardcodedPaletteProviders() {
	static const std::vector<HardcodedPaletteProvider> providers {
		{ "House", createHousePanel, updateHouseMap },
		{ "Waypoint", createWaypointPanel, updateWaypointMap },
		{ "Zone", createZonePanel, updateZoneMap },
	};
	return providers;
}
