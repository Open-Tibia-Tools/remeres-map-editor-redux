#include "map/position_format.h"
#include "util/number_input.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

namespace {
	constexpr PositionBounds kBounds { 2048, 2048 };

	std::optional<Position> Parse(std::string_view text, const PositionBounds& bounds = kBounds) {
		return ParsePosition(text, bounds);
	}

	bool Is(const std::optional<Position>& parsed, int x, int y, int z) {
		return parsed && parsed->x == x && parsed->y == y && parsed->z == z;
	}
}

TEST_CASE("every copy format parses back to the same position", "[position_format]") {
	const Position position(512, 1024, 7);
	for (const PositionFormatInfo& info : kPositionFormats) {
		const std::string text = FormatPosition(position, info.format);
		INFO(text);
		REQUIRE(Is(Parse(text), 512, 1024, 7));
	}
}

TEST_CASE("copy formats keep their exact text", "[position_format]") {
	const Position position(512, 1024, 7);
	REQUIRE(FormatPosition(position, PositionFormat::LuaTable) == "{x = 512, y = 1024, z = 7}");
	REQUIRE(FormatPosition(position, PositionFormat::CompactJson) == R"({"x":512,"y":1024,"z":7})");
	REQUIRE(FormatPosition(position, PositionFormat::Csv) == "512, 1024, 7");
	REQUIRE(FormatPosition(position, PositionFormat::Tuple) == "(512, 1024, 7)");
	REQUIRE(FormatPosition(position, PositionFormat::ConstructorCall) == "Position(512, 1024, 7)");
	REQUIRE(FormatPositionRange(Position(10, 20, 7), Position(30, 40, 7)) == "{fromx = 10, tox = 30, fromy = 20, toy = 40, z = 7}");
	REQUIRE(FormatPositionRange(Position(10, 20, 6), Position(30, 40, 7)) == "{fromx = 10, tox = 30, fromy = 20, toy = 40, fromz = 6, toz = 7}");
}

TEST_CASE("stored format settings are clamped like before", "[position_format]") {
	REQUIRE(PositionFormatFromSetting(-3) == PositionFormat::LuaTable);
	REQUIRE(PositionFormatFromSetting(0) == PositionFormat::LuaTable);
	REQUIRE(PositionFormatFromSetting(2) == PositionFormat::Csv);
	REQUIRE(PositionFormatFromSetting(4) == PositionFormat::ConstructorCall);
	REQUIRE(PositionFormatFromSetting(99) == PositionFormat::ConstructorCall);
}

TEST_CASE("keyed formats accept any key order, case and spacing", "[position_format]") {
	REQUIRE(Is(Parse("{z = 7, x = 512, y = 1024}"), 512, 1024, 7));
	REQUIRE(Is(Parse("{X=512,Y=1024,Z=7}"), 512, 1024, 7));
	REQUIRE(Is(Parse(R"(  { "z" : 7 , "y" : 1024 , "x" : 512 }  )"), 512, 1024, 7));
	REQUIRE(Is(Parse("{x = 512, y = 1024, z = 7,}"), 512, 1024, 7));
	REQUIRE(Is(Parse("{'x': 1, 'y': 2, 'z': 3}"), 1, 2, 3));
}

TEST_CASE("positional formats accept commas or spaces", "[position_format]") {
	REQUIRE(Is(Parse("512,1024,7"), 512, 1024, 7));
	REQUIRE(Is(Parse("512 1024 7"), 512, 1024, 7));
	REQUIRE(Is(Parse("\t512   1024\n7 "), 512, 1024, 7));
	REQUIRE(Is(Parse("(512,1024,7)"), 512, 1024, 7));
	REQUIRE(Is(Parse("[512, 1024, 7]"), 512, 1024, 7));
	REQUIRE(Is(Parse("{512, 1024, 7}"), 512, 1024, 7));
	REQUIRE(Is(Parse("position ( 512 , 1024 , 7 )"), 512, 1024, 7));
	REQUIRE(Is(Parse("POSITION(512, 1024, 7)"), 512, 1024, 7));
}

