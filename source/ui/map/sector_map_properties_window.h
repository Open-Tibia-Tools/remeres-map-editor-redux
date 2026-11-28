//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_MAP_SECTOR_MAP_PROPERTIES_WINDOW_H_
#define RME_UI_MAP_SECTOR_MAP_PROPERTIES_WINDOW_H_

#include "app/main.h"

class Map;

// Map > Properties for a sector map. Sector files have no header, so nothing
// here is editable: it shows the format, the client, the map area and which
// server files around the map folder the editor reads and writes.
class SectorMapPropertiesWindow : public wxDialog {
public:
	SectorMapPropertiesWindow(wxWindow* parent, const Map& map);
};

#endif
