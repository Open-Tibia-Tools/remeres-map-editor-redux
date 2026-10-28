//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_FILE_SYSTEM_H_
#define RME_UTIL_FILE_SYSTEM_H_

#include <filesystem>
#include <string>
#include <wx/wx.h>

class FileSystem {
public:
	static wxString GetExecDirectory();
	static wxString GetDataDirectory();
	static wxString GetLocalDataDirectory();
	static wxString GetLocalDirectory();
	static wxString GetExtensionsDirectory();

	static void DiscoverDataDirectory(const wxString& existentFile);
	static wxString GetFoundDataDirectory();

	[[nodiscard]] static std::filesystem::path GetConfigSavePath();
	[[nodiscard]] static std::filesystem::path GetConfigFilePath();
	[[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);

private:
	static wxString m_dataDirectory;
};

#endif
