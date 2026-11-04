//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SECTOR_PARSER_H_
#define RME_IO_SEC_SECTOR_PARSER_H_

// Parser for sector files (`XXXX-YYYY-ZZ.sec`), the text map format
// of the original Tibia game server. A map is a folder of these files; each
// one holds a 32x32 block of tiles on one floor. The grammar follows the
// server's own loader (LoadSector / LoadObjects).
//
// The parser only reads text. Type IDs are kept as written (server type IDs,
// which are client item IDs) and attribute values are kept raw, so the
// result can be unit tested without item definitions or a map.

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rme::sec {

	inline constexpr int kSectorSize = 32;
	inline constexpr std::string_view kSectorFileExtension = ".sec";

	// Instance attributes in the order of the server's INSTANCEATTRIBUTE enum.
	enum class InstanceAttribute : uint8_t {
		Content,
		ChestQuestNumber,
		Amount,
		KeyNumber,
		KeyholeNumber,
		Level,
		DoorQuestNumber,
		DoorQuestValue,
		Charges,
		String,
		Editor,
		ContainerLiquidType,
		PoolLiquidType,
		AbsTeleportDestination,
		Responsible,
		RemainingExpireTime,
		SavedExpireTime,
		RemainingUses,
	};

	[[nodiscard]] std::string_view attributeName(InstanceAttribute attribute);

	enum class TileFlag : uint8_t {
		Refresh = 1 << 0,
		NoLogout = 1 << 1,
		ProtectionZone = 1 << 2,
	};

	struct ObjectAttribute {
		InstanceAttribute key;
		int64_t number = 0; // Numeric attributes
		std::string text; // String and Editor
	};

	struct SectorObject {
		uint16_t type_id = 0;
		std::vector<ObjectAttribute> attributes;
		std::vector<SectorObject> content; // Items inside a container
	};

	// Objects are in the server's order: ground and always-on-top items first,
	// then the remaining items with the topmost first.
	struct SectorTile {
		uint8_t offset_x = 0;
		uint8_t offset_y = 0;
		uint8_t flags = 0; // TileFlag bits
		std::vector<SectorObject> objects;

		[[nodiscard]] bool hasFlag(TileFlag flag) const {
			return (flags & static_cast<uint8_t>(flag)) != 0;
		}
	};

	struct Sector {
		std::vector<SectorTile> tiles;
	};

	struct SectorCoordinates {
		int x = 0;
		int y = 0;
		int z = 0;
	};

	struct ParseError {
		int line = 0;
		std::string message;
	};

	struct AbsolutePosition {
		int x = 0;
		int y = 0;
		int z = 0;

		bool operator==(const AbsolutePosition&) const = default;
	};

	// Reads sector coordinates from a file name such as `1005-0997-06.sec`.
	[[nodiscard]] std::optional<SectorCoordinates> parseSectorFileName(std::string_view file_name);

	[[nodiscard]] std::expected<Sector, ParseError> parseSector(std::string_view text);

	// `XXXX-YYYY-ZZ.sec`, as the server names its sector files.
	[[nodiscard]] std::string sectorFileName(const SectorCoordinates& coordinates);

	// Writes a sector the way the server's SaveSector does: header, then one
	// line per tile in x, y order (tiles without flags or objects are left
	// out), flags before Content, attributes in the server's order with
	// Content last, LF line endings.
	[[nodiscard]] std::string writeSector(const SectorCoordinates& coordinates, const Sector& sector);

	// Decodes the server's packed absolute coordinate (AbsTeleportDestination).
	[[nodiscard]] AbsolutePosition unpackAbsoluteCoordinate(int64_t packed);
	[[nodiscard]] int32_t packAbsoluteCoordinate(const AbsolutePosition& position);

} // namespace rme::sec

#endif
