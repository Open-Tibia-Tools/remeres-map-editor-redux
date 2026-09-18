//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "editor/editor.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/creatures.h"
#include "game/material_include_resolver.h"
#include "game/materials.h"
#include "brushes/brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/raw/raw_brush.h"

#include <format>
#include <limits>

Materials g_materials;

namespace {
	[[nodiscard]] std::string normalizedPathKey(const std::filesystem::path& file) {
		return file.string();
	}
}

Materials::Materials() {
	////
}

Materials::~Materials() {
	clear();
}

void Materials::clear() {
	for (auto* extension : extensions) {
		delete extension;
	}

	extensions.clear();
	database.clear();
}

const MaterialsExtensionList& Materials::getExtensions() {
	return extensions;
}

MaterialsExtensionList Materials::getExtensionsByVersion(const ClientVersionID& version_id) {
	MaterialsExtensionList ret_list;
	for (MaterialsExtensionList::iterator iter = extensions.begin(); iter != extensions.end(); ++iter) {
		if ((*iter)->isForVersion(version_id)) {
			ret_list.push_back(*iter);
		}
	}
	return ret_list;
}

bool Materials::loadMaterials(const std::filesystem::path& identifier, std::string& error, std::vector<std::string>& warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(identifier.c_str());
	if (!result) {
		warnings.push_back(std::format("Could not open {} (file not found or syntax error)", identifier.filename().string()));
		return false;
	}

	pugi::xml_node node = doc.child("materials");
	if (!node) {
		warnings.push_back(std::format("{}: Invalid rootheader.", identifier.filename().string()));
		return false;
	}

	database.clear();
	database.bindSourceTruth(g_brushes, g_item_definitions, g_creatures);
	if (!unserializeMaterials(identifier, node, error, warnings)) {
		return false;
	}
	database.paletteCatalog().prepareCreatureImportTargets();
	return true;
}

bool Materials::unserializeMaterials(const std::filesystem::path& filename, pugi::xml_node node, std::string& error, std::vector<std::string>& warnings) {
	ManifestSectionsSeen seen;
	std::vector<std::filesystem::path> paletteFiles;

	for (pugi::xml_node childNode = node.first_child(); childNode; childNode = childNode.next_sibling()) {
		const std::string childName = as_lower_str(childNode.name());
		if (childName == "metaitem") {
			if (const auto attribute = childNode.attribute("id")) {
				g_item_definitions.ensureMetaItem(attribute.as_ushort());
			}
		} else if (childName == "tileset") {
			error = "Legacy <tileset> nodes are not supported by the modular material loader.";
			return false;
		} else if (!loadMaterialsSection(filename, childNode, childName, seen, paletteFiles, error, warnings)) {
			return false;
		}
	}

	if (!seen.complete()) {
		error = "Modular materials.xml must define borders, brushes, creatures, items, tilesets, and palettes sections.";
		return false;
	}
	return resolvePaletteReferences(paletteFiles, error, warnings);
}

bool Materials::loadMaterialsSection(const std::filesystem::path& filename, pugi::xml_node section, std::string_view sectionName, ManifestSectionsSeen& seen, std::vector<std::filesystem::path>& paletteFiles, std::string& error, std::vector<std::string>& warnings) {
	if (sectionName == "borders") {
		seen.borders = true;
		return loadModuleIncludes(filename, section, "border", error, warnings);
	}
	if (sectionName == "brushes") {
		seen.brushes = true;
		return loadModuleIncludes(filename, section, "brush", error, warnings);
	}
	if (sectionName == "creatures") {
		seen.creatures = true;
		return loadModuleIncludes(filename, section, {}, error, warnings);
	}
	if (sectionName == "items") {
		seen.items = true;
		return loadModuleIncludes(filename, section, {}, error, warnings);
	}
	if (sectionName == "tilesets") {
		seen.tilesets = true;
		return loadTilesetSources(filename, section, error);
	}
	if (sectionName == "palettes") {
		seen.palettes = true;
		return loadPaletteIncludes(filename, section, paletteFiles, error);
	}
	return true;
}

