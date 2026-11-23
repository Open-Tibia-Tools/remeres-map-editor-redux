//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/detected/detected_palettes.h"

#include "app/main.h"
#include "app/settings.h"
#include "brushes/raw/raw_brush.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"
#include "palette/detected/detected_item_rules.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>
#include <vector>

namespace {

	DetectedItemFacts CollectFacts(const ItemDefinitionView& item) {
		const ItemEditorData& editor = item.editorData();
		return DetectedItemFacts {
			.server_id = item.serverId(),
			.client_id = item.clientId(),
			.meta = item.isMetaItem(),
			.ground = item.isGroundTile(),
			.ground_border_piece = item.hasFlag(ItemFlag::AlwaysOnBottom) && item.attribute(ItemAttributeKey::AlwaysOnTopOrder) == 1,
			.registered_border = item.hasFlag(ItemFlag::IsBorder),
			.brush_owned = editor.brush != nullptr || editor.doodad_brush != nullptr || editor.collection_brush != nullptr,
		};
	}

	struct DetectedTileset {
		std::string name;
		std::vector<Brush*> brushes;
	};

	void AddPalette(std::string name, std::vector<DetectedTileset> tilesets) {
		DynamicPaletteDefinition palette;
		palette.name = std::move(name);
		for (auto& tileset : tilesets) {
			if (tileset.brushes.empty()) {
				continue;
			}
			spdlog::info("Detected palette '{}' / '{}': {} items", palette.name, tileset.name, tileset.brushes.size());
			palette.tilesets.push_back(DynamicTilesetDefinition { .name = std::move(tileset.name), .brushes = std::move(tileset.brushes) });
		}
		if (!palette.tilesets.empty()) {
			g_materials.paletteCatalog().addDynamicPalette(std::move(palette));
		}
	}

} // namespace

void AddDetectedPalettes() {
	if (!g_settings.getBoolean(Config::SHOW_DETECTED_PALETTES)) {
		return;
	}

	DetectedTileset unregistered_borders { .name = "Unregistered borders" };

	for (const ServerItemId id : g_item_definitions.allIds()) {
		if (id < 100) {
			continue;
		}
		const ItemDefinitionView item = g_item_definitions.get(id);
		if (!item) {
			continue;
		}
		const DetectedItemFacts facts = CollectFacts(item);
		if (IsUnregisteredBorder(facts)) {
			if (RAWBrush* brush = g_materials.ensureRawBrush(id)) {
				unregistered_borders.brushes.push_back(brush);
			}
		}
	}

	AddPalette("Detected Borders", { std::move(unregistered_borders) });
}
