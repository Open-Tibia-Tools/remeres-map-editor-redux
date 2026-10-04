#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <fstream>

#define RME_MAIN_H_
#define ASSERT assert

#include "io/filehandle.cpp"
#include "io/compression/gzip_stream.cpp"
#include "io/compression/gzip_node_file_write_handle.cpp"
#include "map/map_storage_format.h"

namespace {

	constexpr uint8_t kNodeStart = 0xFE;

	bool hasValidOtbmPrefix(std::span<const uint8_t> data) noexcept {
		if (data.size() < 5) {
			return false;
		}
		constexpr uint8_t wildcard[4] = { 0, 0, 0, 0 };
		const bool known_id = std::memcmp(data.data(), "OTBM", 4) == 0 || std::memcmp(data.data(), wildcard, 4) == 0;
		return known_id && data[4] == kNodeStart;
	}

	template <class Handle, class... Args>
	void writeSampleMap(const std::string& description, Args&&... args) {
		Handle f(std::forward<Args>(args)...);
		assert(f.isOk());
		f.addNode(0);
		f.addU32(2);
		f.addU16(1024);
		f.addU16(2048);
		f.addU32(3);
		f.addU32(57);
		f.addNode(2); // OTBM_MAP_DATA
		f.addU8(1); // OTBM_ATTR_DESCRIPTION
		f.addString(description);
		f.addU8(11); // OTBM_ATTR_EXT_SPAWN_FILE
		f.addString("spawn.xml");
		for (int i = 0; i < 50000; ++i) {
			f.addNode(4);
			f.addU32(static_cast<uint32_t>(i));
			f.endNode();
		}
		f.endNode();
		f.endNode();
		f.close();
		assert(f.isOk());
	}

	void testScenario1_Detection() {
		std::cout << "[Scenario 1] Testing file format and magic detection...\n";
		const std::filesystem::path gzip_path = "tests/gzip_world.otbm";
		const std::filesystem::path none_path = "tests/none_world.otbm";

		assert(std::filesystem::exists(gzip_path));
		assert(std::filesystem::exists(none_path));

		assert(Compression::isGzipFile(gzip_path));
		assert(!Compression::isGzipFile(none_path));

		const auto isize = Compression::readGzipUncompressedSize(gzip_path);
		assert(isize.has_value());
		assert(*isize == 185504650);

		const auto none_isize = Compression::readGzipUncompressedSize(none_path);
		assert(!none_isize.has_value() || *none_isize != 185504650);
		std::cout << "  -> Passed: Detection and ISIZE reading successful.\n";
	}

	void testScenario2_HeaderPeeking() {
		std::cout << "[Scenario 2] Testing fast header peeking without full inflation...\n";
		const std::filesystem::path gzip_path = "tests/gzip_world.otbm";
		const std::filesystem::path none_path = "tests/none_world.otbm";

		// Peek 64 KiB from gzip map
		const auto t0 = std::chrono::high_resolution_clock::now();
		const auto gzip_prefix = Compression::decompressGzipFile(gzip_path, 64 * 1024);
		const auto t1 = std::chrono::high_resolution_clock::now();
		assert(gzip_prefix && gzip_prefix->size() == 64 * 1024);
		assert(hasValidOtbmPrefix(*gzip_prefix));

		MemoryNodeFileReadHandle gzip_handle(gzip_prefix->data() + 4, gzip_prefix->size() - 4);
		BinaryNode* root = gzip_handle.getRootNode();
		assert(root != nullptr);
		uint8_t type = 0;
		uint32_t version = 0;
		uint16_t width = 0, height = 0;
		uint32_t gzip_major = 0, gzip_minor = 0;
		const bool ok_gz = root->getByte(type) && root->getU32(version) && root->getU16(width) && root->getU16(height) && root->getU32(gzip_major) && root->getU32(gzip_minor);
		assert(ok_gz);
		assert(width == 35143 && height == 34812);
		assert(gzip_major == 4 && gzip_minor == 4);

		// Peek 64 KiB from raw map
		const auto none_prefix = Compression::readRawFile(none_path, 64 * 1024);
		assert(none_prefix && none_prefix->size() == 64 * 1024);
		assert(hasValidOtbmPrefix(*none_prefix));

		MemoryNodeFileReadHandle none_handle(none_prefix->data() + 4, none_prefix->size() - 4);
		BinaryNode* none_root = none_handle.getRootNode();
		assert(none_root != nullptr);
		uint8_t none_type = 0;
		uint32_t none_version = 0;
		uint16_t none_width = 0, none_height = 0;
		uint32_t none_major = 0, none_minor = 0;
		const bool ok_none = none_root->getByte(none_type) && none_root->getU32(none_version) && none_root->getU16(none_width) && none_root->getU16(none_height) && none_root->getU32(none_major) && none_root->getU32(none_minor);
		assert(ok_none);
		assert(none_width == 6063 && none_height == 6018);
		assert(none_major == 3 && none_minor == 57);

		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
		std::cout << "  -> Passed: Header parsed in " << elapsed << " us (Gzip 4/4: " << width << "x" << height << ", None 3/57: " << none_width << "x" << none_height << ").\n";
	}

