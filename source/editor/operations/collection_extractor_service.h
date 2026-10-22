#ifndef RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_
#define RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_

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

class CollectionExtractorService {
public:
	/**
	 * Extracts unique Item IDs and composite/doodad/ground Brushes
	 * present within the current editor selection.
	 * Filters out RAWBrushes and EraserBrushes.
	 */
	static CollectionStagingContent ExtractFromSelection(Editor* editor);
};

} // namespace rme::collection

#endif // RME_EDITOR_OPERATIONS_COLLECTION_EXTRACTOR_SERVICE_H_
