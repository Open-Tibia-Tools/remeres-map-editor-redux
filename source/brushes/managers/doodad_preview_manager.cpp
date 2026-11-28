//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "brushes/managers/doodad_preview_manager.h"
#include "brushes/brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/managers/brush_manager.h"
#include "game/sprites.h"
#include "map/map.h"
#include "app/settings.h"
#include "ui/gui.h"
#include <cmath>
#include <numbers>
#include <vector>

DoodadPreviewManager g_doodad_preview;

DoodadPreviewManager::DoodadPreviewManager() :
	doodad_buffer_map(std::make_unique<BaseMap>()) {
}

DoodadPreviewManager::~DoodadPreviewManager() = default;

void DoodadPreviewManager::Clear() {
	doodad_buffer_map->clear();
}

void DoodadPreviewManager::FillBuffer() {
	Brush* current_brush = g_brush_manager.GetCurrentBrush();
	if (!current_brush || !current_brush->is<DoodadBrush>()) {
		return;
	}

	doodad_buffer_map->clear();

	DoodadBrush* brush = current_brush->as<DoodadBrush>();
	if (brush->isEmpty(g_brush_manager.GetBrushVariation())) {
		return;
	}

	const BrushFootprint footprint = g_brush_manager.GetBrushFootprint();
	std::vector<Position> valid_offsets;
	for (int y = footprint.min_offset_y; y <= footprint.max_offset_y; ++y) {
		for (int x = footprint.min_offset_x; x <= footprint.max_offset_x; ++x) {
			if (footprint.containsOffset(x, y)) {
				valid_offsets.emplace_back(x, y, 0);
			}
		}
	}

	const int variation = g_brush_manager.GetBrushVariation();
	Position center_pos(0x8000, 0x8000, 0x8);

	if (!footprint.isSingleTile() && !brush->oneSizeFitsAll()) {
		const int area = std::max(1, static_cast<int>(valid_offsets.size()));
		int object_count = 0;
		const int object_range = (g_brush_manager.UseCustomThickness() ? static_cast<int>(area * g_brush_manager.GetCustomThicknessMod()) : brush->getThickness() * area / std::max(1, brush->getThicknessCeiling()));

		std::mt19937 seeded_rng(static_cast<uint32_t>(1337u + static_cast<uint32_t>(variation) * 37u));
		auto next_rand = [&](int min_val, int max_val) -> int {
			if (min_val >= max_val) return min_val;
			std::uniform_int_distribution<int> dist(min_val, max_val);
			return dist(seeded_rng);
		};

		const int final_object_count = std::max(1, object_range + next_rand(0, object_range));

		while (object_count < final_object_count) {
			int retries = 0;
			bool exit = false;

			while (retries < 5 && !exit) {
				const Position& offset = valid_offsets[static_cast<size_t>(next_rand(0, area - 1))];
				const int xpos = offset.x;
				const int ypos = offset.y;

				bool fail = false;
				const int total_chance = brush->getTotalChance(variation);
				const int comp_chance = brush->getCompositeChance(variation);
				if (total_chance > 0 && next_rand(1, total_chance) <= comp_chance) {
					// Composite
					const CompositeTileList& composites = brush->getComposite(variation, next_rand(1, std::max(1, comp_chance)));
					for (const auto& composite : composites) {
						Position pos = center_pos + composite.first + Position(xpos, ypos, 0);
						if (Tile* tile = doodad_buffer_map->getTile(pos)) {
							if (!tile->empty()) {
								fail = true;
								break;
							}
						}
					}
					if (fail) {
						++retries;
						break;
					}

					for (const auto& composite : composites) {
						Position pos = center_pos + composite.first + Position(xpos, ypos, 0);
						const auto& items = composite.second;
						Tile* tile = doodad_buffer_map->getTile(pos);
						if (!tile) {
							tile = doodad_buffer_map->createTile(pos.x, pos.y, pos.z);
						}
						for (const auto& item : items) {
							tile->addItem(item->deepCopy());
						}
					}
					exit = true;
				} else if (brush->hasSingleObjects(variation)) {
					Position pos = center_pos + Position(xpos, ypos, 0);
					Tile* tile = doodad_buffer_map->getTile(pos);
					if (tile) {
						if (!tile->empty()) {
							fail = true;
							break;
						}
					} else {
						tile = doodad_buffer_map->createTile(pos.x, pos.y, pos.z);
					}
					const int single_chance = brush->getSingleChance(variation);
					Item* item = brush->getRandomSingleItem(variation, next_rand(1, std::max(1, single_chance)));
					if (item) {
						tile->addItem(item->deepCopy());
					}
					exit = true;
				} else {
					exit = true;
				}
				if (fail) {
					++retries;
					break;
				}
			}
			++object_count;
		}
	} else {
		AddSingleTileVariant(*doodad_buffer_map, *brush, variation);
	}
}

void DoodadPreviewManager::FillSingleTile(BaseMap& buffer, DoodadBrush& brush, int variation) {
	buffer.clear();
	if (!brush.isEmpty(variation)) {
		AddSingleTileVariant(buffer, brush, variation);
	}
}

void DoodadPreviewManager::AddSingleTileVariant(BaseMap& buffer, DoodadBrush& brush, int variation) {
	if (brush.getTotalVariants() <= 0) {
		return;
	}
	const Position center_pos(0x8000, 0x8000, 0x8);
	const auto variant = brush.getVariant(variation);
	if (variant.composite) {
		for (const auto& composite : *variant.composite) {
			Position pos = center_pos + composite.first;
			const auto& items = composite.second;
			Tile* tile = buffer.createTile(pos.x, pos.y, pos.z);
			for (const auto& item : items) {
				if (item) {
					tile->addItem(item->deepCopy());
				}
			}
		}
	} else if (variant.single_item) {
		Tile* tile = buffer.createTile(center_pos.x, center_pos.y, center_pos.z);
		tile->addItem(variant.single_item->deepCopy());
	}
}
