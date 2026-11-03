//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "io/sec/data/server_data_folder.h"

#include "io/sec/sec_file_io.h"
#include "io/sec/srv_script_lexer.h"
#include "util/file_system.h"

#include <format>
#include <iterator>

namespace rme::sec {

	namespace detail {

		bool hasExtension(const std::filesystem::path& path, std::string_view extension) {
			return srv::equalsIgnoreCase(path.extension().string(), extension);
		}

		bool readServerFile(const std::filesystem::path& path, std::string& text, std::vector<std::string>& warnings) {
			if (readFile(path, text)) {
				return true;
			}
			warnings.push_back(std::format("{}: could not be read, skipped.", FileSystem::PathToUtf8(path.filename())));
			return false;
		}

	} // namespace detail

	std::string listNameCounts(const std::map<std::string, size_t>& names) {
		constexpr size_t kMaxListedNames = 20;
		std::string out;
		size_t listed = 0;
		for (const auto& [name, count] : names) {
			if (listed++ == kMaxListedNames) {
				out += ", ...";
				break;
			}
			std::format_to(std::back_inserter(out), "{}{} (x{})", out.empty() ? "" : ", ", name, count);
		}
		return out;
	}

	void ServerFileWriter::write(const std::filesystem::path& path, std::string_view content) {
		if (writeFileIfChanged(path, content, scratch_) == WriteResult::Failed) {
			failed_.push_back(FileSystem::PathToUtf8(path));
		}
	}

} // namespace rme::sec