TEST_CASE("a copied range pastes as its from corner", "[position_format]") {
	REQUIRE(Is(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 40, z = 7}"), 10, 20, 7));
	REQUIRE(Is(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 40, fromz = 6, toz = 7}"), 10, 20, 6));
	REQUIRE(Is(Parse("{toy = 40, z = 7, tox = 30, fromy = 20, fromx = 10}"), 10, 20, 7));
	// A normal map y used to be read as the floor and rejected.
	REQUIRE(Is(Parse("{fromx = 1000, tox = 1010, fromy = 1500, toy = 1510, z = 7}"), 1000, 1500, 7));
    REQUIRE(Is(Parse("{fromx = 30, tox = 10, fromy = 40, toy = 20, fromz = 8, toz = 6}"), 10, 20, 6));
    REQUIRE_FALSE(Parse("{fromx = 10, tox = 2048, fromy = 20, toy = 40, z = 7}"));
    REQUIRE_FALSE(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 2048, z = 7}"));
    REQUIRE_FALSE(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 40, fromz = 6, toz = 16}"));
}

TEST_CASE("text that is not exactly one position is rejected", "[position_format]") {
	REQUIRE_FALSE(Parse(""));
	REQUIRE_FALSE(Parse("   "));
	REQUIRE_FALSE(Parse("512"));
	REQUIRE_FALSE(Parse("512, 1024"));
	REQUIRE_FALSE(Parse("512, 1024, 7, 8"));
	REQUIRE_FALSE(Parse("512 1024 7 8"));
	REQUIRE_FALSE(Parse("item 123, count 4, floor 5"));
	REQUIRE_FALSE(Parse("go to 512, 1024, 7 please"));
	REQUIRE_FALSE(Parse("512, , 7"));
	REQUIRE_FALSE(Parse("512.5, 1024, 7"));
	REQUIRE_FALSE(Parse("+512, 1024, 7"));
	REQUIRE_FALSE(Parse("Position 512, 1024, 7"));
	REQUIRE_FALSE(Parse("{x = 512, y = 1024}"));
	REQUIRE_FALSE(Parse("{x = 512, y = 1024, z = 7, x = 1}"));
	REQUIRE_FALSE(Parse("{x = 512, y = 1024, w = 7}"));
	REQUIRE_FALSE(Parse("{x = 512, y = 1024, z = }"));
	REQUIRE_FALSE(Parse("{x = 512, y = 1024, z = 7, fromx = 1}"));
	REQUIRE_FALSE(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 40}"));
	REQUIRE_FALSE(Parse("{fromx = 10, tox = 30, fromy = 20, toy = 40, z = 7, fromz = 6, toz = 7}"));
	REQUIRE_FALSE(Parse("99999999999, 1, 7"));
}

TEST_CASE("positions are checked with tile-index bounds", "[position_format]") {
	REQUIRE(Is(Parse("0, 0, 0"), 0, 0, 0));
	REQUIRE(Is(Parse("2047, 2047, 15"), 2047, 2047, 15));
	REQUIRE_FALSE(Parse("2048, 0, 7"));
	REQUIRE_FALSE(Parse("0, 2048, 7"));
	REQUIRE_FALSE(Parse("0, 0, 16"));
	REQUIRE_FALSE(Parse("-1, 0, 7"));
	REQUIRE_FALSE(Parse("0, -1, 7"));
	REQUIRE_FALSE(Parse("0, 0, -1"));
	REQUIRE(Is(Parse("64999, 64999, 7", PositionBounds {}), 64999, 64999, 7));
	REQUIRE_FALSE(Parse("65000, 0, 7", PositionBounds {}));
}

TEST_CASE("number input keeps whole integers only", "[number_input]") {
	REQUIRE(ResolveNumberInput("512", 0, 2047, 10) == 512);
	REQUIRE(ResolveNumberInput("  512 ", 0, 2047, 10) == 512);
	REQUIRE(ResolveNumberInput("5000", 0, 2047, 10) == 2047);
	REQUIRE(ResolveNumberInput("-5", 0, 2047, 10) == 0);
	REQUIRE(ResolveNumberInput("", 0, 2047, 10) == 10);
	REQUIRE(ResolveNumberInput("   ", 0, 2047, 10) == 10);
	// A pasted triple no longer turns into one huge number.
	REQUIRE(ResolveNumberInput("512, 1024, 7", 0, 2047, 10) == 10);
	REQUIRE(ResolveNumberInput("32369, 32241, 7", 0, 65000, 10) == 10);
	REQUIRE(ResolveNumberInput("12a", 0, 2047, 10) == 10);
	REQUIRE(ResolveNumberInput("99999999999999999999", 0, 2047, 10) == 10);
	REQUIRE(ResolveNumberInput("9999999999", 0, 2047, 10) == 2047);
    REQUIRE(ResolveNumberInput("", 10, 20, 5) == 10);
    REQUIRE(ResolveNumberInput("abc", 10, 20, 50) == 20);
    REQUIRE(ResolveNumberInput("5", 10, 5, 7) == 10);
    REQUIRE(ResolveNumberInput("", 10, 5, 7) == 10);
}
