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
#include <type_traits>

using MagicWand::HitKind;

namespace {

	using MagicWand::ItemFamily;
	using MagicWand::Plan;
	using MagicWand::ResolvedTarget;
	using MagicWand::SearchMode;
	using MagicWand::TargetNames;
	using MagicWand::Tick;
	using MagicWand::TileLookup;

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

	// Brush names and client item names are written by different people ("Stone Wall" vs
	// "stone wall"), so identity comparisons ignore ASCII case. An empty name is a name like
	// any other: nameless entities share it and match each other.
	bool NamesEqual(std::string_view a, std::string_view b) {
		if (a.size() != b.size()) {
			return false;
		}
		for (size_t index = 0; index < a.size(); ++index) {
			const unsigned char left = static_cast<unsigned char>(a[index]);
			const unsigned char right = static_cast<unsigned char>(b[index]);
			const unsigned char lower_left = left >= 'A' && left <= 'Z' ? left + ('a' - 'A') : left;
			const unsigned char lower_right = right >= 'A' && right <= 'Z' ? right + ('a' - 'A') : right;
			if (lower_left != lower_right) {
				return false;
			}
		}
		return true;
	}

	bool ItemInFamily(const Item& item, const std::string& key) {
		// The definition name is the family identity; items without a definition name share the
		// empty name and therefore form one family too.
		return NamesEqual(item.getDefinition().name(), key);
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

	// Per-tile verdict of one target kind: nullopt = the tile is not part of the target,
	// otherwise an entry is needed and the bool says whether the ground slot itself belongs.
	using TileMatch = std::optional<bool>;

	bool GroundSelectsGroundSlot(const Tile& tile, GroundBrush* ground, const TargetNames& names) {
		return tile.getGroundBrush() == ground ||
			(tile.ground && MagicWand::NameMatches(names, tile.ground->getDefinition().name()));
	}

	// Viewport matcher of a ground: the ground itself, a border piece of the brush, or a
	// same-name entity (the viewport scan has no ring, so borders are collected here).
	TileMatch MatchGround(const Tile& tile, GroundBrush* ground, const TargetNames& names) {
		if (GroundSelectsGroundSlot(tile, ground, names)) {
			return true;
		}
		for (const auto& item : tile.items) {
			if (item->isBorder() && ground->ownsBorderItem(item->getID())) {
				return false;
			}
		}
		// Same-name entities on a foreign ground join without selecting that ground.
		if (std::ranges::any_of(tile.items, [&](const auto& item) {
				return MagicWand::NameMatches(names, item->getDefinition().name());
			})) {
			return false;
		}
		return std::nullopt;
	}

	// Neighbour membership of a ground patch: only grounds extend the flood (the brush's own,
	// or a ground slot carrying one of the identity names). Same-name items and border pieces
	// never chain the flood, otherwise two patches separated by a border run or a decoration
	// named like the ground would join into one selection; they are collected through the ring
	// instead.
	bool GroundBelongsToPatch(const Tile& tile, GroundBrush* ground, const TargetNames& names) {
		return GroundSelectsGroundSlot(tile, ground, names);
	}

	// Wall, carpet and table: the tile belongs when one of its items is owned by the brush or
	// an entity carries one of the identity names.
	template <typename Owns>
	TileMatch MatchOwnedItems(const Tile& tile, const TargetNames& names, Owns&& owns) {
		if (std::ranges::any_of(tile.items, [&](const auto& item) {
				return owns(item.get());
			})) {
			return false;
		}
		if ((tile.ground && MagicWand::NameMatches(names, tile.ground->getDefinition().name())) ||
			std::ranges::any_of(tile.items, [&](const auto& item) {
				return MagicWand::NameMatches(names, item->getDefinition().name());
			})) {
			return false;
		}
		return std::nullopt;
	}

	// Doodads live in the item stack or in the ground slot (Tile::addItem moves ground-typed
	// items there); both count, and the ground slot itself is selected with the doodad.
	TileMatch MatchDoodad(const Tile& tile, DoodadBrush* doodad, const TargetNames& names) {
		if (tile.ground && (doodad->ownsItem(tile.ground.get()) || MagicWand::NameMatches(names, tile.ground->getDefinition().name()))) {
			return true;
		}
		if (std::ranges::any_of(tile.items, [&](const auto& item) {
				return doodad->ownsItem(item.get());
			}) ||
			std::ranges::any_of(tile.items, [&](const auto& item) {
				return MagicWand::NameMatches(names, item->getDefinition().name());
			})) {
			return false;
		}
		return std::nullopt;
	}

	// Creatures only ever match creatures, by their own name.
	TileMatch MatchCreature(const Tile& tile, const MagicWand::CreatureTarget& target) {
		if (!tile.creature || tile.creature->getName() != target.name) {
			return std::nullopt;
		}
		return false;
	}

	TileMatch MatchFamily(const Tile& tile, const ItemFamily& family) {
		if (!HoldsFamily(tile, family.key)) {
			return std::nullopt;
		}
		return GroundHoldsFamily(tile, family.key);
	}

	// Viewport scan: every tile of the visible rectangle is tested once and matching tiles
	// become plan entries.
	template <typename Matches>
	std::optional<Plan> CollectViewport(
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		Matches&& matches
	) {
		Plan plan;
		const uint64_t total = viewport.tileCount();
		uint64_t scanned = 0;
		for (int y = viewport.min_y; y <= viewport.max_y; ++y) {
			for (int x = viewport.min_x; x <= viewport.max_x; ++x) {
				++scanned;
				if ((scanned == 1 || (scanned % 256) == 0) && !Report(tick, "Searching this floor...", static_cast<size_t>(scanned), static_cast<size_t>(total))) {
					return std::nullopt;
				}
				const Tile* tile = lookup(Position(x, y, origin.z));
				if (!tile) {
					continue;
				}
				const TileMatch select_ground = matches(*tile);
				if (select_ground) {
					plan.entries.push_back({ tile->getPosition(), *select_ground });
				}
			}
		}
		plan.target_tiles = plan.entries.size();
		return plan;
	}

	// Eight-connected flood around the origin (diagonals count, so a checkerboard of the same
	// ground is one patch); every visited match becomes a plan entry.
	template <typename Matches>
	std::optional<Plan> CollectNeighbours(
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		std::string_view phase,
		Matches&& matches
	) {
		const auto tiles = ConnectedPositions::collect(
			origin,
			[&](const Position& position) {
				const Tile* tile = lookup(position);
				return tile && matches(*tile).has_value();
			},
			[&](size_t found) { return Report(tick, phase, found, 0); },
			&viewport
		);
		if (!tiles) {
			return std::nullopt;
		}

		Plan plan;
		plan.target_tiles = tiles->size();
		plan.entries.reserve(tiles->size());
		for (const Position& position : *tiles) {
			const Tile* tile = lookup(position);
			plan.entries.push_back({ position, tile ? *matches(*tile) : false });
		}
		return plan;
	}

	template <typename Matches>
	std::optional<Plan> CollectByMode(
		SearchMode mode,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick,
		std::string_view phase,
		Matches&& matches
	) {
		if (mode == SearchMode::Viewport) {
			return CollectViewport(lookup, origin, viewport, tick, std::forward<Matches>(matches));
		}
		return CollectNeighbours(lookup, origin, viewport, tick, phase, std::forward<Matches>(matches));
	}

	// A ground in Neighbour mode: the connected patch plus the ring around it, where the
	// border pieces of the patch live.
	std::optional<Plan> CollectGroundNeighbour(
		GroundBrush* ground,
		const TargetNames& names,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick
	) {
		const auto patch = ConnectedPositions::collect(
			origin,
			[&](const Position& position) {
				const Tile* tile = lookup(position);
				return tile && GroundBelongsToPatch(*tile, ground, names);
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
			plan.entries.push_back({ position, tile && GroundSelectsGroundSlot(*tile, ground, names) });
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

	std::optional<ResolvedTarget> ResolveItem(Tile& tile, Item* item, const TileLookup& lookup) {
		if (!item) {
			return std::nullopt;
		}

		TargetNames names;
		names.item = MagicWand::ItemFamilyKey(*item);
		ResolvedTarget resolved;
		resolved.names = std::move(names);
		resolved.origin = tile.getPosition();

		// Brushes carry their own name; a nameless brush falls back to the clicked item's
		// definition name so it still matches its siblings and the status bar stays readable.
		const auto adopt = [&](auto* brush) {
			resolved.target = brush;
			resolved.names.primary = brush->getName();
			if (resolved.names.primary.empty()) {
				resolved.names.primary = resolved.names.item;
			}
		};

		// A border belongs to the ground that produced it; that is the patch the user means. The
		// producing ground is usually the tile's own, but ring borders sit on the neighbour's
		// ground, so the eight neighbours' grounds are searched too and the collection starts
		// from the owner's tile.
		if (item->isBorder()) {
			if (GroundBrush* ground = tile.getGroundBrush(); ground && ground->ownsBorderItem(item->getID())) {
				adopt(ground);
				return resolved;
			}
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					if (dx == 0 && dy == 0) {
						continue;
					}
					const Position position(tile.getX() + dx, tile.getY() + dy, tile.getZ());
					if (!position.isValid()) {
						continue;
					}
					const Tile* neighbour = lookup(position);
					if (!neighbour) {
						continue;
					}
					if (GroundBrush* ground = neighbour->getGroundBrush(); ground && ground->ownsBorderItem(item->getID())) {
						adopt(ground);
						resolved.origin = position;
						return resolved;
					}
				}
			}
			resolved.target = ItemFamily { resolved.names.item };
			resolved.names.primary = resolved.names.item;
			return resolved;
		}
		if (GroundBrush* ground = item->getGroundBrush()) {
			adopt(ground);
			return resolved;
		}
		if (WallBrush* wall = item->getWallBrush()) {
			adopt(wall);
			return resolved;
		}
		if (CarpetBrush* carpet = item->getCarpetBrush()) {
			adopt(carpet);
			return resolved;
		}
		if (TableBrush* table = item->getTableBrush()) {
			adopt(table);
			return resolved;
		}
		if (Brush* doodad = item->getDoodadBrush(); doodad && doodad->is<DoodadBrush>()) {
			adopt(doodad->as<DoodadBrush>());
			return resolved;
		}
		// Raw items, decorations and identical-looking variants are matched by definition name.
		resolved.target = ItemFamily { resolved.names.item };
		resolved.names.primary = resolved.names.item;
		return resolved;
	}

} // namespace

std::string MagicWand::ItemFamilyKey(const Item& item) {
	// The definition name verbatim: nameless items share the empty name and form one family.
	return std::string(item.getDefinition().name());
}

bool MagicWand::NameMatches(const TargetNames& names, std::string_view entity_name) {
	// The empty name is a name like any other: nameless entities are each other's siblings.
	return NamesEqual(entity_name, names.primary) || NamesEqual(entity_name, names.item);
}

std::optional<MagicWand::ResolvedTarget> MagicWand::ResolveTarget(const WandHit& hit, const TileLookup& lookup) {
	if (!hit.tile) {
		return std::nullopt;
	}
	Tile& tile = *hit.tile;

	if (hit.kind == HitKind::Creature) {
		if (hit.creature) {
			ResolvedTarget resolved;
			resolved.target = CreatureTarget { hit.creature->getName() };
			resolved.names.primary = hit.creature->getName();
			resolved.origin = tile.getPosition();
			return resolved;
		}
	} else if (hit.kind == HitKind::Ground || hit.kind == HitKind::Item) {
		if (auto resolved = ResolveItem(tile, hit.item, lookup)) {
			return resolved;
		}
	}

	// No entity of its own (empty hit, or a spawn marker): fall back to what standard selection
	// would pick on this tile, top item first and the ground last.
	if (auto resolved = ResolveItem(tile, tile.getTopItem(), lookup)) {
		return resolved;
	}
	if (tile.ground) {
		return ResolveItem(tile, tile.ground.get(), lookup);
	}
	return std::nullopt;
}

std::optional<MagicWand::Plan> MagicWand::BuildPlan(
	const Target& target,
	const TargetNames& names,
	SearchMode mode,
	const TileLookup& lookup,
	const Position& origin,
	const PositionRect& viewport,
	const Tick& tick
) {
	if (viewport.empty()) {
		return Plan {};
	}

	return std::visit(
		[&](const auto& value) -> std::optional<Plan> {
			using ValueType = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<ValueType, GroundBrush*>) {
				if (mode == SearchMode::Neighbour) {
					return CollectGroundNeighbour(value, names, lookup, origin, viewport, tick);
				}
				return CollectViewport(lookup, origin, viewport, tick, [&](const Tile& tile) {
					return MatchGround(tile, value, names);
				});
			} else if constexpr (std::is_same_v<ValueType, WallBrush*>) {
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected walls...", [&](const Tile& tile) {
					return MatchOwnedItems(tile, names, [&](Item* item) {
						return WallOwns(value, item);
					});
				});
			} else if constexpr (std::is_same_v<ValueType, CarpetBrush*>) {
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected carpets...", [&](const Tile& tile) {
					return MatchOwnedItems(tile, names, [&](Item* item) {
						return CarpetOwns(value, item);
					});
				});
			} else if constexpr (std::is_same_v<ValueType, TableBrush*>) {
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected tables...", [&](const Tile& tile) {
					return MatchOwnedItems(tile, names, [&](Item* item) {
						return TableOwns(value, item);
					});
				});
			} else if constexpr (std::is_same_v<ValueType, DoodadBrush*>) {
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected tiles...", [&](const Tile& tile) {
					return MatchDoodad(tile, value, names);
				});
			} else if constexpr (std::is_same_v<ValueType, ItemFamily>) {
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected tiles...", [&](const Tile& tile) {
					return MatchFamily(tile, value);
				});
			} else {
				static_assert(std::is_same_v<ValueType, CreatureTarget>, "unhandled wand target");
				return CollectByMode(mode, lookup, origin, viewport, tick, "Finding connected creatures...", [&](const Tile& tile) {
					return MatchCreature(tile, value);
				});
			}
		},
		target
	);
}

void MagicWand::SelectMatching(Tile& tile, const Target& target, const TargetNames& names, bool select_ground) {
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

	// Creatures only ever match creatures, so their identity never selects other kinds.
	if (std::holds_alternative<CreatureTarget>(target)) {
		return;
	}
	// Same-name entities of other kinds join the selection (a doodad named like the wall, a raw
	// item named like the ground, a border named like the clicked border). The empty name is a
	// name too, so nameless entities are each other's siblings.
	if (tile.ground && NameMatches(names, tile.ground->getDefinition().name())) {
		tile.ground->select();
	}
	for (const auto& item : tile.items) {
		if (NameMatches(names, item->getDefinition().name())) {
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
