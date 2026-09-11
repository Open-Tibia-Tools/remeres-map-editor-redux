#include "game/material_database.h"

#include "brushes/brush.h"

#include <algorithm>

namespace {
	constexpr std::string_view CREATURE_PALETTE_NAME = "Creature";
	constexpr std::string_view MONSTER_IMPORT_TILESET_NAME = "Others";
	constexpr std::string_view NPC_IMPORT_TILESET_NAME = "NPCs";
}

bool DynamicTilesetDefinition::containsBrush(const Brush* brush) const {
	return brush && std::ranges::find(brushes, brush) != brushes.end();
}

bool DynamicPaletteDefinition::containsBrush(const Brush* brush) const {
	return brush && std::ranges::any_of(tilesets, [brush](const auto& tileset) {
		return tileset.containsBrush(brush);
	});
}

void PaletteCatalog::clear() {
	palettes.clear();
}

void PaletteCatalog::addDynamicPalette(DynamicPaletteDefinition palette) {
	palettes.push_back(std::move(palette));
}

void PaletteCatalog::prepareCreatureImportTargets() {
	auto paletteIt = std::ranges::find_if(palettes, [](const auto& palette) {
		return palette.name == CREATURE_PALETTE_NAME;
	});
	if (paletteIt == palettes.end()) {
		return;
	}

	auto prepareTarget = [&paletteIt](std::string_view name, CreatureImportTarget target) {
		auto tilesetIt = std::ranges::find_if(paletteIt->tilesets, [name](const auto& tileset) {
			return tileset.name == name;
		});
		if (tilesetIt == paletteIt->tilesets.end()) {
			DynamicTilesetDefinition tileset;
			tileset.name = name;
			paletteIt->tilesets.push_back(std::move(tileset));
			tilesetIt = std::prev(paletteIt->tilesets.end());
		}
		tilesetIt->creatureImportTarget = target;
	};

	prepareTarget(MONSTER_IMPORT_TILESET_NAME, CreatureImportTarget::Monster);
	prepareTarget(NPC_IMPORT_TILESET_NAME, CreatureImportTarget::Npc);
}

PaletteBrushRegistrationResult PaletteCatalog::registerImportedCreatureBrush(Brush* brush, bool isNpc) {
	if (!brush) {
		return PaletteBrushRegistrationResult::TargetUnavailable;
	}
	if (findPaletteContainingBrush(brush)) {
		return PaletteBrushRegistrationResult::AlreadyRegistered;
	}

	const CreatureImportTarget target = isNpc ? CreatureImportTarget::Npc : CreatureImportTarget::Monster;
	for (auto& palette : palettes) {
		for (auto& tileset : palette.tilesets) {
			if (tileset.creatureImportTarget == target) {
				tileset.brushes.push_back(brush);
				return PaletteBrushRegistrationResult::Added;
			}
		}
	}
	return PaletteBrushRegistrationResult::TargetUnavailable;
}

const DynamicPaletteDefinition* PaletteCatalog::findPalette(std::string_view name) const {
	auto it = std::ranges::find_if(palettes, [name](const auto& palette) {
		return palette.name == name;
	});
	return it == palettes.end() ? nullptr : &*it;
}

const DynamicPaletteDefinition* PaletteCatalog::findPaletteContainingBrush(const Brush* brush, std::string_view preferredPalette) const {
	if (!brush) {
		return nullptr;
	}
	if (!preferredPalette.empty()) {
		if (const auto* palette = findPalette(preferredPalette); palette && palette->containsBrush(brush)) {
			return palette;
		}
	}
	auto it = std::ranges::find_if(palettes, [brush](const auto& palette) {
		return palette.containsBrush(brush);
	});
	return it == palettes.end() ? nullptr : &*it;
}

void MaterialDatabase::clear() {
	palettes.clear();
	brush_registry = nullptr;
	item_registry = nullptr;
	creature_registry = nullptr;
	tileset_sources.clear();
}

void MaterialDatabase::bindSourceTruth(Brushes& brushRegistry, ItemDefinitionStore& itemRegistry, CreatureDatabase& creatureRegistry) {
	brush_registry = &brushRegistry;
	item_registry = &itemRegistry;
	creature_registry = &creatureRegistry;
}

void MaterialDatabase::setTilesetSources(std::vector<std::string> sources) {
	tileset_sources = std::move(sources);
	std::ranges::sort(tileset_sources);
	tileset_sources.erase(std::ranges::unique(tileset_sources).begin(), tileset_sources.end());
}

Brushes& MaterialDatabase::brushes() const {
	ASSERT(brush_registry != nullptr);
	return *brush_registry;
}

ItemDefinitionStore& MaterialDatabase::items() const {
	ASSERT(item_registry != nullptr);
	return *item_registry;
}

CreatureDatabase& MaterialDatabase::creatures() const {
	ASSERT(creature_registry != nullptr);
	return *creature_registry;
}

bool MaterialDatabase::hasSourceTruth() const {
	return brush_registry && item_registry && creature_registry;
}

bool MaterialDatabase::isKnownTilesetSource(std::string_view source) const {
	return std::ranges::binary_search(tileset_sources, source);
}
