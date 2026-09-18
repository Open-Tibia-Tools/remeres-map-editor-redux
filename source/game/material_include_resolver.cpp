#include "game/material_include_resolver.h"

#include <algorithm>
#include <filesystem>
#include <format>

namespace fs = std::filesystem;

namespace MaterialIncludeResolver {

std::vector<FileName> collectXmlFiles(const FileName& directory, bool recursive) {
	std::vector<FileName> result;
	std::error_code ec;
	const fs::path dir_path = fs::u8path(directory.GetFullPath().ToStdString());
	if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
		return result;
	}

	auto check_and_add = [&](const fs::directory_entry& entry) {
		std::error_code entry_ec;
		if (entry.is_regular_file(entry_ec) && entry.path().extension() == ".xml") {
			result.emplace_back(wxstr(entry.path().string()));
		}
	};

	if (recursive) {
		for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
			check_and_add(entry);
		}
	} else {
		for (const auto& entry : fs::directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
			check_and_add(entry);
		}
	}

	std::ranges::sort(result, [](const FileName& lhs, const FileName& rhs) {
		return lhs.GetFullPath().CmpNoCase(rhs.GetFullPath()) < 0;
	});
	return result;
}

FileName resolveRelativePath(const FileName& baseFile, const std::string& relative) {
	const fs::path basePath = fs::u8path(baseFile.GetFullPath().ToStdString()).parent_path();
	const fs::path resolved = (basePath / fs::u8path(relative)).lexically_normal();
	return FileName(wxstr(resolved.string()));
}

FileName resolveRelativeFolder(const FileName& baseFile, const std::string& relative) {
	const fs::path basePath = fs::u8path(baseFile.GetFullPath().ToStdString()).parent_path();
	fs::path resolved = (basePath / fs::u8path(relative)).lexically_normal();
	std::string folderStr = resolved.string();
	if (!folderStr.empty() && folderStr.back() != '/' && folderStr.back() != '\\') {
		folderStr += '/';
	}
	return FileName(wxstr(folderStr));
}

bool collectSectionFiles(const FileName& manifestFile, pugi::xml_node section, std::string_view sectionName, std::vector<FileName>& out, std::string& error) {
	bool loadedAny = false;
	for (pugi::xml_node includeNode = section.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
		if (const auto fileAttribute = includeNode.attribute("file")) {
			FileName includeFile = resolveRelativePath(manifestFile, fileAttribute.as_string());
			if (!includeFile.FileExists()) {
				error = std::format("Missing modular include file: {}", includeFile.GetFullPath().ToStdString());
				return false;
			}
			out.push_back(includeFile);
			loadedAny = true;
			continue;
		}

		if (const auto folderAttribute = includeNode.attribute("folder")) {
			FileName folder = resolveRelativeFolder(manifestFile, folderAttribute.as_string());
			if (!folder.DirExists()) {
				error = std::format("Missing modular include folder: {}", folder.GetFullPath().ToStdString());
				return false;
			}
			std::vector<FileName> folderFiles = collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false));
			loadedAny = loadedAny || !folderFiles.empty();
			out.insert(out.end(), folderFiles.begin(), folderFiles.end());
		}
	}

	if (!loadedAny) {
		error = std::format("Modular section has no include entries: {}", sectionName);
		return false;
	}
	return true;
}

}

