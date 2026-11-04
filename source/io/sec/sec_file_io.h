//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SEC_FILE_IO_H_
#define RME_IO_SEC_SEC_FILE_IO_H_

// Reading and writing the original server's text files.

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace rme::sec {

	inline bool readFile(const std::filesystem::path& path, std::string& out) {
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream) {
			return false;
		}
		const std::streamsize size = stream.tellg();
		if (size < 0) {
			return false;
		}
		out.resize(static_cast<size_t>(size));
		stream.seekg(0);
		return static_cast<bool>(stream.read(out.data(), size));
	}

	enum class WriteResult {
		Unchanged, // The file already had this content and was not touched
		Written,
		Failed,
	};

	// Writes `content` to a temporary file next to `path`, then renames it
	// over `path`, so a failed save never leaves a half-written file.
	inline bool writeFileAtomically(const std::filesystem::path& path, std::string_view content) {
		std::filesystem::path temporary = path;
		temporary += ".tmp";
		{
			std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
			if (!stream || !stream.write(content.data(), static_cast<std::streamsize>(content.size())) || !stream.flush()) {
				std::error_code ec;
				std::filesystem::remove(temporary, ec);
				return false;
			}
		}
		std::error_code ec;
		std::filesystem::rename(temporary, path, ec);
		if (ec) {
			std::filesystem::remove(temporary, ec);
			return false;
		}
		return true;
	}

	inline WriteResult writeFileIfChanged(const std::filesystem::path& path, std::string_view content, std::string& scratch) {
		if (readFile(path, scratch) && scratch == content) {
			return WriteResult::Unchanged;
		}
		return writeFileAtomically(path, content) ? WriteResult::Written : WriteResult::Failed;
	}

	// The line ending a file uses, so a rewritten file keeps it.
	[[nodiscard]] inline std::string_view newlineOf(std::string_view text) {
		return text.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
	}

} // namespace rme::sec

#endif
