#ifndef RME_GAME_MATERIAL_DATABASE_H_
#define RME_GAME_MATERIAL_DATABASE_H_

#include <string>
#include <vector>

class Brush;
class Brushes;
class CreatureDatabase;
class ItemDefinitionStore;

enum class CreatureImportTarget {
	None,
	Monster,
	Npc,
};

enum class PaletteBrushRegistrationResult {
	Added,
	AlreadyRegistered,
	TargetUnavailable,
};

struct DynamicTilesetDefinition {
	std::string name;
	std::vector<Brush*> brushes;
	CreatureImportTarget creatureImportTarget = CreatureImportTarget::None;

	[[nodiscard]] size_t size() const {
		return brushes.size();
	}

	[[nodiscard]] bool containsBrush(const Brush* brush) const;
};

struct DynamicPaletteDefinition {
	std::string name;
	// Palette UI panels keep pointers to elements in this vector. Tileset objects stay fixed after publication;
	// their brush membership may grow through PaletteCatalog runtime registration.
	std::vector<DynamicTilesetDefinition> tilesets;

	[[nodiscard]] bool containsBrush(const Brush* brush) const;
};

class PaletteCatalog {
public:
	void clear();
	void addDynamicPalette(DynamicPaletteDefinition palette);
	void prepareCreatureImportTargets();
	[[nodiscard]] PaletteBrushRegistrationResult registerImportedCreatureBrush(Brush* brush, bool isNpc);

	[[nodiscard]] const std::vector<DynamicPaletteDefinition>& dynamicPalettes() const {
		return palettes;
	}

	[[nodiscard]] const DynamicPaletteDefinition* findPalette(std::string_view name) const;
	[[nodiscard]] const DynamicPaletteDefinition* findPaletteContainingBrush(const Brush* brush, std::string_view preferredPalette = {}) const;

private:
	std::vector<DynamicPaletteDefinition> palettes;
};

class MaterialDatabase {
public:
	void clear();
	void bindSourceTruth(Brushes& brushRegistry, ItemDefinitionStore& itemRegistry, CreatureDatabase& creatureRegistry);
	void setTilesetSources(std::vector<std::string> sources);

	[[nodiscard]] Brushes& brushes() const;
	[[nodiscard]] ItemDefinitionStore& items() const;
	[[nodiscard]] CreatureDatabase& creatures() const;
	[[nodiscard]] bool hasSourceTruth() const;
	[[nodiscard]] bool isKnownTilesetSource(std::string_view source) const;

	[[nodiscard]] PaletteCatalog& paletteCatalog() {
		return palettes;
	}

	[[nodiscard]] const PaletteCatalog& paletteCatalog() const {
		return palettes;
	}

private:
	PaletteCatalog palettes;
	Brushes* brush_registry = nullptr;
	ItemDefinitionStore* item_registry = nullptr;
	CreatureDatabase* creature_registry = nullptr;
	std::vector<std::string> tileset_sources;
};

#endif