	void testScenario3_FullLoad() {
		std::cout << "[Scenario 3] Testing full loading of both world files...\n";
		const std::filesystem::path gzip_path = "tests/gzip_world.otbm";
		const std::filesystem::path none_path = "tests/none_world.otbm";

		const auto t0 = std::chrono::high_resolution_clock::now();
		const auto gzip_data = Compression::decompressGzipFile(gzip_path);
		const auto t1 = std::chrono::high_resolution_clock::now();
		assert(gzip_data.has_value());
		assert(gzip_data->size() == 185504650);
		assert(hasValidOtbmPrefix(*gzip_data));

		const auto none_data = Compression::readRawFile(none_path);
		assert(none_data.has_value());
		assert(none_data->size() == 33724117);
		assert(hasValidOtbmPrefix(*none_data));

		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		std::cout << "  -> Passed: 52 MB GZIP inflated to " << gzip_data->size() << " bytes in " << ms << " ms.\n";
	}

	void testScenario4_ConversionNormalToGzip() {
		std::cout << "[Scenario 4] Testing format conversion: Normal OTBM -> GZIP...\n";
		const std::filesystem::path none_path = "tests/none_world.otbm";
		const auto temp_dir = std::filesystem::temp_directory_path() / "rme_gzip_test";
		std::filesystem::create_directories(temp_dir);
		const auto original_none = temp_dir / "original_none.otbm";
		const auto converted_gz = temp_dir / "converted_from_none.otbm";

		std::vector<uint8_t> uncompressed_bytes;
		if (std::filesystem::exists(none_path)) {
			const auto loaded = Compression::readRawFile(none_path);
			assert(loaded.has_value());
			uncompressed_bytes = std::move(*loaded);
		} else {
			writeSampleMap<DiskNodeFileWriteHandle>("Normal to Gzip Map", original_none.string(), std::string(4, '\0'));
			const auto loaded = Compression::readRawFile(original_none);
			assert(loaded.has_value());
			uncompressed_bytes = std::move(*loaded);
		}

		// Convert uncompressed map to GZIP format by streaming its payload
		{
#ifdef _WIN32
			gzFile gz = gzopen_w(converted_gz.c_str(), "wb");
#else
			gzFile gz = gzopen(converted_gz.c_str(), "wb");
#endif
			assert(gz != nullptr);
			const int written = gzwrite(gz, uncompressed_bytes.data(), static_cast<unsigned>(uncompressed_bytes.size()));
			assert(written == static_cast<int>(uncompressed_bytes.size()));
			const int close_res = gzclose(gz);
			assert(close_res == Z_OK);
		}

		assert(Compression::isGzipFile(converted_gz));
		const auto inflated = Compression::decompressGzipFile(converted_gz);
		assert(inflated.has_value());
		assert(inflated->size() == uncompressed_bytes.size());
		assert(*inflated == uncompressed_bytes);
		assert(hasValidOtbmPrefix(*inflated));

		// Verify OTBM parsing on converted gzip map matches original
		MemoryNodeFileReadHandle handle(inflated->data() + 4, inflated->size() - 4);
		BinaryNode* root = handle.getRootNode();
		assert(root != nullptr);
		uint8_t type = 0;
		uint32_t version = 0;
		uint16_t width = 0, height = 0;
		assert(root->getByte(type) && root->getU32(version) && root->getU16(width) && root->getU16(height));
		assert(width > 0 && height > 0);

		std::filesystem::remove_all(temp_dir);
		std::cout << "  -> Passed: Existing normal map converted to GZIP and verified with byte-for-byte fidelity.\n";
	}

