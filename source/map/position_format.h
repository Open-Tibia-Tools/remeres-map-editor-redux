//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_POSITION_FORMAT_H_
#define RME_MAP_POSITION_FORMAT_H_

#include "map/position.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

// The formats offered by Preferences > General > Clipboard > Copy position format. The
// COPY_POSITION_FORMAT setting stores the numeric value, so the order must not change.
enum class PositionFormat : int {
	LuaTable = 0,
	CompactJson = 1,
	Csv = 2,
	Tuple = 3,
	ConstructorCall = 4,
};

struct PositionFormatInfo {
	PositionFormat format;
	const char* label;
};

inline constexpr std::array<PositionFormatInfo, 5> kPositionFormats = { {
	{ PositionFormat::LuaTable, "Lua table" },
	{ PositionFormat::CompactJson, "Compact JSON" },
	{ PositionFormat::Csv, "CSV" },
	{ PositionFormat::Tuple, "Tuple" },
	{ PositionFormat::ConstructorCall, "Constructor call" },
} };

// A stored setting outside the known range falls back to the nearest format.
[[nodiscard]] PositionFormat PositionFormatFromSetting(int value);

[[nodiscard]] std::string FormatPosition(const Position& position, PositionFormat format);

// A multi-tile selection is always copied as a Lua range table, whatever the format:
// {fromx = .., tox = .., fromy = .., toy = .., z = ..}, with fromz/toz instead of z when
// the selection spans several floors.
[[nodiscard]] std::string FormatPositionRange(const Position& from, const Position& to);

// Tile-index bounds of a map: 0 <= x < width, 0 <= y < height, 0 <= z <= MAP_MAX_LAYER.
struct PositionBounds {
	int width = MAP_MAX_WIDTH;
	int height = MAP_MAX_HEIGHT;

	[[nodiscard]] bool contains(const Position& position) const noexcept {
		return position.x >= 0 && position.x < width && position.y >= 0 && position.y < height && position.z >= 0 && position.z <= MAP_MAX_LAYER;
	}
};

// Parses one position written in any format FormatPosition produces, or as "x y z".
// Keys are case-insensitive and may come in any order; whitespace is free. The range
// table from FormatPositionRange yields its from corner: the lowest x, y and floor
// number of the copied area. Returns nullopt for any other text (prose, a single
// number, two or four numbers, missing or repeated keys) and for a position outside
// the bounds.
[[nodiscard]] std::optional<Position> ParsePosition(std::string_view text, const PositionBounds& bounds);

#endif
