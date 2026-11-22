//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_OTBM_TFS_TILE_ZONES_H_
#define RME_IO_OTBM_TFS_TILE_ZONES_H_

#include <cstdint>
#include <span>

// Tile zones in the TFS-ZONES format (fridaii/RME-ZONES, fridaii/TFS-ZONES) for OTBM 1-4 maps.
// A tile with zones sets kZoneFlag in OTBM_ATTR_TILE_FLAGS. Right after the flags come the zone ids
// as u16 values, ended by a 0. Zone names are not part of the map file.
// OTBM 5 maps (Canary / Crystal Server) store zones in the OTBM_TILE_ZONE child node instead.
namespace TfsTileZones {

	inline constexpr uint32_t kZoneFlag = 0x0040;

	// The tile keeps its zones in the zone list; the flag is rebuilt from that list on save.
	[[nodiscard]] constexpr uint32_t FlagsToKeep(uint32_t file_flags) noexcept {
		return file_flags & ~kZoneFlag;
	}

	[[nodiscard]] constexpr uint32_t FlagsToWrite(uint32_t map_flags, bool has_zones) noexcept {
		return has_zones ? (map_flags | kZoneFlag) : (map_flags & ~kZoneFlag);
	}

	// Reads zone ids up to the 0 that ends the list and passes each one to add_zone.
	// Returns false when the data ends before that 0; the ids read so far have been passed on.
	template <typename Stream, typename AddZone>
	[[nodiscard]] bool ReadZoneList(Stream& stream, AddZone&& add_zone) {
		uint16_t zone_id = 0;
		while (stream.getU16(zone_id)) {
			if (zone_id == 0) {
				return true;
			}
			add_zone(zone_id);
		}
		return false;
	}

	template <typename Writer>
	void WriteZoneList(Writer& writer, std::span<const uint16_t> zone_ids) {
		for (const uint16_t zone_id : zone_ids) {
			writer.addU16(zone_id);
		}
		writer.addU16(0);
	}

} // namespace TfsTileZones

#endif