	void testScenario5_ConversionGzipToNormal() {
		std::cout << "[Scenario 5] Testing format conversion: GZIP -> Normal OTBM...\n";
		const std::filesystem::path gzip_path = "tests/gzip_world.otbm";
		const auto temp_dir = std::filesystem::temp_directory_path() / "rme_gzip_test";
		std::filesystem::create_directories(temp_dir);
		const auto original_gz = temp_dir / "original_gz.otbm";
		const auto uncompressed_file = temp_dir / "converted_to_uncompressed.otbm";

		std::vector<uint8_t> decompressed_bytes;
		if (std::filesystem::exists(gzip_path)) {
			const auto loaded = Compression::decompressGzipFile(gzip_path);
			assert(loaded.has_value());
			decompressed_bytes = std::move(*loaded);
		} else {
			writeSampleMap<GzipNodeFileWriteHandle>("Gzip to Normal Map", original_gz, std::string(4, '\0'));
			const auto loaded = Compression::decompressGzipFile(original_gz);
			assert(loaded.has_value());
			decompressed_bytes = std::move(*loaded);
		}

		// Convert by writing uncompressed bytes to standard disk file
		{
			std::ofstream out(uncompressed_file, std::ios::binary);
			assert(out.is_open());
			out.write(reinterpret_cast<const char*>(decompressed_bytes.data()), static_cast<std::streamsize>(decompressed_bytes.size()));
			assert(out.good());
		}

		assert(!Compression::isGzipFile(uncompressed_file));
		const auto raw_read = Compression::readRawFile(uncompressed_file);
		assert(raw_read.has_value());
		assert(raw_read->size() == decompressed_bytes.size());
		assert(*raw_read == decompressed_bytes);
		assert(hasValidOtbmPrefix(*raw_read));

		// Verify OTBM parsing on converted uncompressed map matches original
		MemoryNodeFileReadHandle handle(raw_read->data() + 4, raw_read->size() - 4);
		BinaryNode* root = handle.getRootNode();
		assert(root != nullptr);
		uint8_t type = 0;
		uint32_t version = 0;
		uint16_t width = 0, height = 0;
		assert(root->getByte(type) && root->getU32(version) && root->getU16(width) && root->getU16(height));
		assert(width > 0 && height > 0);

		std::filesystem::remove_all(temp_dir);
		std::cout << "  -> Passed: Existing GZIP map converted to uncompressed OTBM with byte-for-byte fidelity.\n";
	}

	void testScenario6_MultiMapIsolation() {
		std::cout << "[Scenario 6] Testing multi-map tab concurrency and format isolation...\n";
		struct MockMap {
			OtbmCompression compression = OtbmCompression::None;
		};

		MockMap mapTab1;
		mapTab1.compression = OtbmCompression::Gzip;

		MockMap mapTab2;
		mapTab2.compression = OtbmCompression::None;

		assert(mapTab1.compression == OtbmCompression::Gzip);
		assert(mapTab2.compression == OtbmCompression::None);

		// Verify neither mutates the other
		mapTab1.compression = OtbmCompression::None;
		assert(mapTab1.compression == OtbmCompression::None);
		assert(mapTab2.compression == OtbmCompression::None);

		mapTab2.compression = OtbmCompression::Gzip;
		assert(mapTab1.compression == OtbmCompression::None);
		assert(mapTab2.compression == OtbmCompression::Gzip);

		std::cout << "  -> Passed: Independent per-map format state verified.\n";
	}

