#include "palette/hardcoded_palette_registry.h"

#include "app/main.h"
#include "palette/all_items/all_items_palette.h"
#include "palette/house/house_palette.h"
#include "palette/palette_waypoints.h"
#include "palette/palette_zones.h"
#include "palette/town/town_palette.h"

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

PalettePanel* createTownPanel(wxWindow* parent) {
	return newd TownPalettePanel(parent);
}

void updateTownMap(PalettePanel* panel, Map* map) {
	if (auto* townPanel = dynamic_cast<TownPalettePanel*>(panel)) {
		townPanel->SetMap(map);
	}
}

PalettePanel* createAllItemsPanel(wxWindow* parent) {
	return newd AllItemsPalette(parent);
}

void updateAllItemsMap(PalettePanel* /*panel*/, Map* /*map*/) {
}

} // namespace

const std::vector<HardcodedPaletteProvider>& GetHardcodedPaletteProviders() {
	static const std::vector<HardcodedPaletteProvider> providers {
		{ "All items", createAllItemsPanel, updateAllItemsMap },
		{ "House", createHousePanel, updateHouseMap },
		{ "Waypoint", createWaypointPanel, updateWaypointMap },
		{ "Zone", createZonePanel, updateZoneMap },
		{ "Town", createTownPanel, updateTownMap },
	};
	return providers;
}
