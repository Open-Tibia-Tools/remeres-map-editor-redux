#ifndef RME_IO_COMPRESSION_GZIP_STREAM_H_
#define RME_IO_COMPRESSION_GZIP_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace Compression {

	[[nodiscard]] bool isGzip(std::span<const uint8_t> data) noexcept;
	[[nodiscard]] bool isGzipFile(const std::filesystem::path& path);

	// Reads the uncompressed size from the GZIP footer (ISIZE) if available.
	// Returns std::nullopt if the file is not a valid GZIP or shorter than 18 bytes.
	[[nodiscard]] std::optional<uint32_t> readGzipUncompressedSize(const std::filesystem::path& path);

	// Decompresses a GZIP file into a contiguous buffer.
	// Pre-allocates buffer capacity using ISIZE or estimated size to avoid reallocations (DOD).
	// If limit is specified, reading stops once `limit` bytes have been inflated.
	[[nodiscard]] std::optional<std::vector<uint8_t>> decompressGzipFile(
		const std::filesystem::path& path,
		size_t limit = std::numeric_limits<size_t>::max()
	);

	// Reads raw bytes from a file up to `limit`.
	[[nodiscard]] std::optional<std::vector<uint8_t>> readRawFile(
		const std::filesystem::path& path,
		size_t limit = std::numeric_limits<size_t>::max()
	);

} // namespace Compression

#endif // RME_IO_COMPRESSION_GZIP_STREAM_H_
