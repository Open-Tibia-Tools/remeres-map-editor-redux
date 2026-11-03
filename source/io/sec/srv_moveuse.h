//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SRV_MOVEUSE_H_
#define RME_IO_SEC_SRV_MOVEUSE_H_

// Reader for dat/moveuse.dat, the server's rules for using, moving onto and
// moving objects: levers, depots, quest doors, teleports and the like.
// Each rule is one line,
//   Use, IsType (Obj1,2772), IsPosition (Obj1,[32625,31698,11]) -> Change(Obj1,2773,0), SetQuestValue(User,258,1) "Lever"
// an event, its conditions, `->`, its actions and an optional name, inside
// nested `BEGIN "section"` ... `END` blocks. The editor reads the rules to
// show which map positions they name; it does not run them.

#include "io/sec/sec_sector_parser.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace rme::sec {

	struct ScriptValue {
		enum class Kind : uint8_t {
			Number,
			Coordinate,
			String,
			Identifier, // Obj1, User, ...
			Operator, // the comparison of HasQuestValue: =, <, >, <>, ...
		};

		Kind kind = Kind::Number;
		int64_t number = 0;
		AbsolutePosition position;
		std::string text; // String, Identifier and Operator

		bool operator==(const ScriptValue&) const = default;
	};

	// A condition or action: `ChangeOnMap([32627,31699,10],1771,622,0)`.
	// Conditions can be negated: `!IsObjectThere ([32864,32556,11],1563)`.
	struct ScriptCall {
		std::string name;
		std::vector<ScriptValue> arguments;
		bool negated = false;

		bool operator==(const ScriptCall&) const = default;
	};

	struct MoveUseRule {
		int line = 0;
		std::string section; // the enclosing BEGIN names, joined by " / "
		std::string event; // Use, MultiUse, Movement, Collision, Separation
		std::vector<ScriptCall> conditions;
		std::vector<ScriptCall> actions;
		std::string name; // the quoted text after the actions, often empty
	};

	[[nodiscard]] std::expected<std::vector<MoveUseRule>, ParseError> readMoveUseRules(std::string_view text);

} // namespace rme::sec

#endif
