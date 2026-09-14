//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "editor/persistence/map_backup_service.h"
#include "editor/editor.h"
#include "map/map.h"
#include "io/iomap_otbm.h"
#include "util/file_system.h"
#include <ctime>
#include <fstream>
#include <spdlog/spdlog.h>

std::string MapBackupService::GenerateTimestampString(bool compact) {
	std::time_t now = std::time(nullptr);
	std::tm tm_now = *std::localtime(&now);
	char time_buf[64];
	if (compact) {
		std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", &tm_now);
	} else {
		std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d-%H-%M-%S", &tm_now);
	}
	return std::string(time_buf);
}

void MapBackupService::WriteCrashMarker(const std::vector<std::string>& files) {
	std::string marker_path = nstr(FileSystem::GetLocalDataDirectory()) + ".saving.txt";
	std::ofstream f(marker_path.c_str(), std::ios::trunc | std::ios::out);
	for (const auto& file : files) {
		f << file << '\n';
	}
}

void MapBackupService::ClearCrashMarker() {
	std::string marker_path = nstr(FileSystem::GetLocalDataDirectory()) + ".saving.txt";
	std::remove(marker_path.c_str());
}

bool MapBackupService::CreateSnapshot(
	Editor& editor,
	std::string& out_backup_path,
	std::string& out_error,
	const std::string& operation_tag
) {
	const std::string time_str = GenerateTimestampString(true);

	FileName target_file;
	if (editor.map.hasFile()) {
		FileName src(wxstr(editor.map.getFilename()));
		target_file = src;
		target_file.SetName(src.GetName() + "_" + operation_tag + "_" + time_str);
	} else {
		std::string dir = nstr(FileSystem::GetLocalDataDirectory());
		target_file.Assign(wxstr(dir + "untitled_" + operation_tag + "_" + time_str + ".otbm"));
	}

	out_backup_path = nstr(target_file.GetFullPath());

	const std::string original_waypointfile = editor.map.getWaypointFilename();
	const bool original_changed = editor.map.hasChanged();
	const bool original_unnamed = editor.map.isUnnamed();

	IOMapOTBM mapsaver(editor.map.getVersion());
	const bool save_ok = mapsaver.saveMap(editor.map, target_file);

	// Restore waypointfile, unnamed state, and preserve dirty state on both success and failure paths
	editor.map.setWaypointFilename(original_waypointfile);
	editor.map.setUnnamed(original_unnamed);
	if (!original_changed && editor.map.hasChanged()) {
		editor.map.clearChanges();
	}

	if (!save_ok) {
		out_error = "Failed to save backup map snapshot to: " + out_backup_path;
		spdlog::error("MapBackupService::CreateSnapshot: {}", out_error);
		return false;
	}

	spdlog::info("MapBackupService::CreateSnapshot: Created map snapshot at {}", out_backup_path);
	return true;
}
