#include "game/material_manifest.h"

#include "ext/pugixml.hpp"
#include "game/material_include_resolver.h"
#include "util/common.h"

#include <array>
#include <format>

namespace {

struct ManifestSection {
	std::string_view name;
	std::vector<FileName> MaterialManifestFiles::* files;
	bool* seen = nullptr;
};

[[nodiscard]] bool collectManifestSection(const FileName& manifestFile, pugi::xml_node node, const ManifestSection& section, MaterialManifestFiles& files, wxString& error) {
	*section.seen = true;
	return MaterialIncludeResolver::collectSectionFiles(manifestFile, node, section.name, files.*section.files, error);
}

} // namespace

bool LoadMaterialManifestFiles(const FileName& manifestFile, MaterialManifestFiles& files, wxString& error) {
	files = {};

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_file(manifestFile.GetFullPath().ToStdString().c_str());
	if (!result) {
		error = wxstr(std::format("Could not parse materials manifest: {}: {} at offset {}",
			manifestFile.GetFullPath().ToStdString(), result.description(), result.offset));
		return false;
	}

	pugi::xml_node root = doc.child("materials");
	if (!root) {
		error = "Invalid materials manifest root: " + manifestFile.GetFullPath();
		return false;
	}

	bool sawBorders = false;
	bool sawBrushes = false;
	bool sawCreatures = false;
	bool sawItems = false;
	bool sawTilesets = false;
	bool sawPalettes = false;
	const std::array sections {
		ManifestSection { .name = "borders", .files = &MaterialManifestFiles::borders, .seen = &sawBorders },
		ManifestSection { .name = "brushes", .files = &MaterialManifestFiles::brushes, .seen = &sawBrushes },
		ManifestSection { .name = "creatures", .files = &MaterialManifestFiles::creatures, .seen = &sawCreatures },
		ManifestSection { .name = "items", .files = &MaterialManifestFiles::items, .seen = &sawItems },
		ManifestSection { .name = "tilesets", .files = &MaterialManifestFiles::tilesets, .seen = &sawTilesets },
		ManifestSection { .name = "palettes", .files = &MaterialManifestFiles::palettes, .seen = &sawPalettes },
	};

	for (pugi::xml_node childNode = root.first_child(); childNode; childNode = childNode.next_sibling()) {
		const std::string childName = as_lower_str(childNode.name());
		if (childName == "tileset") {
			error = "Legacy <tileset> nodes are not supported by the modular material loader.";
			return false;
		}
		for (const ManifestSection& section : sections) {
			if (childName == section.name && !collectManifestSection(manifestFile, childNode, section, files, error)) {
				return false;
			}
		}
	}

	if (!sawBorders || !sawBrushes || !sawCreatures || !sawItems || !sawTilesets || !sawPalettes) {
		error = "Modular materials.xml must define borders, brushes, creatures, items, tilesets, and palettes sections.";
		return false;
	}
	return true;
}
