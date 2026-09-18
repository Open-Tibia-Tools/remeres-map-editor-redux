#ifndef RME_GAME_MATERIAL_INCLUDE_RESOLVER_H_
#define RME_GAME_MATERIAL_INCLUDE_RESOLVER_H_

#include "app/definitions.h"
#include "ext/pugixml.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace MaterialIncludeResolver {
	[[nodiscard]] std::vector<std::filesystem::path> collectXmlFiles(const std::filesystem::path& directory, bool recursive);
	[[nodiscard]] std::filesystem::path resolveRelativePath(const std::filesystem::path& baseFile, const std::string& relative);
	[[nodiscard]] std::filesystem::path resolveRelativeFolder(const std::filesystem::path& baseFile, const std::string& relative);
	[[nodiscard]] bool collectSectionFiles(const std::filesystem::path& manifestFile, pugi::xml_node section, std::string_view sectionName, std::vector<std::filesystem::path>& out, std::string& error);
}

#endif