bool Materials::loadTilesetSources(const std::filesystem::path& filename, pugi::xml_node section, std::string& error) {
	std::vector<std::filesystem::path> tilesetFiles;
	if (!MaterialIncludeResolver::collectSectionFiles(filename, section, "tilesets", tilesetFiles, error)) {
		return false;
	}
	std::vector<std::string> sourceKeys;
	sourceKeys.reserve(tilesetFiles.size());
	for (const auto& tilesetFile : tilesetFiles) {
		sourceKeys.push_back(normalizedPathKey(tilesetFile));
	}
	database.setTilesetSources(std::move(sourceKeys));
	return true;
}

bool Materials::loadModuleIncludes(const std::filesystem::path& manifest, pugi::xml_node section, std::string_view expectedNode, std::string& error, std::vector<std::string>& warnings) {
	bool loadedAny = false;
	for (pugi::xml_node includeNode = section.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
		std::vector<std::filesystem::path> files;
		if (const auto fileAttribute = includeNode.attribute("file")) {
			std::filesystem::path includeFile = MaterialIncludeResolver::resolveRelativePath(manifest, fileAttribute.as_string());
			if (!std::filesystem::is_regular_file(includeFile)) {
				error = std::format("Missing modular include file: {}", includeFile.string());
				return false;
			}
			files.push_back(includeFile);
		} else if (const auto folderAttribute = includeNode.attribute("folder")) {
			std::filesystem::path folder = MaterialIncludeResolver::resolveRelativeFolder(manifest, folderAttribute.as_string());
			if (!std::filesystem::is_directory(folder)) {
				error = std::format("Missing modular include folder: {}", folder.string());
				return false;
			}
			files = MaterialIncludeResolver::collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false));
		}

		for (const auto& file : files) {
			loadedAny = true;
			if (expectedNode.empty()) {
				continue;
			}

			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_file(file.c_str());
			if (!result) {
				error = std::format("Could not parse modular include file: {}", file.string());
				return false;
			}

			pugi::xml_node root = doc.document_element();
			for (pugi::xml_node childNode = root.first_child(); childNode; childNode = childNode.next_sibling()) {
				const std::string childName = as_lower_str(childNode.name());
				if (childName != expectedNode) {
					continue;
				}
				if (expectedNode == "border") {
					g_brushes.unserializeBorder(childNode, warnings);
				} else if (expectedNode == "brush") {
					g_brushes.unserializeBrush(childNode, warnings);
				}
			}
		}
	}

	if (!loadedAny) {
		error = std::format("Modular section has no include entries: {}", section.name());
		return false;
	}
	return true;
}

bool Materials::loadPaletteIncludes(const std::filesystem::path& manifest, pugi::xml_node section, std::vector<std::filesystem::path>& paletteFiles, std::string& error) {
	bool loadedAny = false;
	for (pugi::xml_node includeNode = section.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
		if (const auto fileAttribute = includeNode.attribute("file")) {
			std::filesystem::path includeFile = MaterialIncludeResolver::resolveRelativePath(manifest, fileAttribute.as_string());
			if (!std::filesystem::is_regular_file(includeFile)) {
				error = std::format("Missing palettes include file: {}", includeFile.string());
				return false;
			}
			loadedAny = true;
			paletteFiles.push_back(includeFile);
		} else if (const auto folderAttribute = includeNode.attribute("folder")) {
			std::filesystem::path folder = MaterialIncludeResolver::resolveRelativeFolder(manifest, folderAttribute.as_string());
			if (!std::filesystem::is_directory(folder)) {
				error = std::format("Missing palettes include folder: {}", folder.string());
				return false;
			}
			for (const auto& file : MaterialIncludeResolver::collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false))) {
				loadedAny = true;
				paletteFiles.push_back(file);
			}
		}
	}

	if (!loadedAny) {
		error = "Modular palettes section has no include entries.";
		return false;
	}
	return true;
}

bool Materials::resolvePaletteReferences(const std::vector<std::filesystem::path>& paletteFiles, std::string& error, std::vector<std::string>& warnings) {
	for (const auto& paletteFile : paletteFiles) {
		if (!loadPaletteFile(paletteFile, error, warnings)) {
			return false;
		}
	}
	return true;
}

static RAWBrush* ensureRawBrush(ServerItemId item_id) {
	ItemEditorData& editor_data = g_item_definitions.mutableEditorData(item_id);
	if (editor_data.raw_brush == nullptr) {
		auto raw_brush = std::make_unique<RAWBrush>(item_id);
		editor_data.raw_brush = raw_brush.get();
		g_item_definitions.setFlag(item_id, ItemFlag::HasRaw, true);
		g_brushes.addBrush(std::move(raw_brush));
	}
	return editor_data.raw_brush;
}

