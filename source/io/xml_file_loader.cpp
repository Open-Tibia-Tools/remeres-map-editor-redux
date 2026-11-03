#include "io/xml_file_loader.h"

#include "util/common.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <unordered_set>

namespace {
	struct XmlVisitState {
		std::unordered_set<std::string> visited;
		std::unordered_set<std::string> visiting;
	};

	std::string normalizedPathKey(const FileName& filename) {
		FileName normalized(filename);
		normalized.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
		std::string path_key = normalized.GetFullPath().ToStdString();
#if defined(_WIN32) || defined(__APPLE__)
		std::transform(path_key.begin(), path_key.end(), path_key.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
#endif
		return path_key;
	}

	bool visitFile(const FileName& filename, const char* root_name, const XmlFileLoader::XmlChildVisitor& visitor, XmlVisitState& state, wxString& error, std::vector<std::string>& warnings) {
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
		const pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().ToStdString().c_str());
		if (!result) {
			error = wxString::FromUTF8(std::format("Could not open XML file {}.", filename.GetFullPath().utf8_string()));
			return false;
		}

		const pugi::xml_node root = doc.child(root_name);
		if (!root) {
			error = wxString::FromUTF8(std::format("XML file {} has an invalid <{}> root.", filename.GetFullPath().utf8_string(), root_name));
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

				const FileName include_file = XmlFileLoader::resolveRelative(filename, wxString::FromUTF8(include_attribute.as_string()));
				wxString include_error;
				if (!visitFile(include_file, root_name, visitor, state, include_error, warnings)) {
					error = include_error.empty()
						? wxString::FromUTF8(std::format("Failed to load XML include {}.", include_file.GetFullPath().ToStdString()))
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

FileName XmlFileLoader::resolveRelative(const FileName& source_file, const wxString& relative_path) {
	FileName resolved(relative_path);
	if (!resolved.IsAbsolute()) {
		resolved.Assign(source_file.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + relative_path);
	}
	resolved.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
	return resolved;
}

bool XmlFileLoader::visitElements(const FileName& entry_file, const char* root_name, const XmlChildVisitor& visitor, wxString& error, std::vector<std::string>& warnings) {
	XmlVisitState state;
	FileName normalized_entry(entry_file);
	normalized_entry.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
	return visitFile(normalized_entry, root_name, visitor, state, error, warnings);
}
