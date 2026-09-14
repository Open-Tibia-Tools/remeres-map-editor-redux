//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_PERSISTENCE_MAP_BACKUP_SERVICE_H_
#define RME_EDITOR_PERSISTENCE_MAP_BACKUP_SERVICE_H_

#include <string>
#include <vector>

class Editor;

class MapBackupService {
public:
	/// Creates a non-destructive safety snapshot of the current in-memory map.
	/// Preserves live editor map state (filename, waypoint filename, unnamed status, dirty state).
	/// Suitable for pre-operation safety snapshots (cleaner, bulk edits) and auto-save.
	static bool CreateSnapshot(
		Editor& editor,
		std::string& out_backup_path,
		std::string& out_error,
		const std::string& operation_tag = "backup"
	);

	/// Generates a standardized timestamp string formatted as YYYYMMDD_HHMMSS (or YYYY-MM-DD-HH-MM-SS if not compact).
	static std::string GenerateTimestampString(bool compact = true);

	/// Writes the crash recovery marker (.saving.txt) with tracked backup files.
	static void WriteCrashMarker(const std::vector<std::string>& files);

	/// Clears the crash recovery marker file (.saving.txt).
	static void ClearCrashMarker();
};

#endif // RME_EDITOR_PERSISTENCE_MAP_BACKUP_SERVICE_H_
