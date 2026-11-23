//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_APP_OTFI_DESCRIPTOR_H_
#define RME_APP_OTFI_DESCRIPTOR_H_

#include <optional>
#include <string>
#include <string_view>

// Contents of an OTClient "Tibia.otfi" file, which describes the DAT/SPR pair it sits next to.
// When a client assets folder holds one, it overrides the clients.toml feature flags for that
// load, the same way OTClient and OTAcademy RME read it.
struct OtfiDescriptor {
	bool extended = false;
	bool transparency = false;
	bool frame_durations = false;
	bool frame_groups = false;
	std::string metadata_file = "Tibia.dat";
	std::string sprites_file = "Tibia.spr";
};

// Parses the OTML text of an .otfi file. Keys missing from the "DatSpr" node keep the
// defaults above. Returns std::nullopt and fills error when there is no "DatSpr" node or a
// flag has a value that is not a boolean.
[[nodiscard]] std::optional<OtfiDescriptor> ParseOtfi(std::string_view text, std::string& error);

#endif
