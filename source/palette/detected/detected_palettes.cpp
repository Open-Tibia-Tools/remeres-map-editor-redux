//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/detected/detected_palettes.h"

#include "app/main.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/raw/raw_brush.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"
#include "palette/detected/detected_item_rules.h"

#include <spdlog/spdlog.h>

#include <format>

#include <string>
#include <utility>
#include <vector>

namespace {

	bool HasAnyBorder(const GroundBrush& brush) {
		return brush.hasOuterBorder() || brush.hasInnerBorder() || brush.hasOptionalBorder() || !brush.getBorders().empty();
	}

	DetectedItemFacts CollectFacts(const ItemDefinitionView& item) {
		const ItemEditorData& editor = item.editorData();
		const GroundBrush* ground_brush = editor.brush ? editor.brush->as<GroundBrush>() : nullptr;
		return DetectedItemFacts {
			.server_id = item.serverId(),
			.client_id = item.clientId(),
			.meta = item.isMetaItem(),
			.ground = item.isGroundTile(),
			.ground_border_piece = item.hasFlag(ItemFlag::AlwaysOnBottom) && item.attribute(ItemAttributeKey::AlwaysOnTopOrder) == 1,
			.registered_border = item.hasFlag(ItemFlag::IsBorder),
			.brush_owned = editor.brush != nullptr || editor.doodad_brush != nullptr || editor.collection_brush != nullptr,
			.unpassable = item.hasFlag(ItemFlag::Unpassable),
			.ground_brush_owned = ground_brush != nullptr,
			.ground_brush_has_border = ground_brush != nullptr && HasAnyBorder(*ground_brush),
			.block_missiles = item.hasFlag(ItemFlag::BlockMissiles),
			.wall = item.hasFlag(ItemFlag::IsWall),
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

	// Ground brushes named "mountain" that cannot paint are reported and kept out of the
	// palettes for this session. The brush itself stays registered, so maps that use its
	// items still load and render.
	void DropBrokenMountainBrushes(std::vector<std::string>& warnings) {
		for (const auto& [name, owned] : g_brushes.getMap()) {
			const GroundBrush* ground = owned ? owned->as<GroundBrush>() : nullptr;
			if (!ground) {
				continue;
			}
			std::vector<MountainBrushItem> items;
			for (const auto& block : ground->getBorderItems()) {
				const ItemDefinitionView item = g_item_definitions.get(block.id);
				items.push_back(MountainBrushItem { .server_id = block.id, .exists = static_cast<bool>(item), .ground = item && item.isGroundTile() });
			}
			const std::string reason = DescribeBrokenMountainBrush(ground->getName(), items);
			if (reason.empty()) {
				continue;
			}
			const size_t removed = g_materials.paletteCatalog().removeBrushFromTilesets(ground);
			warnings.push_back(std::format("Mountain brush \"{}\" is broken ({}); hidden from {} tileset(s)", ground->getName(), reason, removed));
		}
	}

} // namespace

void AddDetectedPalettes(std::vector<std::string>& warnings) {
	if (!g_settings.getBoolean(Config::SHOW_DETECTED_PALETTES)) {
		return;
	}

	DropBrokenMountainBrushes(warnings);

	DetectedTileset unregistered_borders { .name = "Unregistered borders" };
	DetectedTileset grounds_without_brush { .name = "Grounds without ground brush" };
	DetectedTileset brushes_without_border { .name = "Ground brushes without border" };
	DetectedTileset mountain_grounds { .name = "Unregistered mountain grounds" };
	DetectedTileset mountain_borders { .name = "Unregistered mountain borders" };
	DetectedTileset custom_objects { .name = "Objects" };
	DetectedTileset custom_grounds { .name = "Grounds" };
	DetectedTileset custom_borders { .name = "Borders" };
	DetectedTileset custom_walls { .name = "Walls" };
	const ClientVersion* version = g_version.getLoadedVersion();
	const std::optional<uint32_t> stock_item_count = version ? version->getStockItemCount() : std::nullopt;

	for (const ServerItemId id : g_item_definitions.allIds()) {
		if (id < 100) {
			continue;
		}
		const ItemDefinitionView item = g_item_definitions.get(id);
		if (!item) {
			continue;
		}
		const DetectedItemFacts facts = CollectFacts(item);
		DetectedTileset* custom_target = nullptr;
		switch (ClassifyCustomItem(facts, stock_item_count)) {
			case CustomItemKind::Object:
				custom_target = &custom_objects;
				break;
			case CustomItemKind::Ground:
				custom_target = &custom_grounds;
				break;
			case CustomItemKind::Border:
				custom_target = &custom_borders;
				break;
			case CustomItemKind::Wall:
				custom_target = &custom_walls;
				break;
			case CustomItemKind::Stock:
				break;
		}
		DetectedTileset* target = nullptr;
		switch (ClassifyUnregisteredMountain(facts)) {
			case MountainPiece::Ground:
				target = &mountain_grounds;
				break;
			case MountainPiece::Border:
				target = &mountain_borders;
				break;
			case MountainPiece::None:
				break;
		}
		if (!target && IsUnregisteredBorder(facts)) {
			target = &unregistered_borders;
		}
		if (!target) {
			switch (ClassifyGroundBorder(facts)) {
				case GroundBorderState::NoGroundBrush:
					target = &grounds_without_brush;
					break;
				case GroundBorderState::BrushWithoutBorder:
					target = &brushes_without_border;
					break;
				case GroundBorderState::NotAFloor:
				case GroundBorderState::Bordered:
					break;
			}
		}
		if (!target && !custom_target) {
			continue;
		}
		RAWBrush* brush = g_materials.ensureRawBrush(id);
		if (!brush) {
			continue;
		}
		if (target) {
			target->brushes.push_back(brush);
		}
		if (custom_target) {
			custom_target->brushes.push_back(brush);
		}
	}

	AddPalette("Detected Borders", { std::move(unregistered_borders) });
	AddPalette("Detected Grounds", { std::move(grounds_without_brush), std::move(brushes_without_border) });
	AddPalette("Detected Mountains", { std::move(mountain_grounds), std::move(mountain_borders) });
	if (!stock_item_count) {
		spdlog::info("Custom Items palette skipped: no stockItemCount in clients.toml for this client");
	}
	AddPalette("Custom Items", { std::move(custom_objects), std::move(custom_grounds), std::move(custom_borders), std::move(custom_walls) });
}
