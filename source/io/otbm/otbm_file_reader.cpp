#include "io/otbm/otbm_file_reader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <memory>

#include <zlib.h>

namespace {

	constexpr uint8_t kNodeStart = 0xFE;
	constexpr size_t kChunkSize = 64 * 1024;

	struct GzCloser {
		void operator()(gzFile_s* file) const {
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

	std::optional<std::vector<uint8_t>> inflateFile(const std::filesystem::path& path, size_t limit) {
		GzPtr gz(openGz(path));
		if (!gz) {
			return std::nullopt;
		}
		gzbuffer(gz.get(), kChunkSize);

		std::vector<uint8_t> out;
		std::array<uint8_t, kChunkSize> chunk;
		while (out.size() < limit) {
			const auto wanted = static_cast<unsigned>(std::min(chunk.size(), limit - out.size()));
			const int read = gzread(gz.get(), chunk.data(), wanted);
			if (read < 0) {
				return std::nullopt;
			}
			if (read == 0) {
				break;
			}
			out.insert(out.end(), chunk.begin(), chunk.begin() + read);
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

		std::vector<uint8_t> out(static_cast<size_t>(std::min<uintmax_t>(size, limit)));
		if (!file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
			return std::nullopt;
		}
		return out;
	}

}

namespace OTBMFileReader {

	bool isGzip(std::span<const uint8_t> data) {
		return data.size() >= 2 && data[0] == 0x1F && data[1] == 0x8B;
	}

	bool isGzipFile(const std::filesystem::path& path) {
		const auto header = readRawFile(path, 2);
		return header && isGzip(*header);
	}

	bool hasValidOtbmPrefix(std::span<const uint8_t> data) {
		if (data.size() < 5) {
			return false;
		}
		constexpr uint8_t wildcard[4] = { 0, 0, 0, 0 };
		const bool known_id = std::memcmp(data.data(), "OTBM", 4) == 0 || std::memcmp(data.data(), wildcard, 4) == 0;
		return known_id && data[4] == kNodeStart;
	}

	std::optional<std::vector<uint8_t>> readMapBytes(const std::filesystem::path& path, size_t limit) {
		return isGzipFile(path) ? inflateFile(path, limit) : readRawFile(path, limit);
	}

}
