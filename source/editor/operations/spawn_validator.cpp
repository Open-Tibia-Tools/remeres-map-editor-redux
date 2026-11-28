#include "editor/operations/spawn_validator.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "editor/action.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/creature.h"
#include "game/creatures.h"
#include "game/spawn.h"

#include <algorithm>
#include <format>
#include <unordered_set>
#include <unordered_map>

namespace EditorOperations {

std::string_view SpawnIssue::GetTypeString() const noexcept {
	switch (type) {
		case SpawnIssueType::InvalidSpawntime:
			return "Invalid Spawntime";
		case SpawnIssueType::UnknownCreature:
			return "Unknown Creature";
		case SpawnIssueType::KindMismatch:
			return "Kind Mismatch";
		case SpawnIssueType::BlockedTile:
			return "Blocked / Wall";
		case SpawnIssueType::MissingTile:
			return "Missing Tile";
		case SpawnIssueType::EmptyTile:
			return "Empty Tile";
		case SpawnIssueType::UnwalkableZone:
			return "Unwalkable Spawn";
	}
	return "Issue";
}

std::string_view SpawnIssue::GetKindString() const noexcept {
	return is_npc ? "NPC" : "Monster";
}

namespace {

// The checks of one placed creature: its tile, spawn time, type and
// alternatives. `center` is its spawn, or on a sector map its own home.
void checkCreature(const Tile& tile, const Position& pos, const Position& center, const SpawnValidationSettings& settings, std::vector<SpawnIssue>& issues, uint32_t& nextId) {
	const Creature* creature = tile.creature.get();
	const std::string& name = creature->getName();
	const bool isNpc = creature->isNpc();
	const int spawntime = creature->getSpawnTime();

	// 1. Check tile validity for the placed creature
	if (!tile.hasGround()) {
		if (settings.check_missing_or_empty_tiles) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::EmptyTile,
				.description = "Tile has no ground; server voids or drops it on load."
			});
		}
	} else if (tile.isBlocking()) {
		if (settings.check_blocked_tiles) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::BlockedTile,
				.description = "Tile is blocked by a wall or unpassable obstacle."
			});
		}
	} else if (!isNpc && tile.isPZ()) {
		if (settings.check_blocked_tiles) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::BlockedTile,
				.description = "Monster placed in Protection Zone (monsters cannot spawn in PZ)."
			});
		}
	}

	// 2. Check spawntime
	if (settings.check_invalid_spawntime) {
		if (spawntime < settings.min_spawntime || spawntime > settings.max_spawntime) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::InvalidSpawntime,
				.description = std::format("Spawntime is {}s (valid range: [{}s, {}s]).", spawntime, settings.min_spawntime, settings.max_spawntime)
			});
		}
	}

	// 3. Check creature type definition
	if (settings.check_unknown_creature) {
		CreatureType* type = g_creatures[name];
		if (!type || type->missing) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::UnknownCreature,
				.description = std::format("Creature '{}' not found in loaded creature database.", name)
			});
		} else if (type->isNpc != isNpc) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = name,
				.pos = pos,
				.spawn_center = center,
				.spawntime = spawntime,
				.is_npc = isNpc,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::KindMismatch,
				.description = std::format("'{}' registered as {} but placed as {}.", name, type->isNpc ? "NPC" : "Monster", isNpc ? "NPC" : "Monster")
			});
		}
	}

	// 4. Check OTBM 5 alternative creatures on this tile
	const auto& alternatives = creature->getAlternatives();
	for (uint32_t altIdx = 0; altIdx < static_cast<uint32_t>(alternatives.size()); ++altIdx) {
		const auto& alt = alternatives[altIdx];

		if (settings.check_invalid_spawntime) {
			if (alt.spawntime < settings.min_spawntime || alt.spawntime > settings.max_spawntime) {
				issues.push_back(SpawnIssue{
					.id = nextId++,
					.name = alt.name,
					.pos = pos,
					.spawn_center = center,
					.spawntime = alt.spawntime,
					.is_npc = false,
					.is_alternative = true,
					.alternative_index = altIdx,
					.type = SpawnIssueType::InvalidSpawntime,
					.description = std::format("Alternative #{} spawntime is {}s (valid range: [{}s, {}s]).", altIdx + 1, alt.spawntime, settings.min_spawntime, settings.max_spawntime)
				});
			}
		}

		if (settings.check_unknown_creature) {
			CreatureType* altType = g_creatures[alt.name];
			if (!altType || altType->missing) {
				issues.push_back(SpawnIssue{
					.id = nextId++,
					.name = alt.name,
					.pos = pos,
					.spawn_center = center,
					.spawntime = alt.spawntime,
					.is_npc = false,
					.is_alternative = true,
					.alternative_index = altIdx,
					.type = SpawnIssueType::UnknownCreature,
					.description = std::format("Alternative #{} creature '{}' not found in database.", altIdx + 1, alt.name)
				});
			} else if (altType->isNpc) {
				issues.push_back(SpawnIssue{
					.id = nextId++,
					.name = alt.name,
					.pos = pos,
					.spawn_center = center,
					.spawntime = alt.spawntime,
					.is_npc = false,
					.is_alternative = true,
					.alternative_index = altIdx,
					.type = SpawnIssueType::KindMismatch,
					.description = std::format("Alternative #{} '{}' is an NPC, not a monster.", altIdx + 1, alt.name)
				});
			}
		}
	}
}

} // namespace

