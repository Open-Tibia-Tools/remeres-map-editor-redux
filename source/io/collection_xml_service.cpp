#include "io/collection_xml_service.h"

#include "app/managers/version_manager.h"
#include "brushes/brush.h"
#include "game/materials.h"
#include "util/file_system.h"
#include "util/common.h"
#include "ext/pugixml.hpp"

#include <algorithm>
#include <set>
#include <format>
#include <spdlog/spdlog.h>

namespace rme::collection {


std::filesystem::path CollectionXmlService::GetCollectionsDirectory() {
	wxString modularPath = g_version.GetModularDataPath();
	std::filesystem::path baseDir;
	if (!modularPath.empty()) {
		baseDir = modularPath.ToStdWstring();
	} else {
		baseDir = std::filesystem::path(FileSystem::GetDataDirectory().ToStdWstring()) / "1098";
	}
	return baseDir / "tilesets" / "collections";
}

std::filesystem::path CollectionXmlService::GetPalettesXmlPath() {
	wxString modularPath = g_version.GetModularDataPath();
	std::filesystem::path baseDir;
	if (!modularPath.empty()) {
		baseDir = modularPath.ToStdWstring();
	} else {
		baseDir = std::filesystem::path(FileSystem::GetDataDirectory().ToStdWstring()) / "1098";
	}
	return baseDir / "palettes.xml";
}

std::filesystem::path CollectionXmlService::GetTilesetFilePath(std::string_view paletteName, std::string_view tilesetName) {
	std::string sanitized = SanitizeTilesetFilename(tilesetName);
	if (as_lower_str(std::string(paletteName)) == "collection") {
		return GetCollectionsDirectory() / (sanitized + ".xml");
	}

	std::filesystem::path palettesPath = GetPalettesXmlPath();
	std::error_code ec;

	// Check if this tileset already has an explicit include in palettes.xml under paletteName
	if (std::filesystem::exists(palettesPath, ec)) {
		pugi::xml_document doc;
		if (doc.load_file(palettesPath.wstring().c_str())) {
			pugi::xml_node root = doc.child("materials");
			if (!root) root = doc.child("palettes");
			if (!root) root = doc.first_child();

			for (pugi::xml_node pal = root ? root.child("palette") : doc.child("palette"); pal; pal = pal.next_sibling("palette")) {
				if (as_lower_str(pal.attribute("name").as_string()) == as_lower_str(std::string(paletteName))) {
					for (pugi::xml_node ts = pal.child("tileset"); ts; ts = ts.next_sibling("tileset")) {
						for (pugi::xml_node inc = ts.child("include"); inc; inc = inc.next_sibling("include")) {
							std::string rel = inc.attribute("file").as_string();
							if (rel.empty()) continue;
							std::filesystem::path full = palettesPath.parent_path() / rel;
							if (std::filesystem::exists(full, ec)) {
								pugi::xml_document incDoc;
								if (incDoc.load_file(full.wstring().c_str())) {
									if (pugi::xml_node tNode = incDoc.child("tileset")) {
										if (as_lower_str(tNode.attribute("name").as_string()) == as_lower_str(std::string(tilesetName))) {
											return full;
										}
									}
								}
							}
							if (as_lower_str(std::filesystem::path(rel).stem().string()) == as_lower_str(sanitized)) {
								return full;
							}
						}
					}
				}
			}
		}
	}

	// For a new tileset under another palette: place under tilesets/<palette_name_lower>/
	std::string folderName = as_lower_str(std::string(paletteName));
	std::filesystem::path targetDir = palettesPath.parent_path() / "tilesets" / folderName;
	return targetDir / (sanitized + ".xml");
}

std::vector<std::string> CollectionXmlService::GetAvailablePalettes() {
	std::vector<std::string> result;
	std::set<std::string> uniqueNames;

	// 1. From palettes.xml
	std::filesystem::path palettesPath = GetPalettesXmlPath();
	std::error_code ec;
	if (std::filesystem::exists(palettesPath, ec)) {
		pugi::xml_document doc;
		if (doc.load_file(palettesPath.wstring().c_str())) {
			pugi::xml_node root = doc.child("materials");
			if (!root) root = doc.child("palettes");
			if (!root) root = doc.first_child();

			for (pugi::xml_node pal = root ? root.child("palette") : doc.child("palette"); pal; pal = pal.next_sibling("palette")) {
				std::string name = pal.attribute("name").as_string();
				if (!name.empty() && as_lower_str(name) != "collection") {
					uniqueNames.insert(std::move(name));
				}
			}
		}
	}

	// 2. From runtime PaletteCatalog dynamicPalettes
	for (const auto& pal : g_materials.paletteCatalog().dynamicPalettes()) {
		if (!pal.name.empty() && as_lower_str(pal.name) != "collection") {
			uniqueNames.insert(pal.name);
		}
	}

	// "Collection" is guaranteed to be first
	result.push_back("Collection");
	for (const auto& name : uniqueNames) {
		result.push_back(name);
	}
	return result;
}

std::vector<std::string> CollectionXmlService::GetExistingTilesetNamesForPalette(std::string_view paletteName) {
	std::vector<std::string> names;
	std::set<std::string> uniqueNames;
	std::error_code ec;

	// If Collection, scan collections directory
	if (as_lower_str(std::string(paletteName)) == "collection") {
		std::filesystem::path collDir = GetCollectionsDirectory();
		if (std::filesystem::exists(collDir, ec) && std::filesystem::is_directory(collDir, ec)) {
			for (const auto& entry : std::filesystem::directory_iterator(collDir, ec)) {
				if (!entry.is_regular_file()) continue;
				if (entry.path().extension() == ".xml") {
					pugi::xml_document doc;
					if (doc.load_file(entry.path().wstring().c_str())) {
						pugi::xml_node root = doc.child("tileset");
						if (root && root.attribute("name")) {
							std::string name = root.attribute("name").as_string();
							if (!name.empty()) {
								uniqueNames.insert(std::move(name));
								continue;
							}
						}
					}
					uniqueNames.insert(entry.path().stem().string());
				}
			}
		}
	}

	// Scan palettes.xml under <palette name="paletteName">
	std::filesystem::path palettesPath = GetPalettesXmlPath();
	if (std::filesystem::exists(palettesPath, ec)) {
		pugi::xml_document palDoc;
		if (palDoc.load_file(palettesPath.wstring().c_str())) {
			pugi::xml_node root = palDoc.child("materials");
			if (!root) root = palDoc.child("palettes");
			if (!root) root = palDoc.first_child();

			for (pugi::xml_node palNode = root ? root.child("palette") : palDoc.child("palette"); palNode; palNode = palNode.next_sibling("palette")) {
				if (as_lower_str(palNode.attribute("name").as_string()) == as_lower_str(std::string(paletteName))) {
					for (pugi::xml_node tilesetNode = palNode.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
						for (pugi::xml_node incNode = tilesetNode.child("include"); incNode; incNode = incNode.next_sibling("include")) {
							std::string relPath = incNode.attribute("file").as_string();
							std::string folderPath = incNode.attribute("folder").as_string();
							if (!folderPath.empty()) {
								std::filesystem::path fullFolder = palettesPath.parent_path() / folderPath;
								if (std::filesystem::exists(fullFolder, ec) && std::filesystem::is_directory(fullFolder, ec)) {
									for (const auto& entry : std::filesystem::directory_iterator(fullFolder, ec)) {
										if (entry.is_regular_file() && entry.path().extension() == ".xml") {
											pugi::xml_document incDoc;
											if (incDoc.load_file(entry.path().wstring().c_str())) {
												pugi::xml_node tNode = incDoc.child("tileset");
												if (tNode && tNode.attribute("name")) {
													std::string name = tNode.attribute("name").as_string();
													if (!name.empty()) {
														uniqueNames.insert(std::move(name));
														continue;
													}
												}
											}
											uniqueNames.insert(entry.path().stem().string());
										}
									}
								}
							}
							if (!relPath.empty()) {
								std::filesystem::path fullPath = palettesPath.parent_path() / relPath;
								if (std::filesystem::exists(fullPath, ec)) {
									pugi::xml_document incDoc;
									if (incDoc.load_file(fullPath.wstring().c_str())) {
										pugi::xml_node tNode = incDoc.child("tileset");
										if (tNode && tNode.attribute("name")) {
											std::string name = tNode.attribute("name").as_string();
											if (!name.empty()) {
												uniqueNames.insert(std::move(name));
												continue;
											}
										}
									}
								}
								std::filesystem::path fPath(relPath);
								std::string stem = fPath.stem().string();
								if (!stem.empty()) {
									uniqueNames.insert(std::move(stem));
								}
							}
						}
					}
				}
			}
		}
	}

	// Also check in-memory PaletteCatalog
	if (const auto* pal = g_materials.paletteCatalog().findPalette(paletteName)) {
		for (const auto& ts : pal->tilesets) {
			if (!ts.name.empty()) {
				uniqueNames.insert(ts.name);
			}
		}
	}

	names.assign(uniqueNames.begin(), uniqueNames.end());
	std::ranges::sort(names);
	return names;
}

bool CollectionXmlService::SaveTilesetXml(
	const std::filesystem::path& filePath,
	const std::string& tilesetName,
	std::span<const uint16_t> itemIds,
	std::span<Brush* const> brushes,
	bool appendToExisting,
	std::string& outErrorMessage) {
	try {
		std::error_code ec;
		std::filesystem::create_directories(filePath.parent_path(), ec);
		if (ec) {
			outErrorMessage = std::format("Failed to create directory: {}", ec.message());
			return false;
		}

		std::set<std::string> brushSet;
		std::set<uint16_t> itemSet;
		pugi::xml_document doc;
		pugi::xml_node root;

		// If appendToExisting and file already exists, load existing contents directly into doc
		if (appendToExisting && std::filesystem::exists(filePath, ec)) {
			pugi::xml_parse_result res = doc.load_file(filePath.wstring().c_str(), pugi::parse_default | pugi::parse_comments | pugi::parse_declaration);
			if (res) {
				root = doc.child("tileset");
				if (root) {
					for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
						std::string childName = as_lower_str(child.name());
						if (childName == "brush") {
							std::string bname = child.attribute("name").as_string();
							if (!bname.empty()) {
								brushSet.insert(bname);
							}
						} else if (childName == "item") {
							if (child.attribute("id")) {
								uint16_t id = static_cast<uint16_t>(child.attribute("id").as_uint());
								if (id != 0) {
									itemSet.insert(id);
								}
							} else if (child.attribute("fromid")) {
								uint32_t fromId = child.attribute("fromid").as_uint();
								uint32_t toId = child.attribute("toid").as_uint(fromId);
								if (fromId > toId) {
									std::swap(fromId, toId);
								}
								for (uint32_t id = fromId; id <= toId && id <= 0xFFFF; ++id) {
									if (id != 0) {
										itemSet.insert(static_cast<uint16_t>(id));
									}
								}
							}
						}
					}
				}
			}
		}

		if (!root) {
			doc.reset();
			pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
			decl.append_attribute("version").set_value("1.0");
			decl.append_attribute("encoding").set_value("utf-8");

			root = doc.append_child("tileset");
			root.append_attribute("name").set_value(tilesetName.c_str());
		}

		// Append only missing brushes
		for (Brush* b : brushes) {
			if (b && !b->getName().empty() && !brushSet.contains(b->getName())) {
				brushSet.insert(b->getName());
				pugi::xml_node brushNode = root.append_child("brush");
				brushNode.append_attribute("name").set_value(b->getName().c_str());
			}
		}

		// Append only missing items
		for (uint16_t id : itemIds) {
			if (id != 0 && !itemSet.contains(id)) {
				itemSet.insert(id);
				pugi::xml_node itemNode = root.append_child("item");
				itemNode.append_attribute("id").set_value(id);
			}
		}

		if (!doc.save_file(filePath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8)) {
			outErrorMessage = std::format("Failed to save tileset XML to '{}'", filePath.string());
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		outErrorMessage = std::format("Exception writing tileset XML: {}", e.what());
		return false;
	}
}

bool CollectionXmlService::EnsurePaletteInclude(
	const std::string& paletteName,
	const std::string& tilesetName,
	std::string& outErrorMessage) {
	try {
		std::filesystem::path palettesPath = GetPalettesXmlPath();
		std::error_code ec;
		if (!std::filesystem::exists(palettesPath, ec)) {
			outErrorMessage = std::format("palettes.xml not found at '{}'", palettesPath.string());
			return false;
		}

		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(palettesPath.wstring().c_str(), pugi::parse_default | pugi::parse_comments | pugi::parse_declaration);
		if (!res) {
			outErrorMessage = std::format("Failed to parse palettes.xml: {}", res.description());
			return false;
		}

		std::filesystem::path tilesetFile = GetTilesetFilePath(paletteName, tilesetName);
		std::filesystem::path relPath = std::filesystem::relative(tilesetFile, palettesPath.parent_path(), ec);
		std::string includeRelPath = ec ? ("tilesets/collections/" + SanitizeTilesetFilename(tilesetName) + ".xml") : relPath.generic_string();

		pugi::xml_node root = doc.child("materials");
		if (!root) {
			root = doc.child("palettes");
		}
		if (!root) {
			root = doc.first_child();
		}

		pugi::xml_node targetPal;
		for (pugi::xml_node palNode = root ? root.child("palette") : doc.child("palette"); palNode; palNode = palNode.next_sibling("palette")) {
			if (as_lower_str(palNode.attribute("name").as_string()) == as_lower_str(paletteName)) {
				targetPal = palNode;
				break;
			}
		}

		if (!targetPal) {
			if (root) {
				targetPal = root.append_child("palette");
				targetPal.append_attribute("name").set_value(paletteName.c_str());
			} else {
				outErrorMessage = "Invalid palettes.xml structure: no root found";
				return false;
			}
		}

		pugi::xml_node tilesetNode = targetPal.child("tileset");
		if (!tilesetNode) {
			tilesetNode = targetPal.append_child("tileset");
		}

		// Check if explicit include file is already present
		bool alreadyIncluded = false;
		std::string normIncludeRel = as_lower_str(includeRelPath);
		std::replace(normIncludeRel.begin(), normIncludeRel.end(), '\\', '/');

		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFolder = inc.attribute("folder").as_string();
			if (!existingFolder.empty()) {
				std::string normFolder = as_lower_str(existingFolder);
				std::replace(normFolder.begin(), normFolder.end(), '\\', '/');
				if (!normFolder.ends_with('/')) {
					normFolder.push_back('/');
				}
				if (normIncludeRel.starts_with(normFolder)) {
					alreadyIncluded = true;
					break;
				}
			}

			std::string existingFile = inc.attribute("file").as_string();
			if (!existingFile.empty()) {
				std::string normExisting = as_lower_str(existingFile);
				std::replace(normExisting.begin(), normExisting.end(), '\\', '/');
				if (normExisting == normIncludeRel) {
					alreadyIncluded = true;
					break;
				}
			}
		}

		if (!alreadyIncluded) {
			pugi::xml_node newInc = tilesetNode.append_child("include");
			newInc.append_attribute("file").set_value(includeRelPath.c_str());
		}

		if (!doc.save_file(palettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8)) {
			outErrorMessage = std::format("Failed to save updated palettes.xml to '{}'", palettesPath.string());
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		outErrorMessage = std::format("Exception updating palettes.xml: {}", e.what());
		return false;
	}
}

} // namespace rme::collection
