#ifndef RME_XML_FILE_LOADER_H_
#define RME_XML_FILE_LOADER_H_

#include "app/definitions.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace XmlFileLoader {
	using XmlChildVisitor = std::function<bool(const std::filesystem::path&, pugi::xml_node, std::string&, std::vector<std::string>&)>;

	[[nodiscard]] std::filesystem::path resolveRelative(const std::filesystem::path& source_file, const std::string& relative_path);
	[[nodiscard]] bool visitElements(const std::filesystem::path& entry_file, const char* root_name, const XmlChildVisitor& visitor, std::string& error, std::vector<std::string>& warnings);
}

#endif