std::vector<SpawnIssue> SpawnValidator::ScanMap(Map& map, const SpawnValidationSettings& settings) {
	std::vector<SpawnIssue> issues;
	uint32_t nextId = 1;

	struct PositionHash {
		size_t operator()(const Position& p) const noexcept {
			return (static_cast<size_t>(p.x) * 73856093) ^
			       (static_cast<size_t>(p.y) * 19349663) ^
			       (static_cast<size_t>(p.z) * 83492791);
		}
	};
	std::unordered_set<Position, PositionHash> visitedCreatures;

	for (const Position& spawnPos : map.spawns) {
		const Tile* centerTile = map.getTile(spawnPos);
		const Spawn* spawn = centerTile ? centerTile->spawn.get() : nullptr;
		if (!spawn) {
			continue;
		}

		const int32_t radius = std::max({ 1, spawn->getSize(), spawn->getNpcSize() });
		const bool spawnHasMonsters = (spawn->getKinds() & Spawn::MONSTERS) != 0;
		uint32_t walkableTiles = 0;
		uint32_t checkedTiles = 0;

		for (int32_t dy = -radius; dy <= radius; ++dy) {
			for (int32_t dx = -radius; dx <= radius; ++dx) {
				Position checkPos(spawnPos.x + dx, spawnPos.y + dy, spawnPos.z);
				++checkedTiles;

				const Tile* tile = map.getTile(checkPos);
				const bool tileHasGround = (tile != nullptr && tile->hasGround());
				const bool tileBlocking = (tile == nullptr || tile->isBlocking());

				// A tile is walkable if it exists, has ground, and is not blocked by walls/obstacles
				if (tileHasGround && !tileBlocking) {
					if (!spawnHasMonsters || !tile->isPZ()) {
						++walkableTiles;
					}
				}

				if (!tile || !tile->creature) {
					continue;
				}

				if (!visitedCreatures.insert(checkPos).second) {
					continue;
				}

				checkCreature(*tile, checkPos, spawnPos, settings, issues, nextId);
			}
		}

		// 5. Check if spawn area has zero walkable tiles
		if (settings.check_unwalkable_zone && walkableTiles == 0 && checkedTiles > 0) {
			issues.push_back(SpawnIssue{
				.id = nextId++,
				.name = "Spawn Area",
				.pos = spawnPos,
				.spawn_center = spawnPos,
				.spawntime = 0,
				.is_npc = (spawn->getKinds() & Spawn::NPCS) != 0,
				.is_alternative = false,
				.alternative_index = 0,
				.type = SpawnIssueType::UnwalkableZone,
				.description = std::format("Spawn radius {} contains 0 walkable tiles; creatures cannot spawn.", radius)
			});
		}
	}

	// Sector maps have no spawns: each creature with a home radius is a
	// home of its own (game/srv_homes.h), centred on its own tile.
	for (auto& location : map.tiles()) {
		const Tile* tile = location.get();
		if (!tile || !tile->creature || !tile->creature->getHomeRadius()) {
			continue;
		}
		const Position& pos = tile->getPosition();
		if (visitedCreatures.insert(pos).second) {
			checkCreature(*tile, pos, pos, settings, issues, nextId);
		}
	}

	return issues;
}

