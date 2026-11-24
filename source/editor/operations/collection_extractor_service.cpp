#include "editor/operations/collection_extractor_service.h"

#include "editor/editor.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/creature.h"
#include "game/creatures.h"
#include "game/spawn.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/managers/brush_manager.h"
#include "game/materials.h"
#include "ui/replace_tool/map_item_collector.h"

#include <algorithm>
#include <string>
#include <utility>

namespace rme::collection {

void CollectionStagingContent::Merge(const CollectionStagingContent& other) {
	// Merge item IDs
	itemIds.insert(itemIds.end(), other.itemIds.begin(), other.itemIds.end());
	std::ranges::sort(itemIds);
	itemIds.erase(std::unique(itemIds.begin(), itemIds.end()), itemIds.end());

	// Merge brushes
	brushes.insert(brushes.end(), other.brushes.begin(), other.brushes.end());
	std::ranges::sort(brushes, [](Brush* a, Brush* b) {
		if (!a || !b) {
			return a < b;
		}
		if (a->getName() != b->getName()) {
			return a->getName() < b->getName();
		}
		return a->getID() < b->getID();
	});
	auto [bfirst, blast] = std::ranges::unique(brushes, [](Brush* a, Brush* b) {
		if (a == b) return true;
		if (!a || !b) return false;
		return a->getName() == b->getName();
	});
	brushes.erase(bfirst, blast);
}

void CollectionStagingContent::DiscardItem(uint16_t itemId) {
	std::erase(itemIds, itemId);
}

void CollectionStagingContent::DiscardBrush(Brush* brush) {
	std::erase_if(brushes, [brush](Brush* b) {
		if (b == brush) return true;
		if (b && brush && b->getName() == brush->getName()) return true;
		return false;
	});
}

void CollectionStagingContent::Clear() {
	itemIds.clear();
	brushes.clear();
}

CollectionBrushCategory ClassifyCollectionBrush(const Brush& brush) {
	if (brush.is<GroundBrush>()) {
		return CollectionBrushCategory::Ground;
	}
	if (brush.is<WallBrush>()) {
		return CollectionBrushCategory::Wall;
	}
	if (brush.is<DoodadBrush>()) {
		return CollectionBrushCategory::Doodad;
	}
	if (brush.is<CarpetBrush>()) {
		return CollectionBrushCategory::Carpet;
	}
	if (brush.is<TableBrush>()) {
		return CollectionBrushCategory::Table;
	}
	if (brush.is<DoorBrush>()) {
		return CollectionBrushCategory::Door;
	}
	if (brush.is<CreatureBrush>()) {
		return CollectionBrushCategory::Creature;
	}
	if (brush.is<HouseBrush>()) {
		return CollectionBrushCategory::House;
	}
	return CollectionBrushCategory::Other;
}

CollectionSaveSelection CollectionExtractorService::BuildSaveSelection(
	const CollectionStagingContent& staging,
	CollectionInclusionMode mode
) {
	std::vector<Brush*> brushes;
	brushes.reserve(staging.brushes.size());
	for (Brush* brush : staging.brushes) {
		if (brush) {
			brushes.push_back(brush);
		}
	}

	std::vector<std::string> names;
	std::vector<std::vector<uint16_t>> related(brushes.size());
	names.reserve(brushes.size());
	for (size_t i = 0; i < brushes.size(); ++i) {
		names.push_back(brushes[i]->getName());
		brushes[i]->getRelatedItems(related[i]);
	}

	std::vector<CollectionBrushInfo> infos;
	infos.reserve(brushes.size());
	for (size_t i = 0; i < brushes.size(); ++i) {
		infos.push_back({
			.name = names[i],
			.id = GetBrushSortID(brushes[i]),
			.category = ClassifyCollectionBrush(*brushes[i]),
			.coveredItemIds = related[i],
		});
	}

	CollectionSavePlan plan = BuildCollectionSavePlan(staging.itemIds, infos, mode);

	CollectionSaveSelection selection;
	selection.itemIds = std::move(plan.itemIds);
	selection.brushes.reserve(plan.brushIndices.size());
	for (const size_t index : plan.brushIndices) {
		selection.brushes.push_back(brushes[index]);
	}
	selection.stagedItemCount = plan.stagedItemCount;
	selection.coveredItemCount = plan.coveredItemCount;
	return selection;
}

CollectionStagingContent CollectionExtractorService::ExtractFromSelection(Editor* editor) {
	CollectionStagingContent content;
	if (!editor) {
		return content;
	}

	// 1. Unique item IDs
	content.itemIds = rme::replace_tool::MapItemCollector::CollectSelectionItemIds(editor);
	std::ranges::sort(content.itemIds);

	// 2. Composite brushes from the selected entities (same rule as the item IDs above, so staged
	// IDs and staged brushes cannot disagree).
	std::vector<Brush*> candidates;
	for (Tile* tile : editor->selection.getTiles()) {
		if (!tile) {
			continue;
		}

		if (tile->ground && tile->ground->isSelected()) {
			if (GroundBrush* gb = tile->getGroundBrush()) {
				candidates.push_back(gb);
			} else if (Brush* b = tile->ground->getBrush()) {
				candidates.push_back(b);
			}
		}

		for (const auto& item : tile->items) {
			if (!item || !item->isSelected()) {
				continue;
			}
			if (Brush* b = item->getBrush()) {
				candidates.push_back(b);
			}
			if (Brush* db = item->getDoodadBrush()) {
				candidates.push_back(db);
			}
		}

		if (tile->creature && tile->creature->isSelected()) {
			CreatureBrush* cb = tile->creature->getBrush();
			if (!cb) {
				if (CreatureType* ct = g_creatures[tile->creature->getName()]) {
					cb = g_materials.ensureCreatureBrush(ct);
				}
			}
			if (cb) {
				candidates.push_back(cb);
			}
			for (const auto& alt : tile->creature->getAlternatives()) {
				if (CreatureType* altType = g_creatures[alt.name]) {
					if (Brush* b = g_materials.ensureCreatureBrush(altType)) {
						candidates.push_back(b);
					}
				}
			}
		}

	}

	for (Brush* b : candidates) {
		if (!b) {
			continue;
		}
		if (b->getName().empty()) {
			continue;
		}
		// Filter out raw, eraser, spawn, and technical/meta brushes
		auto to_lower_str = [](std::string_view s) {
			std::string result;
			result.reserve(s.size());
			for (char c : s) {
				result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			return result;
		};
		std::string lowerName = to_lower_str(b->getName());
		if (b->is<RAWBrush>() || b->is<EraserBrush>() || b->is<SpawnBrush>() ||
			b == g_brush_manager.spawn_brush || b == g_brush_manager.eraser ||
			lowerName == "spawn" || lowerName == "spawn brush") {
			continue;
		}
		content.brushes.push_back(b);
	}

	std::ranges::sort(content.brushes, [](Brush* a, Brush* b) {
		if (a->getName() != b->getName()) {
			return a->getName() < b->getName();
		}
		return a->getID() < b->getID();
	});

	auto [cfirst, clast] = std::ranges::unique(content.brushes, [](Brush* a, Brush* b) {
		if (a == b) return true;
		if (!a || !b) return false;
		return a->getName() == b->getName();
	});
	content.brushes.erase(cfirst, clast);

	return content;
}

} // namespace rme::collection
