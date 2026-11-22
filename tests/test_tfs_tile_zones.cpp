#include "io/otbm/fast_otbm_reader.h"
#include "io/otbm/tfs_tile_zones.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

using TfsTileZones::FlagsToKeep;
using TfsTileZones::FlagsToWrite;
using TfsTileZones::kZoneFlag;
using TfsTileZones::ReadZoneList;
using TfsTileZones::WriteZoneList;

namespace {
	struct RecordingWriter {
		std::vector<uint16_t> values;
		void addU16(uint16_t value) {
			values.push_back(value);
		}
	};

	std::vector<uint16_t> ReadAll(const std::vector<uint8_t>& bytes, bool& complete, size_t& bytes_left) {
		FastOTBMStream stream(bytes.data(), bytes.size());
		std::vector<uint16_t> ids;
		complete = ReadZoneList(stream, [&ids](uint16_t id) { ids.push_back(id); });
		bytes_left = stream.remaining();
		return ids;
	}
} // namespace

TEST_CASE("the zone flag is the one fridaii TFS-ZONES uses", "[tfs_zones]") {
	STATIC_REQUIRE(kZoneFlag == 0x0040);
}

TEST_CASE("the zone flag is rebuilt from the zone list", "[tfs_zones]") {
	constexpr uint32_t pz_and_nologout = 0x0001 | 0x0008;
	STATIC_REQUIRE(FlagsToKeep(pz_and_nologout | kZoneFlag) == pz_and_nologout);
	STATIC_REQUIRE(FlagsToWrite(pz_and_nologout, true) == (pz_and_nologout | kZoneFlag));
	STATIC_REQUIRE(FlagsToWrite(pz_and_nologout | kZoneFlag, false) == pz_and_nologout);
	// A zone alone still needs the flags attribute
	STATIC_REQUIRE(FlagsToWrite(0, true) == kZoneFlag);
}

TEST_CASE("zone ids are read up to the terminating 0", "[tfs_zones]") {
	// ids 3 and 0x0102, end, then the next tile attribute (OTBM_ATTR_ITEM = 9)
	const std::vector<uint8_t> bytes { 0x03, 0x00, 0x02, 0x01, 0x00, 0x00, 0x09 };
	bool complete = false;
	size_t left = 0;
	const auto ids = ReadAll(bytes, complete, left);
	REQUIRE(complete);
	REQUIRE(ids == std::vector<uint16_t> { 3, 0x0102 });
	// The reader stops right after the terminator, so the next attribute is still there
	REQUIRE(left == 1);
}

TEST_CASE("an empty zone list is only the terminator", "[tfs_zones]") {
	const std::vector<uint8_t> bytes { 0x00, 0x00 };
	bool complete = false;
	size_t left = 0;
	REQUIRE(ReadAll(bytes, complete, left).empty());
	REQUIRE(complete);
	REQUIRE(left == 0);
}

TEST_CASE("escaped bytes inside a zone id are decoded", "[tfs_zones]") {
	// Zone id 0x00FE: its low byte equals OTBM_NODE_START and is written behind the escape byte
	const std::vector<uint8_t> bytes { OTBM_ESCAPE_CHAR, OTBM_NODE_START, 0x00, 0x00, 0x00 };
	bool complete = false;
	size_t left = 0;
	const auto ids = ReadAll(bytes, complete, left);
	REQUIRE(complete);
	REQUIRE(ids == std::vector<uint16_t> { 0x00FE });
}

TEST_CASE("a list cut off by the end of the node is reported", "[tfs_zones]") {
	// One id, then the node ends before the terminator
	const std::vector<uint8_t> bytes { 0x07, 0x00, OTBM_NODE_END };
	bool complete = true;
	size_t left = 0;
	const auto ids = ReadAll(bytes, complete, left);
	REQUIRE_FALSE(complete);
	REQUIRE(ids == std::vector<uint16_t> { 7 });
	// The node end stays for the caller
	REQUIRE(left == 1);
}

TEST_CASE("written zone lists end with 0 and read back the same", "[tfs_zones]") {
	constexpr std::array<uint16_t, 3> zones { 1, 42, 0xFFFF };
	RecordingWriter writer;
	WriteZoneList(writer, zones);
	REQUIRE(writer.values == std::vector<uint16_t> { 1, 42, 0xFFFF, 0 });

	std::vector<uint8_t> bytes;
	for (const uint16_t value : writer.values) {
		bytes.push_back(static_cast<uint8_t>(value & 0xFF));
		bytes.push_back(static_cast<uint8_t>(value >> 8));
	}
	// 0xFFFF holds OTBM_NODE_END bytes; escape them like the node writer does
	std::vector<uint8_t> escaped;
	for (const uint8_t byte : bytes) {
		if (byte == OTBM_NODE_START || byte == OTBM_NODE_END || byte == OTBM_ESCAPE_CHAR) {
			escaped.push_back(OTBM_ESCAPE_CHAR);
		}
		escaped.push_back(byte);
	}
	bool complete = false;
	size_t left = 0;
	REQUIRE(ReadAll(escaped, complete, left) == std::vector<uint16_t> { 1, 42, 0xFFFF });
	REQUIRE(complete);
}

TEST_CASE("writing no zones still writes the terminator", "[tfs_zones]") {
	RecordingWriter writer;
	WriteZoneList(writer, {});
	REQUIRE(writer.values == std::vector<uint16_t> { 0 });
}