bool SpawnValidator::RemoveIssue(Editor& editor, const SpawnIssue& issue) {
	if (issue.type == SpawnIssueType::UnwalkableZone) {
		Tile* centerTile = editor.map.getTile(issue.spawn_center);
		if (!centerTile || !centerTile->spawn) {
			return false;
		}

		std::unique_ptr<Tile> newTile = centerTile->deepCopy();
		newTile->spawn.reset();

		auto action = editor.actionQueue->createAction(ACTION_DELETE_TILES);
		action->addChange(std::make_unique<Change>(std::move(newTile)));
		editor.addAction(std::move(action));
		return true;
	}

	Tile* tile = editor.map.getTile(issue.pos);
	if (!tile || !tile->creature) {
		return false;
	}

	std::unique_ptr<Tile> newTile = tile->deepCopy();
	if (issue.is_alternative) {
		auto& alts = newTile->creature->getAlternatives();
		if (issue.alternative_index >= alts.size()) {
			return false;
		}
		alts.erase(alts.begin() + issue.alternative_index);
	} else {
		newTile->creature.reset();
	}

	auto action = editor.actionQueue->createAction(ACTION_DELETE_TILES);
	action->addChange(std::make_unique<Change>(std::move(newTile)));
	editor.addAction(std::move(action));
	return true;
}

size_t SpawnValidator::RemoveAllIssues(Editor& editor, std::span<const SpawnIssue> issues) {
	if (issues.empty()) {
		return 0;
	}

	struct PositionHash {
		size_t operator()(const Position& p) const noexcept {
			return (static_cast<size_t>(p.x) * 73856093) ^
			       (static_cast<size_t>(p.y) * 19349663) ^
			       (static_cast<size_t>(p.z) * 83492791);
		}
	};

	struct TileRemovalPlan {
		bool remove_spawn = false;
		bool remove_creature = false;
		std::vector<uint32_t> alt_indices;
	};

	std::unordered_map<Position, TileRemovalPlan, PositionHash> plans;
	for (const auto& issue : issues) {
		if (issue.type == SpawnIssueType::UnwalkableZone) {
			plans[issue.spawn_center].remove_spawn = true;
		} else if (!issue.is_alternative) {
			plans[issue.pos].remove_creature = true;
		} else {
			plans[issue.pos].alt_indices.push_back(issue.alternative_index);
		}
	}

	auto action = editor.actionQueue->createAction(ACTION_DELETE_TILES);
	size_t count = 0;

	for (auto& [pos, plan] : plans) {
		Tile* tile = editor.map.getTile(pos);
		if (!tile) {
			continue;
		}

		std::unique_ptr<Tile> newTile = tile->deepCopy();
		bool changed = false;

		if (plan.remove_spawn && newTile->spawn) {
			newTile->spawn.reset();
			changed = true;
			++count;
		}

		if (plan.remove_creature && newTile->creature) {
			newTile->creature.reset();
			changed = true;
			++count;
		} else if (!plan.alt_indices.empty() && newTile->creature) {
			std::ranges::sort(plan.alt_indices, std::greater<uint32_t>());
			auto [first, last] = std::ranges::unique(plan.alt_indices);
			plan.alt_indices.erase(first, last);

			auto& alts = newTile->creature->getAlternatives();
			for (uint32_t idx : plan.alt_indices) {
				if (idx < alts.size()) {
					alts.erase(alts.begin() + idx);
					changed = true;
					++count;
				}
			}
		}

		if (changed) {
			action->addChange(std::make_unique<Change>(std::move(newTile)));
		}
	}

	if (count == 0) {
		return 0;
	}

	editor.addAction(std::move(action));
	return count;
}

} // namespace EditorOperations
