//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_SERVER_DATA_FOLDER_H_
#define RME_IO_SEC_DATA_SERVER_DATA_FOLDER_H_

// The data folder of an original game server holds the map folder (the
// sector files) and the server files around it:
//   dat/map.dat, dat/houseareas.dat, dat/houses.dat, dat/monster.db,
//   dat/moveuse.dat, mon/*.mon, mon/*.evt, npc/*.npc
// Each of them has its own handler next to this file (map_dat_file.h,
// house_files.h, monster_files.h, npc_files.h, server_marker_files.h).
//
// Only the sector files and dat/objects.srv are needed to open a map; every
// other file is optional. A map without them starts with no towns, houses or
// creatures, and saving creates the files that its data needs.

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rme::sec {

	struct ServerDataFolder {
		std::filesystem::path root;

		[[nodiscard]] std::filesystem::path dat() const {
			return root / "dat";
		}
		[[nodiscard]] std::filesystem::path mon() const {
			return root / "mon";
		}
		[[nodiscard]] std::filesystem::path npc() const {
			return root / "npc";
		}
	};

	// Calls `on_file(path, text)` for each file in `folder` whose extension is
	// `extension` (any case). Files that cannot be read are skipped with a
	// warning; a folder that does not exist has no files.
	template <typename OnFile>
	void forEachServerFile(const std::filesystem::path& folder, std::string_view extension, std::vector<std::string>& warnings, OnFile on_file);

	// "Rat (x3), Troll (x1)": names with how often they occur, for warnings.
	[[nodiscard]] std::string listNameCounts(const std::map<std::string, size_t>& names);

	// Writes server files, each one only when its content changed, creating
	// the files and folders that do not exist yet.
	class ServerFileWriter {
	public:
		void write(const std::filesystem::path& path, std::string_view content);

		// The files that did not exist before.
		[[nodiscard]] const std::vector<std::filesystem::path>& createdFiles() const {
			return created_;
		}

		// UTF-8 paths of the files that could not be written.
		[[nodiscard]] const std::vector<std::string>& failedFiles() const {
			return failed_;
		}

	private:
		std::string scratch_;
		std::vector<std::string> failed_;
		std::vector<std::filesystem::path> created_;
	};

	namespace detail {
		[[nodiscard]] bool hasExtension(const std::filesystem::path& path, std::string_view extension);
		[[nodiscard]] bool readServerFile(const std::filesystem::path& path, std::string& text, std::vector<std::string>& warnings);
	}

	template <typename OnFile>
	void forEachServerFile(const std::filesystem::path& folder, std::string_view extension, std::vector<std::string>& warnings, OnFile on_file) {
		std::error_code ec;
		std::string text;
		for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
			if (entry.is_regular_file(ec) && detail::hasExtension(entry.path(), extension) && detail::readServerFile(entry.path(), text, warnings)) {
				on_file(entry.path(), std::string_view(text));
			}
		}
	}

} // namespace rme::sec

#endif
