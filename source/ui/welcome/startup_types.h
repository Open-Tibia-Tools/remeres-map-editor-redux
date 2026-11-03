#ifndef RME_STARTUP_TYPES_H_
#define RME_STARTUP_TYPES_H_

#include <string>

#include <wx/colour.h>
#include <wx/string.h>

#include "editor/persistence/map_load_options.h"

class ClientVersion;

struct StartupRecentMapEntry {
	wxString path;
	wxString modified_label;
	bool ephemeral = false;
};

struct StartupConfiguredClientEntry {
	ClientVersion* client = nullptr;
	wxString name;
	wxString client_path;
};

enum class StartupCompatibilityStatus {
	MissingSelection,
	Compatible,
	ForceRequired,
	Forced,
	MapError,
	Incompatible, // The map needs another kind of client; Force Load cannot help
};

struct StartupLoadRequest {
	wxString map_path;
	MapLoadOptions load_options;
};

struct StartupListItem {
	wxString primary_text;
	wxString secondary_text;
	std::string icon_art_id;
	wxColour accent_colour = wxNullColour;
};

struct StartupInfoField {
	wxString label;
	wxString value;
	std::string icon_art_id;
	wxColour value_colour = wxNullColour;
	bool separator_after = false;
};

#endif