static CreatureBrush* ensureCreatureBrush(CreatureType* type) {
	if (!type) {
		return nullptr;
	}
	if (type->brush) {
		return type->brush;
	}
	auto creature_brush = std::make_unique<CreatureBrush>(type);
	type->brush = creature_brush.get();
	CreatureBrush* brush = creature_brush.get();
	g_brushes.addBrush(std::move(creature_brush));
	return brush;
}

PaletteBrushRegistrationResult Materials::registerImportedCreature(CreatureType& creature) {
	CreatureBrush* brush = ensureCreatureBrush(&creature);
	brush->flagAsVisible();
	return database.paletteCatalog().registerImportedCreatureBrush(brush, creature.isNpc);
}

static void loadTilesetBrushEntry(pugi::xml_node node, DynamicTilesetDefinition& tileset, std::vector<std::string>& warnings) {
	const auto brushName = node.attribute("name");
	if (!brushName) {
		return;
	}
	if (Brush* brush = g_brushes.getBrush(brushName.as_string())) {
		brush->flagAsVisible();
		if (!tileset.containsBrush(brush)) {
			tileset.brushes.push_back(brush);
		}
	} else {
		warnings.push_back(std::format("tileset_brush_references: tileset=\"{}\" brush=\"{}\"", tileset.name, brushName.as_string()));
	}
}

static bool readTilesetItemRange(pugi::xml_node node, uint32_t& fromId, uint32_t& toId) {
	if (const auto idAttribute = node.attribute("id")) {
		fromId = idAttribute.as_uint();
		toId = fromId;
		return true;
	}
	if (const auto fromAttribute = node.attribute("fromid")) {
		fromId = fromAttribute.as_uint();
		toId = node.attribute("toid").as_uint(fromId);
		return true;
	}
	return false;
}

static void loadTilesetItemEntry(pugi::xml_node node, DynamicTilesetDefinition& tileset, std::vector<std::string>& warnings) {
	uint32_t fromId = 0;
	uint32_t toId = 0;
	if (!readTilesetItemRange(node, fromId, toId)) {
		return;
	}

	constexpr uint32_t maxItemId = std::numeric_limits<uint16_t>::max();
	if (fromId > toId || fromId > maxItemId || toId > maxItemId) {
		warnings.push_back(std::format("tileset_item_references: tileset=\"{}\" invalid_range={}..{}", tileset.name, fromId, toId));
		return;
	}

	for (uint32_t id = fromId; id <= toId; ++id) {
		const auto definition = g_item_definitions.get(static_cast<ServerItemId>(id));
		if (!definition) {
			warnings.push_back(std::format("tileset_item_references: tileset=\"{}\" missing_id={}", tileset.name, id));
			continue;
		}
		RAWBrush* brush = ensureRawBrush(static_cast<ServerItemId>(id));
		brush->flagAsVisible();
		if (!tileset.containsBrush(brush)) {
			tileset.brushes.push_back(brush);
		}
	}
}

static void loadTilesetCreatureEntry(pugi::xml_node node, DynamicTilesetDefinition& tileset, std::vector<std::string>& warnings) {
	const auto creatureName = node.attribute("name");
	if (!creatureName) {
		return;
	}
	CreatureType* type = g_creatures[creatureName.as_string()];
	if (!type) {
		warnings.push_back(std::format("tileset_creature_references: tileset=\"{}\" creature=\"{}\"", tileset.name, creatureName.as_string()));
		return;
	}
	CreatureBrush* brush = ensureCreatureBrush(type);
	brush->flagAsVisible();
	if (!tileset.containsBrush(brush)) {
		tileset.brushes.push_back(brush);
	}
}

