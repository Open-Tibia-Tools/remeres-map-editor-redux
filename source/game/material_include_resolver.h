#ifndef RME_GAME_MATERIAL_INCLUDE_RESOLVER_H_
#define RME_GAME_MATERIAL_INCLUDE_RESOLVER_H_

#include "app/main.h"
#include "ext/pugixml.hpp"

#include <string_view>
#include <vector>

namespace MaterialIncludeResolver {
	[[nodiscard]] std::vector<FileName> collectXmlFiles(const FileName& directory, bool recursive);
	[[nodiscard]] FileName resolveRelativePath(const FileName& baseFile, const std::string& relative);
	[[nodiscard]] FileName resolveRelativeFolder(const FileName& baseFile, const std::string& relative);
	[[nodiscard]] bool collectSectionFiles(const FileName& manifestFile, pugi::xml_node section, std::string_view sectionName, std::vector<FileName>& out, std::string& error);
}

#endif
