// Round-trip check for gzip-compressed OTBM maps (Crystal Server format).
// Build (MSVC, from repo root):
//   cl /std:c++latest /EHsc /I source /I <zlib>/include tests/test_otbm_gzip_cpp.cpp <zlib>/lib/zs.lib
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Keep filehandle.cpp free of the heavy wx/app includes.
#define RME_MAIN_H_
#define ASSERT assert

#include "io/filehandle.cpp"
#include "io/otbm/otbm_file_reader.cpp"

namespace {

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
		for (int i = 0; i < 200000; ++i) {
			f.addNode(4);
			f.addU32(static_cast<uint32_t>(i));
			f.endNode();
		}
		f.endNode();
		f.endNode();
		f.close();
		assert(f.isOk());
	}

}

int main() {
	const auto dir = std::filesystem::temp_directory_path() / "rme_otbm_gzip_test";
	std::filesystem::create_directories(dir);
	const auto plain = dir / "plain.otbm";
	const auto packed = dir / L"packed_\u00f1.otbm";

	const std::string escaped_description("desc \xFE\xFD\xFF end");
	writeSampleMap<DiskNodeFileWriteHandle>(escaped_description, plain.string(), std::string(4, '\0'));
	writeSampleMap<GzipNodeFileWriteHandle>(escaped_description, packed, std::string(4, '\0'));

	assert(!OTBMFileReader::isGzipFile(plain));
	assert(OTBMFileReader::isGzipFile(packed));
	assert(std::filesystem::file_size(packed) < std::filesystem::file_size(plain));

	const auto plain_bytes = OTBMFileReader::readMapBytes(plain);
	const auto packed_bytes = OTBMFileReader::readMapBytes(packed);
	assert(plain_bytes && packed_bytes);
	assert(*plain_bytes == *packed_bytes);
	assert(OTBMFileReader::hasValidOtbmPrefix(*packed_bytes));

	const auto prefix = OTBMFileReader::readMapBytes(packed, 4096);
	assert(prefix && prefix->size() == 4096);
	assert(std::equal(prefix->begin(), prefix->end(), packed_bytes->begin()));

	MemoryNodeFileReadHandle handle(prefix->data() + 4, prefix->size() - 4);
	BinaryNode* root = handle.getRootNode();
	uint8_t type = 0;
	uint32_t version = 0;
	uint16_t width = 0;
	assert(root && root->getByte(type) && root->getU32(version) && root->getU16(width));
	assert(version == 2 && width == 1024);
	BinaryNode* map_data = root->getChild();
	std::string description;
	assert(map_data && map_data->getByte(type) && type == 2);
	assert(map_data->getU8(type) && type == 1 && map_data->getString(description));
	assert(description == escaped_description);

	// A max-length description of escaped bytes doubles on disk and overflows the 64 KiB
	// header probe; the truncation must be reported so iomap_otbm retries with more bytes.
	const auto long_map = dir / "long_description.otbm";
	writeSampleMap<GzipNodeFileWriteHandle>(std::string(65535, '\xFE'), long_map, std::string(4, '\0'));
	const auto readHeader = [&](size_t limit, std::string& spawn) {
		const auto bytes = OTBMFileReader::readMapBytes(long_map, limit);
		MemoryNodeFileReadHandle f(bytes->data() + 4, bytes->size() - 4);
		BinaryNode* node = f.getRootNode()->getChild();
		std::string text;
		uint8_t attr = 0;
		const bool ok = node->getByte(attr) && node->getU8(attr) && node->getString(text) && node->getU8(attr) && node->getString(spawn);
		return std::pair { ok, f.error_code };
	};
	std::string spawn;
	assert(readHeader(64 * 1024, spawn) == std::pair(false, FILE_PREMATURE_END));
	assert(readHeader(256 * 1024, spawn) == std::pair(true, FILE_NO_ERROR) && spawn == "spawn.xml");

	std::vector<uint8_t> corrupt = *OTBMFileReader::readMapBytes(plain, 16);
	corrupt[0] = 'X';
	assert(!OTBMFileReader::hasValidOtbmPrefix(corrupt));

	std::filesystem::remove_all(dir);
	std::cout << "otbm gzip: plain " << plain_bytes->size() << " bytes, round-trip OK\n";
	return 0;
}
