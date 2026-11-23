#ifndef RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_
#define RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_

#include "editor/operations/collection_save_plan.h"

#include <cstdint>
#include <vector>
#include <span>

class Editor;
class Brush;

namespace rme::collection {

struct CollectionStagingContent {
	std::vector<uint16_t> itemIds;
	std::vector<Brush*> brushes;

	void Merge(const CollectionStagingContent& other);
	void DiscardItem(uint16_t itemId);
	void DiscardBrush(Brush* brush);
	void Clear();
	[[nodiscard]] bool Empty() const {
		return itemIds.empty() && brushes.empty();
	}
};

// Items and brushes a tileset save writes, resolved from a CollectionSavePlan.
struct CollectionSaveSelection {
	std::vector<uint16_t> itemIds;
	std::vector<Brush*> brushes;
	size_t stagedItemCount = 0;
	size_t coveredItemCount = 0;
};

// Tileset group of a brush (see CollectionBrushCategory).
[[nodiscard]] CollectionBrushCategory ClassifyCollectionBrush(const Brush& brush);

class CollectionExtractorService {
public:
	/**
	 * Extracts unique Item IDs and composite/doodad/ground Brushes
	 * present within the current editor selection.
	 * Filters out RAWBrushes and EraserBrushes.
	 */
	static CollectionStagingContent ExtractFromSelection(Editor* editor);

	/**
	 * Builds the save plan of the staged content for the given mode.
	 * Covered item ids come from Brush::getRelatedItems() of the staged brushes.
	 * The staging content is not modified.
	 */
	static CollectionSaveSelection BuildSaveSelection(const CollectionStagingContent& staging, CollectionInclusionMode mode);
};

} // namespace rme::collection

#endif // RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_
