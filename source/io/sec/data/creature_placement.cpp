//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/creature_placement.h"

#include "app/settings.h"
#include "brushes/creature/creature_brush.h"
#include "game/creature.h"
#include "game/creatures.h"
#include "game/materials.h"
#include "game/spawn.h"
#include "game/srv_homes.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <cstdlib>
#include <format>

namespace rme::sec {

	namespace {
		constexpr size_t kMaxListedUnplacedHomes = 30;
	}

	CreatureType* serverCreatureType(const std::string& name, bool is_npc, const CreatureOutfit& look) {
		if (CreatureType* type = g_creatures[name]) {
			return type;
		}
		Outfit outfit(look.look_type, look.head, look.body, look.legs, look.feet, 0);
		outfit.lookItem = look.look_item;
		return g_creatures.addCreatureType(name, is_npc, outfit);
	}

	void addServerCreaturePalette(std::vector<CreatureType*> monsters, std::vector<CreatureType*> npcs) {
		const auto tileset = [](std::string name, std::vector<CreatureType*>& types) {
			std::ranges::sort(types, {}, [](const CreatureType* type) { return as_lower_str(type->name); });
			DynamicTilesetDefinition definition { .name = std::move(name) };
			for (CreatureType* type : types) {
				if (CreatureBrush* brush = g_materials.ensureCreatureBrush(type)) {
					brush->flagAsVisible();
					if (!definition.containsBrush(brush)) {
						definition.brushes.push_back(brush);
					}
				}
			}
			return definition;
		};

		DynamicPaletteDefinition palette { .name = "Creature" };
		palette.tilesets.push_back(tileset("Monsters", monsters));
		palette.tilesets.push_back(tileset("NPCs", npcs));
		g_materials.paletteCatalog().addDynamicPalette(std::move(palette));
	}

	CreaturePlacer::CreaturePlacer(Map& map) :
		map_(map), max_radius_(std::max(1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS))) { }

	size_t CreaturePlacer::place(CreatureType* type, bool is_npc, const AbsolutePosition& home, int radius, int amount, int spawn_time) {
		const auto wanted = static_cast<size_t>(std::max(0, amount));
		if (wanted == 0) {
			return 0;
		}
		if (home.z < 0 || home.z >= MAP_LAYERS || home.x < 0 || home.y < 0 || home.x >= MAP_MAX_WIDTH || home.y >= MAP_MAX_HEIGHT) {
			noteUnplaced(type, home, radius, wanted);
			return 0;
		}
		// Spawn coverage is counted per tile in the radius, so a few huge
		// radii (some homes use 5000) would cost minutes and gigabytes.
		const int spawn_radius = rme::srv::spawnSizeFor(radius, max_radius_);
		if (radius > max_radius_) {
			++capped_radii_;
		}
		Tile* center = map_.createTile(home.x, home.y, home.z);
		if (center->spawn) {
			if (center->spawn->getSize() < spawn_radius) {
				// Coverage is counted when a spawn is added, so re-add it at its new size.
				map_.removeSpawn(center);
				center->spawn->setSize(spawn_radius);
				map_.addSpawn(center);
			}
		} else {
			center->spawn = std::make_unique<Spawn>(spawn_radius);
			map_.addSpawn(center);
		}

		Tile* tile = center->creature ? findFreeTile(home, spawn_radius) : center;
		if (!tile) {
			noteUnplaced(type, home, radius, wanted);
			return 0;
		}
		tile->creature = std::make_unique<Creature>(type);
		tile->creature->setSpawnTime(spawn_time);
		tile->creature->setAmount(static_cast<int>(wanted));
		tile->creature->setHomeRadius(radius);
		if (is_npc) {
			tile->creature->markAsNpcSpawn();
		}
		return wanted;
	}

	void CreaturePlacer::report(std::vector<std::string>& warnings) const {
		if (unplaced_ > 0) {
			warnings.push_back(std::format("{} creatures could not be placed (their home tile is taken and no free walkable tile is within the radius):", unplaced_));
			for (const std::string& line : unplaced_lines_) {
				warnings.push_back("    " + line);
			}
			if (unplaced_homes_ > unplaced_lines_.size()) {
				warnings.push_back(std::format("    ... and {} more spawns", unplaced_homes_ - unplaced_lines_.size()));
			}
		}
		if (capped_radii_ > 0) {
			warnings.push_back(std::format("{} monster homes and NPCs have a radius above the editor's maximum spawn radius ({}); their spawns use {}. Raise max_spawn_radius in config.toml to keep larger radii (most homes use 50).", capped_radii_, max_radius_, max_radius_));
		}
	}

	Tile* CreaturePlacer::findFreeTile(const AbsolutePosition& home, int radius) const {
		for (int ring = 1; ring <= radius; ++ring) {
			for (int dy = -ring; dy <= ring; ++dy) {
				for (int dx = -ring; dx <= ring; ++dx) {
					if (std::max(std::abs(dx), std::abs(dy)) != ring) {
						continue;
					}
					Tile* tile = map_.getTile(home.x + dx, home.y + dy, home.z);
					if (tile && tile->hasGround() && !tile->isBlocking() && !tile->creature) {
						return tile;
					}
				}
			}
		}
		return nullptr;
	}

	void CreaturePlacer::noteUnplaced(const CreatureType* type, const AbsolutePosition& home, int radius, size_t count) {
		if (count == 0) {
			return;
		}
		unplaced_ += count;
		++unplaced_homes_;
		if (unplaced_lines_.size() < kMaxListedUnplacedHomes) {
			unplaced_lines_.push_back(std::format("{} x {} at [{}, {}, {}], radius {}", count, type ? type->name : std::string("?"), home.x, home.y, home.z, radius));
		}
	}

} // namespace rme::sec
