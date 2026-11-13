//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "util/file_system.h"
#include "app/settings.h"

#include <spdlog/spdlog.h>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdlib>
#else
#include <unistd.h>
#include <cstdlib>
#endif

std::filesystem::path FileSystem::m_dataDirectory;

std::filesystem::path FileSystem::GetExecDirectory() {
#ifdef _WIN32
	wchar_t buffer[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (len > 0) {
		return std::filesystem::path(buffer).parent_path();
	}
	return std::filesystem::current_path();
#elif defined(__APPLE__)
	char buffer[1024];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0) {
		std::error_code ec;
		return std::filesystem::canonical(buffer, ec).parent_path();
	}
	return std::filesystem::current_path();
#else
	char buffer[1024];
	ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (len != -1) {
		buffer[len] = '\0';
		return std::filesystem::path(buffer).parent_path();
	}
	return std::filesystem::current_path();
#endif
}

std::filesystem::path FileSystem::GetDataDirectory() {
	std::string cfg_str = g_settings.getString(Config::DATA_DIRECTORY);
	if (!cfg_str.empty()) {
		std::filesystem::path dir(cfg_str);
		std::error_code ec;
		if (std::filesystem::is_directory(dir, ec)) {
			return dir;
		}
	}
	return GetExecDirectory() / "data";
}

std::filesystem::path FileSystem::GetLocalDirectory() {
	std::filesystem::path dir;
	if (g_settings.getInteger(Config::INDIRECTORY_INSTALLATION)) {
		dir = GetDataDirectory() / "user";
	} else {
#ifdef _WIN32
		const wchar_t* appdata = _wgetenv(L"APPDATA");
		if (appdata) {
			dir = std::filesystem::path(appdata) / "Remere's Map Editor";
		} else {
			dir = GetExecDirectory() / "user";
		}
#elif defined(__APPLE__)
		const char* home = getenv("HOME");
		if (home) {
			dir = std::filesystem::path(home) / "Library" / "Application Support" / "Remere's Map Editor";
		} else {
			dir = GetExecDirectory() / "user";
		}
#else
		const char* home = getenv("HOME");
		if (home) {
			dir = std::filesystem::path(home) / ".rme";
		} else {
			dir = GetExecDirectory() / "user";
		}
#endif
	}
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}

std::filesystem::path FileSystem::GetLocalDataDirectory() {
	std::filesystem::path dir;
	if (g_settings.getInteger(Config::INDIRECTORY_INSTALLATION)) {
		dir = GetDataDirectory() / "user" / "data";
	} else {
		dir = GetLocalDirectory() / "data";
	}
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}

std::filesystem::path FileSystem::GetExtensionsDirectory() {
	std::string cfg_str = g_settings.getString(Config::EXTENSIONS_DIRECTORY);
	if (!cfg_str.empty()) {
		std::filesystem::path dir(cfg_str);
		std::error_code ec;
		if (std::filesystem::is_directory(dir, ec)) {
			return dir;
		}
	}
	std::filesystem::path dir = GetLocalDirectory() / "extensions";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}

void FileSystem::DiscoverDataDirectory(const std::string& existentFile) {
	std::filesystem::path currentDir = std::filesystem::current_path();
	std::filesystem::path execDir = GetExecDirectory();

	std::vector<std::filesystem::path> possiblePaths = {
		execDir,
		currentDir,
		execDir / "..",
		execDir / ".." / "..",
		execDir / ".." / ".." / "..",
		currentDir / "..",
	};

	bool found = false;
	for (const auto& path : possiblePaths) {
		std::error_code ec;
		std::filesystem::path testPath = path / "data" / existentFile;
		if (std::filesystem::exists(testPath, ec)) {
			m_dataDirectory = (path / "data").lexically_normal();
			found = true;
			break;
		}
	}

	if (!found) {
		spdlog::error("Could not find data directory containing {}", existentFile);
	}
}

std::filesystem::path FileSystem::GetFoundDataDirectory() {
	return m_dataDirectory;
}

std::filesystem::path FileSystem::GetConfigSavePath() {
	return GetExecDirectory() / "config.toml";
}

std::filesystem::path FileSystem::GetConfigFilePath() {
	std::error_code ec;
	const auto own = GetConfigSavePath();
	if (std::filesystem::exists(own, ec)) {
		return own;
	}
	const auto legacy = std::filesystem::current_path(ec) / "config.toml";
	if (!ec && std::filesystem::exists(legacy, ec)) {
		return legacy;
	}
	return own;
}

std::string FileSystem::PathToUtf8(const std::filesystem::path& path) {
	const auto u8 = path.u8string();
	return std::string(u8.begin(), u8.end());
}