bool Materials::loadPaletteFile(const std::filesystem::path& filename, std::string& error, std::vector<std::string>& warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.c_str());
	if (!result) {
		error = std::format("Could not parse palettes file: {}", filename.string());
		return false;
	}

	pugi::xml_node root = doc.child("palettes");
	if (!root) {
		error = std::format("Invalid palettes file root: {}", filename.string());
		return false;
	}

	for (pugi::xml_node paletteNode = root.child("palette"); paletteNode; paletteNode = paletteNode.next_sibling("palette")) {
		const auto nameAttribute = paletteNode.attribute("name");
		if (!nameAttribute || std::string(nameAttribute.as_string()).empty()) {
			warnings.push_back("palettes: skipped palette without name in " + filename.string());
			continue;
		}

		DynamicPaletteDefinition palette;
		palette.name = nameAttribute.as_string();

		for (pugi::xml_node tilesetNode = paletteNode.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
			for (pugi::xml_node includeNode = tilesetNode.child("include"); includeNode; includeNode = includeNode.next_sibling("include")) {
				if (!loadTilesetInclude(filename, includeNode, palette, error, warnings)) {
					return false;
				}
			}
		}

		database.paletteCatalog().addDynamicPalette(std::move(palette));
	}
	return true;
}

bool Materials::loadTilesetInclude(const std::filesystem::path& manifest, pugi::xml_node includeNode, DynamicPaletteDefinition& palette, std::string& error, std::vector<std::string>& warnings) {
	if (const auto fileAttribute = includeNode.attribute("file")) {
		std::filesystem::path includeFile = MaterialIncludeResolver::resolveRelativePath(manifest, fileAttribute.as_string());
		if (!std::filesystem::is_regular_file(includeFile)) {
			error = std::format("Missing tileset include file: {}", includeFile.string());
			return false;
		}
		if (!database.isKnownTilesetSource(normalizedPathKey(includeFile))) {
			error = std::format("Tileset include is not declared in materials.xml <tilesets>: {}", includeFile.string());
			return false;
		}
		return loadDynamicTilesetFile(includeFile, palette, error, warnings);
	}

	if (const auto folderAttribute = includeNode.attribute("folder")) {
		std::filesystem::path folder = MaterialIncludeResolver::resolveRelativeFolder(manifest, folderAttribute.as_string());
		if (!std::filesystem::is_directory(folder)) {
			error = std::format("Missing tileset include folder: {}", folder.string());
			return false;
		}
		for (const auto& file : MaterialIncludeResolver::collectXmlFiles(folder, includeNode.attribute("subfolders").as_bool(false))) {
			if (!database.isKnownTilesetSource(normalizedPathKey(file))) {
				error = std::format("Tileset include is not declared in materials.xml <tilesets>: {}", file.string());
				return false;
			}
			if (!loadDynamicTilesetFile(file, palette, error, warnings)) {
				return false;
			}
		}
		return true;
	}

	const std::string message = std::format("tileset_include: manifest=\"{}\" include node is missing both file and folder attributes", manifest.string());
	warnings.push_back(message);
	error = message;
	return false;
}

bool Materials::loadDynamicTilesetFile(const std::filesystem::path& filename, DynamicPaletteDefinition& palette, std::string& error, std::vector<std::string>& warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.c_str());
	if (!result) {
		error = std::format("Could not parse tileset file: {}", filename.string());
		return false;
	}

	pugi::xml_node root = doc.child("tileset");
	if (!root) {
		error = std::format("Invalid tileset file root: {}", filename.string());
		return false;
	}

	const auto nameAttribute = root.attribute("name");
	if (!nameAttribute || std::string(nameAttribute.as_string()).empty()) {
		error = std::format("Tileset file is missing name attribute: {}", filename.string());
		return false;
	}

	const std::string tilesetName = nameAttribute.as_string();
	auto it = std::ranges::find_if(palette.tilesets, [&](const DynamicTilesetDefinition& existing) {
		return existing.name == tilesetName;
	});
	DynamicTilesetDefinition* targetTileset = nullptr;
	if (it != palette.tilesets.end()) {
		targetTileset = &(*it);
	} else {
		palette.tilesets.emplace_back();
		targetTileset = &palette.tilesets.back();
		targetTileset->name = tilesetName;
	}

	for (pugi::xml_node childNode = root.first_child(); childNode; childNode = childNode.next_sibling()) {
		const std::string childName = as_lower_str(childNode.name());
		if (childName == "brush") {
			loadTilesetBrushEntry(childNode, *targetTileset, warnings);
		} else if (childName == "item") {
			loadTilesetItemEntry(childNode, *targetTileset, warnings);
		} else if (childName == "creature") {
			loadTilesetCreatureEntry(childNode, *targetTileset, warnings);
		}
	}

	return true;
}
