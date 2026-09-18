#include "io/xml_file_loader.h"

#include "util/common.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {
	struct XmlVisitState {
		std::unordered_set<std::string> visited;
		std::unordered_set<std::string> visiting;
	};

	std::string normalizedPathKey(const FileName& filename) {
		std::error_code ec;
		fs::path p = fs::weakly_canonical(filename.GetFullPath().ToStdWstring(), ec);
		if (ec) {
			p = fs::path(filename.GetFullPath().ToStdWstring()).lexically_normal();
		}
		std::string path_key = p.string();
#if defined(_WIN32) || defined(__APPLE__)
		std::transform(path_key.begin(), path_key.end(), path_key.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
#endif
		return path_key;
	}

	bool visitFile(const FileName& filename, const char* root_name, const XmlFileLoader::XmlChildVisitor& visitor, XmlVisitState& state, std::string& error, std::vector<std::string>& warnings) {
		const std::string path_key = normalizedPathKey(filename);
		if (state.visiting.contains(path_key)) {
			warnings.push_back(std::format("Skipping cyclic XML include: {}", path_key));
			return true;
		}
		if (state.visited.contains(path_key)) {
			warnings.push_back(std::format("Skipping duplicate XML include: {}", path_key));
			return true;
		}

		pugi::xml_document doc;
		const pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
		if (!result) {
			error = std::format("Could not open XML file {}.", filename.GetFullPath().ToStdString());
			return false;
		}

		const pugi::xml_node root = doc.child(root_name);
		if (!root) {
			error = std::format("XML file {} has an invalid <{}> root.", filename.GetFullPath().ToStdString(), root_name);
			return false;
		}

		state.visiting.insert(path_key);

		for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
			if (as_lower_str(child.name()) == "include") {
				const auto include_attribute = child.attribute("file");
				if (!include_attribute) {
					warnings.push_back(std::format("XML include inside {} is missing a file attribute.", path_key));
					continue;
				}

				const FileName include_file = XmlFileLoader::resolveRelative(filename, include_attribute.as_string());
				std::string include_error;
				if (!visitFile(include_file, root_name, visitor, state, include_error, warnings)) {
					error = include_error.empty()
						? std::format("Failed to load XML include {}.", include_file.GetFullPath().ToStdString())
						: include_error;
					state.visiting.erase(path_key);
					return false;
				}
				continue;
			}

			if (!visitor(filename, child, error, warnings)) {
				state.visiting.erase(path_key);
				return false;
			}
		}

		state.visiting.erase(path_key);
		state.visited.insert(path_key);
		return true;
	}
}

FileName XmlFileLoader::resolveRelative(const FileName& source_file, const std::string& relative_path) {
	fs::path rel(relative_path);
	fs::path base = fs::path(source_file.GetFullPath().ToStdWstring()).parent_path();
	fs::path resolved = rel.is_absolute() ? rel : (base / rel);
	std::error_code ec;
	fs::path canon = fs::weakly_canonical(resolved, ec);
	if (ec) {
		canon = resolved.lexically_normal();
	}
	return FileName(wxstr(canon.string()));
}

bool XmlFileLoader::visitElements(const FileName& entry_file, const char* root_name, const XmlChildVisitor& visitor, std::string& error, std::vector<std::string>& warnings) {
	XmlVisitState state;
	return visitFile(entry_file, root_name, visitor, state, error, warnings);
}
