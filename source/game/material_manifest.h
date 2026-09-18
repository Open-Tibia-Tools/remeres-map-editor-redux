#ifndef RME_GAME_MATERIAL_MANIFEST_H_
#define RME_GAME_MATERIAL_MANIFEST_H_

#include "app/main.h"

#include <vector>

struct MaterialManifestFiles {
	std::vector<FileName> borders;
	std::vector<FileName> brushes;
	std::vector<FileName> creatures;
	std::vector<FileName> items;
	std::vector<FileName> tilesets;
	std::vector<FileName> palettes;
};

[[nodiscard]] bool LoadMaterialManifestFiles(const FileName& manifestFile, MaterialManifestFiles& files, std::string& error);

#endif
