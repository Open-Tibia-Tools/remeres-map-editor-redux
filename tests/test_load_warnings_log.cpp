#include "util/load_warnings_log.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

TEST_CASE("a block has a header and one indented line per warning", "[load_warnings_log]") {
	const std::vector<std::string> warnings = { "Item 100 has no sprite", "Unknown flag 0x20" };
	CHECK(FormatLoadWarningsBlock("2026-11-23 18:00:00", "Warnings", warnings) == "[2026-11-23 18:00:00] Warnings (2)\n"
																					"  Item 100 has no sprite\n"
																					"  Unknown flag 0x20\n");
}

TEST_CASE("multi-line warnings are split and blank lines dropped", "[load_warnings_log]") {
	const std::vector<std::string> warnings = { "first\r\nsecond\n\nthird\n" };
	CHECK(FormatLoadWarningsBlock("t", "Map loader errors", warnings) == "[t] Map loader errors (1)\n"
																		 "  first\n"
																		 "  second\n"
																		 "  third\n");
}

TEST_CASE("an empty list still writes the header", "[load_warnings_log]") {
	CHECK(FormatLoadWarningsBlock("t", "Warnings", {}) == "[t] Warnings (0)\n");
}
