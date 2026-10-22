#ifndef RME_IO_COLLECTION_XML_SERVICE_H_
#define RME_IO_COLLECTION_XML_SERVICE_H_

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <cstdint>

class Brush;

namespace rme::collection {

class CollectionXmlService {
public:
	/**
	 * Sanitizes a tileset name for use as a filesystem filename.
	 * Replaces < > : " / \ | ? * and control chars with '_'.
	 * Trims leading and trailing spaces and dots.
	 */
	static inline std::string SanitizeTilesetFilename(std::string_view name) {
		std::string result;
		result.reserve(name.size());
		for (char c : name) {
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
				result.push_back('_');
			} else if (static_cast<unsigned char>(c) < 32) {
				result.push_back('_');
			} else {
				result.push_back(c);
			}
		}

		size_t start = result.find_first_not_of(" .");
		if (start == std::string::npos) {
			return "collection";
		}
		size_t end = result.find_last_not_of(" .");
		result = result.substr(start, end - start + 1);
		return result.empty() ? "collection" : result;
	}

	/**
	 * Returns the collections directory under the active client version's data folder:
	 * <data_dir>/<version>/tilesets/collections/
	 */
	static std::filesystem::path GetCollectionsDirectory();

	/**
	 * Returns full path to palettes.xml for the active client version.
	 */
	static std::filesystem::path GetPalettesXmlPath();

	/**
	 * Returns full path to a tileset xml file under the specified palette:
	 * If palette is "Collection": <data_dir>/<version>/tilesets/collections/<sanitizedName>.xml
	 * For other palettes: resolves matching include path in palettes.xml or generates default under tilesets/<palette>/
	 */
	static std::filesystem::path GetTilesetFilePath(std::string_view paletteName, std::string_view tilesetName);
	static std::filesystem::path GetTilesetFilePath(std::string_view tilesetName) {
		return GetTilesetFilePath("Collection", tilesetName);
	}

	/**
	 * Returns names of all available XML palettes (e.g. "Collection", "Terrain", "Doodad", "Item", "Creature", "Raw").
	 * "Collection" is guaranteed to be the first element.
	 */
	static std::vector<std::string> GetAvailablePalettes();

	/**
	 * Returns names of all existing tilesets declared under the specified XML palette.
	 */
	static std::vector<std::string> GetExistingTilesetNamesForPalette(std::string_view paletteName);

	/**
	 * Returns names of all existing collection tilesets found in the collections folder
	 * and/or declared in palettes.xml under <palette name="Collection">.
	 */
	static std::vector<std::string> GetExistingCollectionTilesetNames() {
		return GetExistingTilesetNamesForPalette("Collection");
	}

	/**
	 * Saves a tileset to disk at the specified file path.
	 * <tileset name="<tilesetName>">
	 *     <brush name="..."/>
	 *     <item id="..."/>
	 * </tileset>
	 * If appendToExisting is true, merges with any existing entries in the file.
	 */
	static bool SaveTilesetXml(
		const std::filesystem::path& filePath,
		const std::string& tilesetName,
		std::span<const uint16_t> itemIds,
		std::span<Brush* const> brushes,
		bool appendToExisting,
		std::string& outErrorMessage);

	static bool SaveTilesetXml(
		const std::string& tilesetName,
		std::span<const uint16_t> itemIds,
		std::span<Brush* const> brushes,
		bool appendToExisting,
		std::string& outErrorMessage) {
		return SaveTilesetXml(GetTilesetFilePath(tilesetName), tilesetName, itemIds, brushes, appendToExisting, outErrorMessage);
	}

	/**
	 * Ensures <palette name="<paletteName>"> in palettes.xml includes the tileset file.
	 */
	static bool EnsurePaletteInclude(
		const std::string& paletteName,
		const std::string& tilesetName,
		std::string& outErrorMessage);

	static bool EnsurePaletteInclude(
		const std::string& tilesetName,
		std::string& outErrorMessage) {
		return EnsurePaletteInclude("Collection", tilesetName, outErrorMessage);
	}
};

} // namespace rme::collection

#endif // RME_IO_COLLECTION_XML_SERVICE_H_
