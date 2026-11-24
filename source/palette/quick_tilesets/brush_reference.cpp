//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/brush_reference.h"

#include "brushes/border/optional_border_brush.h"
#include "brushes/brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/flag/flag_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/town/town_temple_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/zone/zone_brush.h"
#include "game/creatures.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"

namespace QuickTilesets {

	BrushKind ClassifyBrush(const Brush& brush) {
		if (brush.is<RAWBrush>()) {
			return BrushKind::Raw;
		}
		if (brush.is<CreatureBrush>()) {
			return BrushKind::Creature;
		}
		if (brush.is<HouseBrush>() || brush.is<HouseExitBrush>() || brush.is<WaypointBrush>() || brush.is<TownTempleBrush>() || brush.is<ZoneBrush>()) {
			return BrushKind::PaletteContext;
		}
		if (brush.is<EraserBrush>() || brush.is<FlagBrush>() || brush.is<DoorBrush>() || brush.is<OptionalBorderBrush>() || brush.is<SpawnBrush>()) {
			return BrushKind::Tool;
		}
		return BrushKind::Named;
	}

	std::optional<Item> MakeReference(const Brush& brush) {
		switch (ClassifyBrush(brush)) {
			case BrushKind::Raw:
				return Item { .key = RawKey(brush.as<RAWBrush>()->getItemID()) };
			case BrushKind::Creature:
				return Item { .key = "c:" + brush.getName() };
			case BrushKind::Named:
				return Item { .key = "b:" + brush.getName() };
			case BrushKind::PaletteContext:
			case BrushKind::Tool:
				break;
		}
		return std::nullopt;
	}

	std::vector<Item> MakeReferences(std::span<Brush* const> brushes) {
		std::vector<Item> references;
		for (const Brush* brush : brushes) {
			if (!brush) {
				continue;
			}
			if (auto reference = MakeReference(*brush); reference && !ContainsKey(references, reference->key)) {
				references.push_back(std::move(*reference));
			}
		}
		return references;
	}

	Brush* Resolve(const Item& item) {
		if (item.isRaw()) {
			const auto server_id = ParseRawKey(item.key);
			if (!server_id || !g_item_definitions.get(*server_id)) {
				return nullptr;
			}
			return g_materials.ensureRawBrush(*server_id);
		}
		if (item.key.starts_with("c:")) {
			return g_materials.ensureCreatureBrush(g_creatures[item.key.substr(2)]);
		}
		if (item.key.starts_with("b:")) {
			Brush* brush = g_brushes.getBrush(std::string_view(item.key).substr(2));
			return brush && IsCollectable(ClassifyBrush(*brush)) ? brush : nullptr;
		}
		return nullptr;
	}

} // namespace QuickTilesets
