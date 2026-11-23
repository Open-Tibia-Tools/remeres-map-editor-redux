//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "editor/operations/magic_wand_plan.h"

#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/connected_positions.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/spawn.h"
#include "map/tile.h"

#include <algorithm>
#include <format>
#include <type_traits>

using MagicWand::HitKind;

namespace {

	using MagicWand::ItemFamily;
	using MagicWand::Plan;
	using MagicWand::Target;
	using MagicWand::Tick;
	using MagicWand::TileLookup;
	using MagicWand::WandHit;

	bool Report(const Tick& tick, std::string_view phase, size_t done, size_t total) {
		return !tick || tick(phase, done, total);
	}

	bool WallOwns(WallBrush* wall, Item* item) {
		return item->isWall() && (item->getWallBrush() == wall || wall->hasWall(item));
	}

	bool CarpetOwns(CarpetBrush* carpet, Item* item) {
		return item->getCarpetBrush() == carpet || carpet->ownsItem(item->getID());
	}

	bool TableOwns(TableBrush* table, Item* item) {
		return item->getTableBrush() == table || table->ownsItem(item->getID());
	}

	bool ItemInFamily(const Item& item, const std::string& key) {
		const std::string_view name = item.getDefinition().name();
		if (!name.empty()) {
			return name == key;
		}
		// "#<id>" keys stand for nameless items; never group them by an empty name.
		return key.size() > 1 && key.front() == '#' && std::format("#{}", item.getID()) == key;
	}

	bool HoldsFamily(const Tile& tile, const std::string& key) {
		if (tile.ground && ItemInFamily(*tile.ground, key)) {
			return true;
		}
		return std::ranges::any_of(tile.items, [&key](const auto& item) {
			return ItemInFamily(*item, key);
		});
	}

	bool GroundHoldsFamily(const Tile& tile, const std::string& key) {
		return tile.ground && ItemInFamily(*tile.ground, key);
	}

	// A "#id" key stands for a nameless item; it must never match entities by name.
	bool NameMatchingEnabled(std::string_view name) {
		return !name.empty() && name.front() != '#';
	}

	bool ItemNamed(const Item& item, std::string_view name) {
		return !name.empty() && item.getDefinition().name() == name;
	}

	// Name matching spans entity kinds: a wall, a doodad and a raw item that share a definition
	// name belong to the same selection when they touch (or anywhere in the viewport).
	bool TileHasNamedEntity(const Tile& tile, std::string_view name) {
		if (!NameMatchingEnabled(name)) {
			return false;
		}
		if (tile.ground && ItemNamed(*tile.ground, name)) {
			return true;
		}
		return std::ranges::any_of(tile.items, [&](const auto& item) {
			return ItemNamed(*item, name);
		});
	}

	Plan ConnectedPlan(const std::vector<Position>& tiles) {
		Plan plan;
		plan.target_tiles = tiles.size();
		plan.entries.reserve(tiles.size());
		for (const Position& position : tiles) {
			plan.entries.push_back({ position, false });
		}
		return plan;
	}

	std::optional<Plan> CollectGround(
		GroundBrush* ground,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		std::string_view name
	) {
		const auto patch = ConnectedPositions::collect(
			origin,
			[&](const Position& position) {
				const Tile* tile = lookup(position);
				return tile && (tile->getGroundBrush() == ground || TileHasNamedEntity(*tile, name));
			},
			[&](size_t found) { return Report(tick, "Finding connected tiles...", found, 0); },
			&viewport
		);
		if (!patch) {
			return std::nullopt;
		}

		const PositionVector ring = ConnectedPositions::borderRing(*patch, &viewport);
		Plan plan;
		plan.target_tiles = patch->size();
		plan.entries.reserve(patch->size() + ring.size());
		for (const Position& position : *patch) {
			const Tile* tile = lookup(position);
			const bool ground_selected = tile && (tile->getGroundBrush() == ground || (tile->ground && ItemNamed(*tile->ground, name)));
			plan.entries.push_back({ position, ground_selected });
		}
		for (size_t index = 0; index < ring.size(); ++index) {
			if ((index % 1024) == 0 && !Report(tick, "Finding borders...", index, ring.size())) {
				return std::nullopt;
			}
			if (lookup(ring[index])) {
				plan.entries.push_back({ ring[index], false });
			}
		}
		return plan;
	}

	template <typename Owns>
	std::optional<Plan> CollectConnectedTarget(
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		std::string_view phase,
		std::string_view name,
		Owns&& owns
	) {
		const auto tiles = ConnectedPositions::collect(
			origin,
			[&](const Position& position) {
				const Tile* tile = lookup(position);
				if (!tile) {
					return false;
				}
				if (TileHasNamedEntity(*tile, name)) {
					return true;
				}
				return std::ranges::any_of(tile->items, [&](const auto& item) {
					return owns(item.get());
				});
			},
			[&](size_t found) { return Report(tick, phase, found, 0); },
			&viewport
		);
		if (!tiles) {
			return std::nullopt;
		}
		return ConnectedPlan(*tiles);
	}

