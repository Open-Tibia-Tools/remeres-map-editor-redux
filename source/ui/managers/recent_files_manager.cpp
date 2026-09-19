#include "ui/managers/recent_files_manager.h"
#include <toml++/toml.h>
#include <wx/filename.h>
#include "app/settings.h"

RecentFilesManager::RecentFilesManager() :
	recentFiles(10) {
}

RecentFilesManager::~RecentFilesManager() {
}

void RecentFilesManager::Load() {
	toml::table& table = g_settings.getTable();
	bool changed = false;
	if (auto editor = table["editor"].as_table()) {
		if (auto files = (*editor)["recent_files"].as_array()) {
			// Add in reverse order to preserve history ranking
			for (size_t i = files->size(); i > 0; --i) {
				auto& node = (*files)[i - 1];
				if (auto val = node.as_string()) {
					wxString path = wxstr(val->get());
					if (wxFileName::FileExists(path)) {
						recentFiles.AddFileToHistory(path);
					} else {
						files->erase(files->begin() + (i - 1));
						changed = true;
					}
				}
			}
		}
	}

	if (changed) {
		g_settings.save();
	}
}

void RecentFilesManager::Save() {
	toml::table& table = g_settings.getTable();
	toml::table* editor = table.get_as<toml::table>("editor");
	if (!editor) {
		table.insert_or_assign("editor", toml::table {});
		editor = table.get_as<toml::table>("editor");
	}

	toml::array files_array;
	for (size_t i = 0; i < recentFiles.GetCount(); ++i) {
		files_array.push_back(recentFiles.GetHistoryFile(i).ToStdString());
	}

	editor->insert_or_assign("recent_files", std::move(files_array));
	g_settings.save();
}

void RecentFilesManager::AddFile(const FileName& file) {
	recentFiles.AddFileToHistory(wxString(file.wstring()));
}

void RecentFilesManager::UseMenu(wxMenu* menu) {
	recentFiles.UseMenu(menu);
	recentFiles.AddFilesToMenu();
}

std::vector<wxString> RecentFilesManager::GetFiles() const {
	std::vector<wxString> files(recentFiles.GetCount());
	for (size_t i = 0; i < recentFiles.GetCount(); ++i) {
		files[i] = recentFiles.GetHistoryFile(i);
	}
	return files;
}

int RecentFilesManager::GetBaseId() const {
	return recentFiles.GetBaseId();
}

size_t RecentFilesManager::GetCount() const {
	return recentFiles.GetCount();
}

wxString RecentFilesManager::GetFile(size_t index) const {
	if (index < recentFiles.GetCount()) {
		return recentFiles.GetHistoryFile(index);
	}
	return "";
}
