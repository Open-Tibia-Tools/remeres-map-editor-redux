#ifndef RME_GAME_MATERIAL_MANIFEST_H_
#define RME_GAME_MATERIAL_MANIFEST_H_

#include "app/definitions.h"

#include <filesystem>
#include <string>
#include <vector>

struct MaterialManifestFiles {
	std::vector<std::filesystem::path> borders;
	std::vector<std::filesystem::path> brushes;
	std::vector<std::filesystem::path> creatures;
	std::vector<std::filesystem::path> items;
	std::vector<std::filesystem::path> tilesets;
	std::vector<std::filesystem::path> palettes;
};

[[nodiscard]] bool LoadMaterialManifestFiles(const std::filesystem::path& manifestFile, MaterialManifestFiles& files, std::string& error);

#endif