	// Doodads and creatures are scattered, so they are collected from every visible tile:
	// the plan is limited by the viewport, not by connectivity.
	template <typename Matches>
	std::optional<Plan> CollectScattered(
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		std::string_view phase,
		Matches&& matches
	) {
		Plan plan;
		const uint64_t total = viewport.tileCount();
		uint64_t scanned = 0;
		for (int y = viewport.min_y; y <= viewport.max_y; ++y) {
			for (int x = viewport.min_x; x <= viewport.max_x; ++x) {
				++scanned;
				if ((scanned == 1 || (scanned % 256) == 0) && !Report(tick, phase, static_cast<size_t>(scanned), static_cast<size_t>(total))) {
					return std::nullopt;
				}
				const Tile* tile = lookup(Position(x, y, origin.z));
				if (!tile) {
					continue;
				}
				const std::optional<bool> select_ground = matches(*tile);
				if (select_ground) {
					plan.entries.push_back({ tile->getPosition(), *select_ground });
				}
			}
		}
		plan.target_tiles = plan.entries.size();
		return plan;
	}

	// Item families are viewport-wide: identical-looking variants are often scattered.
	std::optional<Plan> CollectItemFamily(
		const ItemFamily& family,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick
	) {
		return CollectScattered(lookup, origin, viewport, tick, "Searching this floor...", [&](const Tile& tile) -> std::optional<bool> {
			if (!HoldsFamily(tile, family.key)) {
				return std::nullopt;
			}
			return GroundHoldsFamily(tile, family.key);
		});
	}

	std::optional<Target> ResolveItem(Tile& tile, Item* item) {
		if (!item) {
			return std::nullopt;
		}
		// A border belongs to the ground that produced it; that is the patch the user means.
		if (item->isBorder()) {
			if (GroundBrush* ground = tile.getGroundBrush(); ground && ground->ownsBorderItem(item->getID())) {
				return ground;
			}
			return ItemFamily { MagicWand::ItemFamilyKey(*item) };
		}
		if (GroundBrush* ground = item->getGroundBrush()) {
			return ground;
		}
		if (WallBrush* wall = item->getWallBrush()) {
			return wall;
		}
		if (CarpetBrush* carpet = item->getCarpetBrush()) {
			return carpet;
		}
		if (TableBrush* table = item->getTableBrush()) {
			return table;
		}
		if (Brush* doodad = item->getDoodadBrush(); doodad && doodad->is<DoodadBrush>()) {
			return doodad->as<DoodadBrush>();
		}
		// Raw items, decorations and identical-looking variants are matched by definition name.
		return ItemFamily { MagicWand::ItemFamilyKey(*item) };
	}

} // namespace

std::string MagicWand::ItemFamilyKey(const Item& item) {
	const std::string_view name = item.getDefinition().name();
	if (name.empty()) {
		return std::format("#{}", item.getID());
	}
	return std::string(name);
}

std::optional<MagicWand::Target> MagicWand::ResolveTarget(const WandHit& hit) {
	if (!hit.tile) {
		return std::nullopt;
	}
	Tile& tile = *hit.tile;

	if (hit.kind == HitKind::Creature) {
		if (hit.creature) {
			return CreatureTarget { hit.creature->getName() };
		}
	} else if (hit.kind == HitKind::Ground || hit.kind == HitKind::Item) {
		if (auto target = ResolveItem(tile, hit.item)) {
			return target;
		}
	}

	// No entity of its own (empty hit, or a spawn marker): fall back to what standard selection
	// would pick on this tile, top item first and the ground last.
	if (auto target = ResolveItem(tile, tile.getTopItem())) {
		return target;
	}
	if (tile.ground) {
		return ResolveItem(tile, tile.ground.get());
	}
	return std::nullopt;
}

