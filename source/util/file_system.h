//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_FILE_SYSTEM_H_
#define RME_UTIL_FILE_SYSTEM_H_

#include <filesystem>
#include <string>

class FileSystem {
public:
	static std::filesystem::path GetExecDirectory();
	static std::filesystem::path GetDataDirectory();
	static std::filesystem::path GetLocalDataDirectory();
	static std::filesystem::path GetLocalDirectory();
	static std::filesystem::path GetExtensionsDirectory();

	static void DiscoverDataDirectory(const std::string& existentFile);
	static std::filesystem::path GetFoundDataDirectory();

private:
	static std::filesystem::path m_dataDirectory;
};

#endif
