#include "app/otfi_descriptor.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("an extended 8.60 otfi sets every flag and file", "[otfi]") {
	std::string error;
	const auto otfi = ParseOtfi(
		"DatSpr\r\n"
		"  extended: true\r\n"
		"  transparency: true\r\n"
		"  frame-durations: false\r\n"
		"  frame-groups: false\r\n"
		"  metadata-file: Custom.dat\r\n"
		"  sprites-file: Custom.spr\r\n"
		"  sprite-size: 32\r\n"
		"  sprite-data-size: 4096",
		error
	);
	REQUIRE(otfi.has_value());
	CHECK(otfi->extended);
	CHECK(otfi->transparency);
	CHECK_FALSE(otfi->frame_durations);
	CHECK_FALSE(otfi->frame_groups);
	CHECK(otfi->metadata_file == "Custom.dat");
	CHECK(otfi->sprites_file == "Custom.spr");
}

TEST_CASE("missing keys keep the defaults", "[otfi]") {
	std::string error;
	const auto otfi = ParseOtfi("\xEF\xBB\xBF" "DatSpr\n  frame-groups: true\n", error);
	REQUIRE(otfi.has_value());
	CHECK_FALSE(otfi->extended);
	CHECK_FALSE(otfi->transparency);
	CHECK_FALSE(otfi->frame_durations);
	CHECK(otfi->frame_groups);
	CHECK(otfi->metadata_file == "Tibia.dat");
	CHECK(otfi->sprites_file == "Tibia.spr");
}

TEST_CASE("only the DatSpr node is read", "[otfi]") {
	std::string error;
	const auto otfi = ParseOtfi(
		"Other\n  extended: true\n"
		"DatSpr\n  transparency: true\n"
		"Later\n  extended: true\n",
		error
	);
	REQUIRE(otfi.has_value());
	CHECK_FALSE(otfi->extended);
	CHECK(otfi->transparency);
}

TEST_CASE("a file without DatSpr is rejected", "[otfi]") {
	std::string error;
	REQUIRE_FALSE(ParseOtfi("Something\n  extended: true\n", error).has_value());
	CHECK(error.find("DatSpr") != std::string::npos);
}

TEST_CASE("a non-boolean flag is rejected", "[otfi]") {
	std::string error;
	REQUIRE_FALSE(ParseOtfi("DatSpr\n  extended: maybe\n", error).has_value());
	CHECK(error.find("extended") != std::string::npos);
}