std::optional<MagicWand::Plan> MagicWand::BuildPlan(
	const Target& target,
	const TileLookup& lookup,
	const Position& origin,
	const PositionRect& viewport,
	const Tick& tick
) {
	if (viewport.empty()) {
		return Plan {};
	}

	// Name matching extends every brush kind: a wall, a doodad or an item that share a name
	// belong to the same selection when they touch. Computed once, no per-tile allocation.
	const std::string name = DescribeTarget(target);

	return std::visit(
		[&](const auto& value) -> std::optional<Plan> {
			using ValueType = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<ValueType, GroundBrush*>) {
				return CollectGround(value, lookup, origin, viewport, tick, name);
			} else if constexpr (std::is_same_v<ValueType, WallBrush*>) {
				return CollectConnectedTarget(lookup, origin, viewport, tick, "Finding connected walls...", name, [&](Item* item) {
					return WallOwns(value, item);
				});
			} else if constexpr (std::is_same_v<ValueType, CarpetBrush*>) {
				return CollectConnectedTarget(lookup, origin, viewport, tick, "Finding connected carpets...", name, [&](Item* item) {
					return CarpetOwns(value, item);
				});
			} else if constexpr (std::is_same_v<ValueType, TableBrush*>) {
				return CollectConnectedTarget(lookup, origin, viewport, tick, "Finding connected tables...", name, [&](Item* item) {
					return TableOwns(value, item);
				});
			} else if constexpr (std::is_same_v<ValueType, DoodadBrush*>) {
				return CollectScattered(lookup, origin, viewport, tick, "Searching this floor...", [&](const Tile& tile) -> std::optional<bool> {
					const bool ground_match = tile.ground && value->ownsItem(tile.ground.get());
					const bool item_match = std::ranges::any_of(tile.items, [&](const auto& item) { return value->ownsItem(item.get()); });
					const bool name_match = TileHasNamedEntity(tile, name);
					if (!ground_match && !item_match && !name_match) {
						return std::nullopt;
					}
					return ground_match || (tile.ground && ItemNamed(*tile.ground, name));
				});
			} else if constexpr (std::is_same_v<ValueType, ItemFamily>) {
				return CollectItemFamily(value, lookup, origin, viewport, tick);
			} else {
				static_assert(std::is_same_v<ValueType, CreatureTarget>, "unhandled wand target");
				return CollectScattered(lookup, origin, viewport, tick, "Searching this floor...", [&](const Tile& tile) -> std::optional<bool> {
					if (!tile.creature || tile.creature->getName() != value.name) {
						return std::nullopt;
					}
					return false;
				});
			}
		},
		target
	);
}

void MagicWand::SelectMatching(Tile& tile, const Target& target, bool select_ground, std::string_view target_name) {
	std::visit(
		[&](const auto& value) {
			using ValueType = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<ValueType, GroundBrush*>) {
				if (select_ground && tile.ground && tile.ground->getGroundBrush() == value) {
					tile.ground->select();
				}
				for (const auto& item : tile.items) {
					if (item->isBorder() && value->ownsBorderItem(item->getID())) {
						item->select();
					}
				}
			} else if constexpr (std::is_same_v<ValueType, WallBrush*>) {
				for (const auto& item : tile.items) {
					if (WallOwns(value, item.get())) {
						item->select();
					}
				}
			} else if constexpr (std::is_same_v<ValueType, CarpetBrush*>) {
				for (const auto& item : tile.items) {
					if (CarpetOwns(value, item.get())) {
						item->select();
					}
				}
			} else if constexpr (std::is_same_v<ValueType, TableBrush*>) {
				for (const auto& item : tile.items) {
					if (TableOwns(value, item.get())) {
						item->select();
					}
				}
			} else if constexpr (std::is_same_v<ValueType, DoodadBrush*>) {
				if (select_ground && tile.ground && value->ownsItem(tile.ground.get())) {
					tile.ground->select();
				}
				for (const auto& item : tile.items) {
					if (value->ownsItem(item.get())) {
						item->select();
					}
				}
			} else if constexpr (std::is_same_v<ValueType, ItemFamily>) {
				if (select_ground && tile.ground && ItemInFamily(*tile.ground, value.key)) {
					tile.ground->select();
				}
				for (const auto& item : tile.items) {
					if (ItemInFamily(*item, value.key)) {
						item->select();
					}
				}
			} else {
				static_assert(std::is_same_v<ValueType, CreatureTarget>, "unhandled wand target");
				if (tile.creature && tile.creature->getName() == value.name) {
					tile.creature->select();
				}
			}
		},
		target
	);

	// Same-name entities of other kinds join the selection (a doodad named like the wall, a raw
	// item named like the ground). Creatures only ever match creatures, and nameless items
	// ("#id" families) only match their own id.
	if (std::holds_alternative<CreatureTarget>(target) || !NameMatchingEnabled(target_name)) {
		return;
	}
	if (tile.ground && ItemNamed(*tile.ground, target_name)) {
		tile.ground->select();
	}
	for (const auto& item : tile.items) {
		if (ItemNamed(*item, target_name)) {
			item->select();
		}
	}
}

bool MagicWand::SameSelection(const Tile& live, const Tile& copy) {
	const auto selected = [](const auto& entity) { return entity && entity->isSelected(); };
	const bool meta_differs = selected(live.ground) != selected(copy.ground) ||
		selected(live.spawn) != selected(copy.spawn) ||
		selected(live.creature) != selected(copy.creature);
	if (meta_differs) {
		return false;
	}
	if (live.items.size() != copy.items.size()) {
		return false;
	}
	for (size_t index = 0; index < live.items.size(); ++index) {
		if (live.items[index]->isSelected() != copy.items[index]->isSelected()) {
			return false;
		}
	}
	return true;
}

std::string MagicWand::DescribeTarget(const Target& target) {
	return std::visit(
		[](const auto& value) -> std::string {
			using ValueType = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<ValueType, ItemFamily>) {
				return value.key;
			} else if constexpr (std::is_same_v<ValueType, CreatureTarget>) {
				return value.name;
			} else {
				return value->getName();
			}
		},
		target
	);
}
