//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_ITEM_DEFINITIONS_OBJECTS_SRV_READER_H_
#define RME_ITEM_DEFINITIONS_OBJECTS_SRV_READER_H_

// Reader for objects.srv, the object type list of the original game
// server. Each record is a block of `Key = Value` lines starting at TypeID:
//
//   TypeID      = 1648
//   Name        = "a gate of expertise"
//   Description = "Only the worthy may pass"
//   Flags       = {Bottom,LevelDoor,Unpass,Unmove,Unthrow,Unlay}
//   Attributes  = {Waypoints=0,DisguiseTarget=353}
//
// Only text is read here; mapping to editor item definitions is done by
// SrvItemParser.

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rme::srv {

	struct ObjectType {
		uint16_t type_id = 0;
		std::string name;
		std::string description;
		std::vector<std::string> flags;
		std::vector<std::pair<std::string, int64_t>> attributes;

		// Names are compared case-insensitively, as the server does.
		[[nodiscard]] bool hasFlag(std::string_view flag) const;
		[[nodiscard]] std::optional<int64_t> attribute(std::string_view name) const;
	};

	struct ReadError {
		int line = 0;
		std::string message;
	};

	[[nodiscard]] std::expected<std::vector<ObjectType>, ReadError> readObjectTypes(std::string_view text);

} // namespace rme::srv

#endif
