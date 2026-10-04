#include "io/compression/gzip_stream.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <memory>

#include <zlib.h>

namespace {

	constexpr size_t kChunkSize = 64 * 1024;
	constexpr size_t kGzipMinFileSize = 18; // 10 header + 8 trailer

	struct GzCloser {
		void operator()(gzFile_s* file) const noexcept {
			if (file) {
				gzclose(file);
			}
		}
	};
	using GzPtr = std::unique_ptr<gzFile_s, GzCloser>;

	gzFile openGz(const std::filesystem::path& path) {
#ifdef _WIN32
		return gzopen_w(path.c_str(), "rb");
#else
		return gzopen(path.c_str(), "rb");
#endif
	}

} // namespace

namespace Compression {

	bool isGzip(std::span<const uint8_t> data) noexcept {
		return data.size() >= 2 && data[0] == 0x1F && data[1] == 0x8B;
	}

	bool isGzipFile(const std::filesystem::path& path) {
		const auto header = readRawFile(path, 2);
		return header && isGzip(*header);
	}

	std::optional<uint32_t> readGzipUncompressedSize(const std::filesystem::path& path) {
		std::error_code ec;
		const auto size = std::filesystem::file_size(path, ec);
		if (ec || size < kGzipMinFileSize) {
			return std::nullopt;
		}

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			return std::nullopt;
		}

		file.seekg(static_cast<std::streamoff>(size - 4), std::ios::beg);
		if (!file.good()) {
			return std::nullopt;
		}

		uint8_t bytes[4];
		if (!file.read(reinterpret_cast<char*>(bytes), 4)) {
			return std::nullopt;
		}

		const uint32_t isize = static_cast<uint32_t>(bytes[0]) |
			(static_cast<uint32_t>(bytes[1]) << 8) |
			(static_cast<uint32_t>(bytes[2]) << 16) |
			(static_cast<uint32_t>(bytes[3]) << 24);

		return isize;
	}

	constexpr size_t kMaxDecompressedBudget = 2ULL * 1024 * 1024 * 1024; // 2 GiB safety budget
	constexpr size_t kMaxInitialReserve = 512 * 1024 * 1024; // 512 MiB initial reservation cap

	std::optional<std::vector<uint8_t>> decompressGzipFile(const std::filesystem::path& path, size_t limit) {
		GzPtr gz(openGz(path));
		if (!gz) {
			return std::nullopt;
		}
		gzbuffer(gz.get(), static_cast<unsigned>(kChunkSize));

		const size_t budget = std::min<size_t>(limit, kMaxDecompressedBudget);
		std::vector<uint8_t> out;

		// Pre-reserve memory using ISIZE footer as an untrusted hint (capped) or reasonable estimate (DOD)
		if (limit == std::numeric_limits<size_t>::max()) {
			const auto expected_size = readGzipUncompressedSize(path);
			if (expected_size && *expected_size > 0) {
				out.reserve(std::min<size_t>(*expected_size, kMaxInitialReserve));
			} else {
				std::error_code ec;
				const auto fsize = std::filesystem::file_size(path, ec);
				if (!ec && fsize > 0) {
					out.reserve(std::min<size_t>(static_cast<size_t>(std::min<uintmax_t>(fsize * 4, kMaxInitialReserve)), kMaxInitialReserve));
				}
			}
		} else {
			out.reserve(std::min<size_t>(budget, 256 * 1024));
		}

		while (out.size() < budget) {
			const auto wanted = static_cast<unsigned>(std::min<size_t>(kChunkSize, budget - out.size()));
			const size_t current_size = out.size();
			out.resize(current_size + wanted);

			const int read = gzread(gz.get(), out.data() + current_size, wanted);
			if (read < 0) {
				return std::nullopt;
			}

			out.resize(current_size + static_cast<size_t>(read));
			if (read == 0) {
				break;
			}
		}

		// If full decompression exceeded the maximum safety budget before EOF, reject excessive/corrupted file
		if (limit == std::numeric_limits<size_t>::max() && out.size() >= budget && !gzeof(gz.get())) {
			return std::nullopt;
		}

		// A truncated prefix read never reaches the gzip trailer, so its CRC can't be checked.
		const bool full_read = out.size() < limit;
		if (gzclose(gz.release()) != Z_OK && full_read) {
			return std::nullopt;
		}

		return out;
	}

	std::optional<std::vector<uint8_t>> readRawFile(const std::filesystem::path& path, size_t limit) {
		std::error_code ec;
		const auto size = std::filesystem::file_size(path, ec);
		if (ec) {
			return std::nullopt;
		}

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			return std::nullopt;
		}

		const size_t bytes_to_read = static_cast<size_t>(std::min<uintmax_t>(size, limit));
		std::vector<uint8_t> out(bytes_to_read);
		if (!file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(bytes_to_read))) {
			return std::nullopt;
		}

		return out;
	}

} // namespace Compression
