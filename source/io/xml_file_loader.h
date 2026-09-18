#ifndef RME_XML_FILE_LOADER_H_
#define RME_XML_FILE_LOADER_H_

#include "app/main.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace XmlFileLoader {
	using XmlChildVisitor = std::function<bool(const FileName&, pugi::xml_node, std::string&, std::vector<std::string>&)>;

	[[nodiscard]] FileName resolveRelative(const FileName& source_file, const std::string& relative_path);
	[[nodiscard]] bool visitElements(const FileName& entry_file, const char* root_name, const XmlChildVisitor& visitor, std::string& error, std::vector<std::string>& warnings);
}

#endif