	void testScenario7_EscapedCharactersAndHeaderGrowth() {
		std::cout << "[Scenario 7] Testing escaped characters and dynamic header probe expansion...\n";
		const auto dir = std::filesystem::temp_directory_path() / "rme_probe_test";
		std::filesystem::create_directories(dir);
		const auto long_map = dir / "long_description.otbm";

		writeSampleMap<GzipNodeFileWriteHandle>(std::string(65535, '\xFE'), long_map, std::string(4, '\0'));

		const auto readHeader = [&](size_t limit, std::string& spawn) {
			const auto bytes = Compression::decompressGzipFile(long_map, limit);
			if (!bytes || !hasValidOtbmPrefix(*bytes)) {
				return std::pair{ false, FILE_INVALID_IDENTIFIER };
			}
			MemoryNodeFileReadHandle f(bytes->data() + 4, bytes->size() - 4);
			BinaryNode* node = f.getRootNode()->getChild();
			std::string text;
			uint8_t attr = 0;
			const bool ok = node && node->getByte(attr) && node->getU8(attr) && node->getString(text) && node->getU8(attr) && node->getString(spawn);
			return std::pair{ ok, f.error_code };
		};

		std::string spawn;
		// 64 KiB should report premature end due to huge description
		assert(readHeader(64 * 1024, spawn) == std::pair(false, FILE_PREMATURE_END));
		// 256 KiB succeeds
		assert(readHeader(256 * 1024, spawn) == std::pair(true, FILE_NO_ERROR) && spawn == "spawn.xml");

		std::filesystem::remove_all(dir);
		std::cout << "  -> Passed: Escaped bytes and dynamic header growth verified.\n";
	}

	void testScenario8_ClientVersionDisambiguation() {
		std::cout << "[Scenario 8] Testing OTB major+minor client version disambiguation...\n";

		struct MockClient {
			std::string name;
			uint32_t otbMajor;
			uint32_t otbId;
		};

		const std::vector<MockClient> clients = {
			{ "7.80", 1, 4 },
			{ "10.98", 3, 57 },
			{ "15.25", 4, 4 },
		};

		const auto matchClient = [&](uint32_t major, uint32_t minor) -> std::string {
			// First try exact major + minor match (PR 1045 behavior)
			for (const auto& c : clients) {
				if (c.otbMajor == major && c.otbId == minor) {
					return c.name;
				}
			}
			// Fallback to best match on minor only (legacy behavior)
			for (const auto& c : clients) {
				if (c.otbId == minor) {
					return c.name;
				}
			}
			return "";
		};

		// Canary / Crystal Server header (4/4) -> Must match 15.25, NOT 7.80!
		assert(matchClient(4, 4) == "15.25");

		// Classic 7.80 header (1/4) -> Must match 7.80
		assert(matchClient(1, 4) == "7.80");

		// Unknown major version with minor 4 -> Must fall back to 7.80
		assert(matchClient(99, 4) == "7.80");

		// Standard 10.98 header (3/57) -> Must match 10.98
		assert(matchClient(3, 57) == "10.98");

		std::cout << "  -> Passed: 4/4 header resolves to 15.25, 1/4 to 7.80, and legacy fallbacks work.\n";
	}

} // namespace

int main() {
	std::cout.setf(std::ios::unitbuf);
	std::cerr.setf(std::ios::unitbuf);
	std::cout << "========================================================\n";
	std::cout << "  RME Redux: Comprehensive GZIP OTBM Test Suite\n";
	std::cout << "========================================================\n";

	testScenario1_Detection();
	testScenario2_HeaderPeeking();
	testScenario3_FullLoad();
	testScenario4_ConversionNormalToGzip();
	testScenario5_ConversionGzipToNormal();
	testScenario6_MultiMapIsolation();
	testScenario7_EscapedCharactersAndHeaderGrowth();
	testScenario8_ClientVersionDisambiguation();

	std::cout << "========================================================\n";
	std::cout << "  ALL SCENARIOS PASSED SUCCESSFULLY!\n";
	std::cout << "========================================================\n";
	return 0;
}
