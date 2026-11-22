#ifndef RME_EDITOR_OPERATIONS_COLLECTION_SAVE_PLAN_H_
#define RME_EDITOR_OPERATIONS_COLLECTION_SAVE_PLAN_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rme::collection {

// Brush groups of a saved tileset. The enumerator order is the order in the tileset file.
enum class CollectionBrushCategory : uint8_t {
	Ground,
	Wall,
	Doodad,
	Carpet,
	Table,
	Door,
	Creature,
	House,
	Other
};

enum class CollectionInclusionMode : uint8_t {
	ItemsOnly,
	BrushesOnly,
	// Every staged item and every staged brush, without cross-filtering.
	ItemsAndBrushes,
	// Every staged brush plus only the staged items that no staged brush covers.
	BrushesAndUncoveredItems
};

// Maps the "Add to Tileset" inclusion checkboxes to a mode.
// onlyUncoveredItems only matters when both items and brushes are included.
// Returns std::nullopt when nothing is included.
[[nodiscard]] constexpr std::optional<CollectionInclusionMode> ResolveCollectionInclusionMode(
	bool includeItems,
	bool includeBrushes,
	bool onlyUncoveredItems
) {
	if (includeItems && includeBrushes) {
		return onlyUncoveredItems ? CollectionInclusionMode::BrushesAndUncoveredItems : CollectionInclusionMode::ItemsAndBrushes;
	}
	if (includeItems) {
		return CollectionInclusionMode::ItemsOnly;
	}
	if (includeBrushes) {
		return CollectionInclusionMode::BrushesOnly;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr bool IncludesItems(CollectionInclusionMode mode) {
	return mode != CollectionInclusionMode::BrushesOnly;
}

[[nodiscard]] constexpr bool IncludesBrushes(CollectionInclusionMode mode) {
	return mode != CollectionInclusionMode::ItemsOnly;
}

// Sort key of one brush entry in a saved tileset: category, numeric sort id, then name.
struct CollectionBrushSortKey {
	CollectionBrushCategory category = CollectionBrushCategory::Other;
	uint32_t id = 0;
	std::string_view name;
};

[[nodiscard]] constexpr char CollectionAsciiLower(char c) {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Three-way, ASCII case-insensitive comparison.
[[nodiscard]] constexpr int CompareCollectionNamesIgnoreCase(std::string_view a, std::string_view b) {
	const size_t common = std::min(a.size(), b.size());
	for (size_t i = 0; i < common; ++i) {
		const auto ca = static_cast<unsigned char>(CollectionAsciiLower(a[i]));
		const auto cb = static_cast<unsigned char>(CollectionAsciiLower(b[i]));
		if (ca != cb) {
			return ca < cb ? -1 : 1;
		}
	}
	if (a.size() == b.size()) {
		return 0;
	}
	return a.size() < b.size() ? -1 : 1;
}

[[nodiscard]] constexpr bool EqualsCollectionNameIgnoreCase(std::string_view a, std::string_view b) {
	return CompareCollectionNamesIgnoreCase(a, b) == 0;
}

// Strict weak order of brush entries: category, numeric sort id, case-insensitive name, then exact name.
[[nodiscard]] constexpr bool CollectionBrushLess(const CollectionBrushSortKey& a, const CollectionBrushSortKey& b) {
	if (a.category != b.category) {
		return a.category < b.category;
	}
	if (a.id != b.id) {
		return a.id < b.id;
	}
	if (const int cmp = CompareCollectionNamesIgnoreCase(a.name, b.name); cmp != 0) {
		return cmp < 0;
	}
	return a.name < b.name;
}

// A staged brush as seen by the save plan. Views must outlive the BuildCollectionSavePlan call.
struct CollectionBrushInfo {
	std::string_view name;
	// GetBrushSortID value (palette "Sort by ID": look/outfit id, brush id fallback).
	uint32_t id = 0;
	CollectionBrushCategory category = CollectionBrushCategory::Other;
	// Item ids the brush draws (Brush::getRelatedItems); duplicates are allowed.
	std::span<const uint16_t> coveredItemIds;
};

struct CollectionSavePlan {
	// Item ids to write: ascending, unique, without id 0.
	std::vector<uint16_t> itemIds;
	// Indices into the brush input to write, in tileset order (CollectionBrushLess).
	std::vector<size_t> brushIndices;
	// Unique non-zero staged item ids.
	size_t stagedItemCount = 0;
	// Staged item ids covered by at least one staged brush, whatever the mode.
	size_t coveredItemCount = 0;
};

// Decides which staged items and brushes a tileset save writes. Inputs are not modified.
[[nodiscard]] inline CollectionSavePlan BuildCollectionSavePlan(
	std::span<const uint16_t> stagedItemIds,
	std::span<const CollectionBrushInfo> stagedBrushes,
	CollectionInclusionMode mode
) {
	CollectionSavePlan plan;

	std::vector<uint16_t> items(stagedItemIds.begin(), stagedItemIds.end());
	std::erase(items, uint16_t { 0 });
	std::ranges::sort(items);
	items.erase(std::unique(items.begin(), items.end()), items.end());
	plan.stagedItemCount = items.size();

	std::vector<bool> covered(static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1, false);
	for (const CollectionBrushInfo& brush : stagedBrushes) {
		for (const uint16_t id : brush.coveredItemIds) {
			covered[id] = true;
		}
	}
	plan.coveredItemCount = static_cast<size_t>(std::ranges::count_if(items, [&covered](uint16_t id) { return covered[id]; }));

	if (IncludesItems(mode)) {
		if (mode == CollectionInclusionMode::BrushesAndUncoveredItems) {
			std::erase_if(items, [&covered](uint16_t id) { return covered[id]; });
		}
		plan.itemIds = std::move(items);
	}

	if (IncludesBrushes(mode)) {
		plan.brushIndices.resize(stagedBrushes.size());
		std::iota(plan.brushIndices.begin(), plan.brushIndices.end(), size_t { 0 });
		std::ranges::stable_sort(plan.brushIndices, [stagedBrushes](size_t a, size_t b) {
			const CollectionBrushInfo& lhs = stagedBrushes[a];
			const CollectionBrushInfo& rhs = stagedBrushes[b];
			return CollectionBrushLess({ lhs.category, lhs.id, lhs.name }, { rhs.category, rhs.id, rhs.name });
		});
	}

	return plan;
}

// Number of items and brushes a save writes, from the counts of the staged content.
// stagedItemCount and coveredItemCount follow the CollectionSavePlan definitions.
struct CollectionSaveCounts {
	size_t items = 0;
	size_t brushes = 0;

	[[nodiscard]] constexpr bool Empty() const {
		return items == 0 && brushes == 0;
	}
};

[[nodiscard]] constexpr CollectionSaveCounts ComputeCollectionSaveCounts(
	CollectionInclusionMode mode,
	size_t stagedItemCount,
	size_t coveredItemCount,
	size_t brushCount
) {
	CollectionSaveCounts counts;
	if (IncludesItems(mode)) {
		counts.items = stagedItemCount;
		if (mode == CollectionInclusionMode::BrushesAndUncoveredItems) {
			counts.items -= std::min(coveredItemCount, stagedItemCount);
		}
	}
	if (IncludesBrushes(mode)) {
		counts.brushes = brushCount;
	}
	return counts;
}

} // namespace rme::collection

#endif // RME_EDITOR_OPERATIONS_COLLECTION_SAVE_PLAN_H_
