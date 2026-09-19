//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"
#include "brushes/managers/doodad_preview_manager.h"
#include "brushes/brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "game/sprites.h"
#include "map/map.h"
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

void DoodadPreviewManager::FillBuffer(
	DoodadBrush* brush,
	int variation,
	const BrushFootprint& footprint,
	bool useCustomThickness,
	float customThicknessMod
) {
	if (!brush) {
		return;
	}

	doodad_buffer_map->clear();

	if (brush->isEmpty(variation)) {
		return;
	}

	std::vector<Position> valid_offsets;
	for (int y = footprint.min_offset_y; y <= footprint.max_offset_y; ++y) {
		for (int x = footprint.min_offset_x; x <= footprint.max_offset_x; ++x) {
			if (footprint.containsOffset(x, y)) {
				valid_offsets.emplace_back(x, y, 0);
			}
		}
	}

	const int area = std::max(1, static_cast<int>(valid_offsets.size()));
	int object_count = 0;
	const int object_range = (useCustomThickness ? static_cast<int>(area * customThicknessMod) : brush->getThickness() * area / std::max(1, brush->getThicknessCeiling()));
	const int final_object_count = std::max(1, object_range + random(object_range));

	Position center_pos(0x8000, 0x8000, 0x8);

	if (!footprint.isSingleTile() && !brush->oneSizeFitsAll()) {
		while (object_count < final_object_count) {
			int retries = 0;
			bool exit = false;

			// Try to place objects 5 times
			while (retries < 5 && !exit) {

				const Position& offset = valid_offsets[static_cast<size_t>(random(0, area - 1))];
				const int xpos = offset.x;
				const int ypos = offset.y;

				// Decide whether the zone should have a composite or several single objects.
				bool fail = false;
				if (random(brush->getTotalChance(variation)) <= brush->getCompositeChance(variation)) {
					// Composite
					const CompositeTileList& composites = brush->getComposite(variation);

					// Figure out if the placement is valid
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

					// Transfer items to the stack
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
					int var = variation;
					brush->draw(doodad_buffer_map.get(), tile, &var);
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
		if (brush->hasCompositeObjects(variation) && random(brush->getTotalChance(variation)) <= brush->getCompositeChance(variation)) {
			// Composite
			const CompositeTileList& composites = brush->getComposite(variation);

			// All placement is valid...

			// Transfer items to the buffer
			for (const auto& composite : composites) {
				Position pos = center_pos + composite.first;
				const auto& items = composite.second;
				Tile* tile = doodad_buffer_map->createTile(pos.x, pos.y, pos.z);

				for (const auto& item : items) {
					tile->addItem(item->deepCopy());
				}
			}
		} else if (brush->hasSingleObjects(variation)) {
			Tile* tile = doodad_buffer_map->createTile(center_pos.x, center_pos.y, center_pos.z);
			int var = variation;
			brush->draw(doodad_buffer_map.get(), tile, &var);
		}
	}
}
