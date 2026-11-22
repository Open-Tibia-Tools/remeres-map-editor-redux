#include "game/material_include_resolver.h"

#include <algorithm>
#include <filesystem>
#include <format>

namespace fs = std::filesystem;

namespace MaterialIncludeResolver {

std::vector<fs::path> collectXmlFiles(const fs::path& directory, bool recursive) {
	std::vector<fs::path> result;
	std::error_code ec;
	if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec)) {
		return result;
	}

	auto check_and_add = [&](const fs::directory_entry& entry) {
		std::error_code entry_ec;
		if (entry.is_regular_file(entry_ec) && entry.path().extension() == ".xml") {
			result.emplace_back(entry.path());
		}
	};

	if (recursive) {
		for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
			check_and_add(entry);
		}
	} else {
		for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
			check_and_add(entry);
		}
	}

	std::ranges::sort(result, [](const fs::path& lhs, const fs::path& rhs) {
		return lhs.string() < rhs.string();
	});
	return result;
}

fs::path resolveRelativePath(const fs::path& baseFile, const std::string& relative) {
	const fs::path basePath = baseFile.parent_path();
	return (basePath / fs::u8path(relative)).lexically_normal();
}

fs::path resolveRelativeFolder(const fs::path& baseFile, const std::string& relative) {
	const fs::path basePath = baseFile.parent_path();
	return (basePath / fs::u8path(relative)).lexically_normal();
}

bool collectSectionFiles(const fs::path& manifestFile, pugi::xml_node section, std::string_view sectionName, std::vector<fs::path>& out, std::string& error) {
	bool loadedAny = false;
	for (pugi::xml_node includeNode = section.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
		if (const auto fileAttribute = includeNode.attribute("file")) {
			fs::path includeFile = resolveRelativePath(manifestFile, fileAttribute.as_string());
			std::error_code ec;
			if (!fs::is_regular_file(includeFile, ec)) {
				error = std::format("Missing modular include file: {}", includeFile.string());
				return false;
			}
			out.push_back(includeFile);
			loadedAny = true;
			continue;
		}

		if (const auto folderAttribute = includeNode.attribute("folder")) {
			fs::path folder = resolveRelativeFolder(manifestFile, folderAttribute.as_string());
			std::error_code ec;
			if (!fs::is_directory(folder, ec)) {
				error = std::format("Missing modular include folder: {}", folder.string());
				return false;
			}
			std::vector<fs::path> folderFiles = collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false));
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

