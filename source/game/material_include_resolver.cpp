#include "game/material_include_resolver.h"

#include <wx/dir.h>

namespace MaterialIncludeResolver {

std::vector<FileName> collectXmlFiles(const FileName& directory, bool recursive) {
	wxArrayString files;
	const int flags = wxDIR_FILES | (recursive ? wxDIR_DIRS : 0);
	wxDir::GetAllFiles(directory.GetFullPath(), &files, "*.xml", flags);

	std::vector<FileName> result;
	result.reserve(files.size());
	for (const auto& file : files) {
		FileName fn(file);
		fn.Normalize(wxPATH_NORM_ALL);
		result.emplace_back(std::move(fn));
	}
	std::ranges::sort(result, [](const FileName& lhs, const FileName& rhs) {
		return lhs.GetFullPath().CmpNoCase(rhs.GetFullPath()) < 0;
	});
	return result;
}

FileName resolveRelativePath(const FileName& baseFile, const std::string& relative) {
	wxString normalized = wxString(relative);
	normalized.Replace("\\", "/");
	FileName path(baseFile.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + normalized);
	path.Normalize(wxPATH_NORM_ALL);
	return path;
}

FileName resolveRelativeFolder(const FileName& baseFile, const std::string& relative) {
	wxString normalized = wxString(relative);
	normalized.Replace("\\", "/");
	if (!normalized.empty() && !normalized.EndsWith("/")) {
		normalized += "/";
	}
	FileName path(baseFile.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + normalized);
	path.Normalize(wxPATH_NORM_ALL);
	return path;
}

bool collectSectionFiles(const FileName& manifestFile, pugi::xml_node section, std::string_view sectionName, std::vector<FileName>& out, wxString& error) {
	bool loadedAny = false;
	for (pugi::xml_node includeNode = section.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
		if (const auto fileAttribute = includeNode.attribute("file")) {
			FileName includeFile = resolveRelativePath(manifestFile, fileAttribute.as_string());
			if (!includeFile.FileExists()) {
				error = "Missing modular include file: " + includeFile.GetFullPath();
				return false;
			}
			out.push_back(includeFile);
			loadedAny = true;
			continue;
		}

		if (const auto folderAttribute = includeNode.attribute("folder")) {
			FileName folder = resolveRelativeFolder(manifestFile, folderAttribute.as_string());
			if (!folder.DirExists()) {
				error = "Missing modular include folder: " + folder.GetFullPath();
				return false;
			}
			std::vector<FileName> folderFiles = collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false));
			loadedAny = loadedAny || !folderFiles.empty();
			out.insert(out.end(), folderFiles.begin(), folderFiles.end());
		}
	}

	if (!loadedAny) {
		error = "Modular section has no include entries: " + wxString(std::string(sectionName));
		return false;
	}
	return true;
}

}
