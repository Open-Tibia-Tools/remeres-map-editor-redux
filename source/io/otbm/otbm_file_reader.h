#ifndef RME_IO_OTBM_OTBM_FILE_READER_H_
#define RME_IO_OTBM_OTBM_FILE_READER_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace OTBMFileReader {

	bool isGzip(std::span<const uint8_t> data);
	bool isGzipFile(const std::filesystem::path& path);

	// OTBM identifier ("OTBM" or four zero bytes) followed by the root NODE_START.
	bool hasValidOtbmPrefix(std::span<const uint8_t> data);

	// Reads the whole map file (or its first `limit` bytes), transparently inflating gzip-compressed maps.
	std::optional<std::vector<uint8_t>> readMapBytes(const std::filesystem::path& path, size_t limit = std::numeric_limits<size_t>::max());

}

#endif
