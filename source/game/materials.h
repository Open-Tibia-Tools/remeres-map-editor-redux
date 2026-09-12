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

#ifndef RME_MATERIALS_H_
#define RME_MATERIALS_H_

#include "app/extension.h"
#include "game/material_database.h"

class CreatureType;

class Materials {
public:
	Materials();
	~Materials();

	void clear();

	const MaterialsExtensionList& getExtensions();
	MaterialsExtensionList getExtensionsByVersion(const ClientVersionID& version_id);

	[[nodiscard]] PaletteCatalog& paletteCatalog() {
		return database.paletteCatalog();
	}

	[[nodiscard]] const PaletteCatalog& paletteCatalog() const {
		return database.paletteCatalog();
	}

	[[nodiscard]] PaletteBrushRegistrationResult registerImportedCreature(CreatureType& creature);

	bool loadMaterials(const FileName& identifier, wxString& error, std::vector<std::string>& warnings);
	bool needSave() const {
		return modified;
	}

	void modify(bool newValue = true) {
		this->modified = newValue;
	}

protected:
	struct ManifestSectionsSeen {
		bool borders = false;
		bool brushes = false;
		bool creatures = false;
		bool items = false;
		bool tilesets = false;
		bool palettes = false;

		[[nodiscard]] bool complete() const {
			return borders && brushes && creatures && items && tilesets && palettes;
		}
	};

	bool unserializeMaterials(const FileName& filename, pugi::xml_node node, wxString& error, std::vector<std::string>& warnings);
	bool loadMaterialsSection(const FileName& filename, pugi::xml_node section, std::string_view sectionName, ManifestSectionsSeen& seen, std::vector<FileName>& paletteFiles, wxString& error, std::vector<std::string>& warnings);
	bool loadTilesetSources(const FileName& filename, pugi::xml_node section, wxString& error);
	bool loadModuleIncludes(const FileName& manifest, pugi::xml_node section, std::string_view expectedNode, wxString& error, std::vector<std::string>& warnings);
	bool loadPaletteIncludes(const FileName& manifest, pugi::xml_node section, std::vector<FileName>& paletteFiles, wxString& error);
	bool resolvePaletteReferences(const std::vector<FileName>& paletteFiles, wxString& error, std::vector<std::string>& warnings);
	bool loadPaletteFile(const FileName& filename, wxString& error, std::vector<std::string>& warnings);
	bool loadTilesetInclude(const FileName& manifest, pugi::xml_node includeNode, DynamicPaletteDefinition& palette, wxString& error, std::vector<std::string>& warnings);
	bool loadDynamicTilesetFile(const FileName& filename, DynamicPaletteDefinition& palette, wxString& error, std::vector<std::string>& warnings);

	MaterialsExtensionList extensions;

private:
	MaterialDatabase database;
	bool modified = false;
	Materials(const Materials&) = delete;
	Materials& operator=(const Materials&) = delete;
};

extern Materials g_materials;

#endif
