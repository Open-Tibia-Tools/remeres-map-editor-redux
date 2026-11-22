#ifndef RME_IO_COLLECTION_TILESET_XML_H_
#define RME_IO_COLLECTION_TILESET_XML_H_

#include "editor/operations/collection_save_plan.h"
#include "ext/pugixml.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rme::collection {

// Category and id of a brush listed by name in a tileset file.
struct CollectionBrushLookup {
	CollectionBrushCategory category = CollectionBrushCategory::Other;
	uint32_t id = 0;
};

using CollectionBrushResolver = std::function<CollectionBrushLookup(std::string_view name)>;

namespace detail {

	// Inclusive server id range of an <item> node (id="..." or fromid/toid); std::nullopt for other nodes.
	[[nodiscard]] inline std::optional<std::pair<uint32_t, uint32_t>> TilesetItemRange(pugi::xml_node node) {
		if (!EqualsCollectionNameIgnoreCase(node.name(), "item")) {
			return std::nullopt;
		}
		if (pugi::xml_attribute id = node.attribute("id")) {
			return std::pair { id.as_uint(), id.as_uint() };
		}
		if (pugi::xml_attribute from = node.attribute("fromid")) {
			uint32_t fromId = from.as_uint();
			uint32_t toId = node.attribute("toid").as_uint(fromId);
			if (fromId > toId) {
				std::swap(fromId, toId);
			}
			return std::pair { fromId, toId };
		}
		return std::nullopt;
	}

} // namespace detail

// Appends <brush name> and <item id> nodes for every brush name and item id the tileset does not list yet.
// Existing nodes are left untouched; <item fromid toid> ranges count as listing their ids.
inline void AppendMissingTilesetEntries(pugi::xml_node root, std::span<const std::string> brushNames, std::span<const uint16_t> itemIds) {
	std::set<std::string, std::less<>> brushSet;
	std::set<uint16_t> itemSet;
	for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
		if (child.type() != pugi::node_element) {
			continue;
		}
		// Creature brushes may already be listed as <creature name>, which loads the same brush.
		if (EqualsCollectionNameIgnoreCase(child.name(), "brush") || EqualsCollectionNameIgnoreCase(child.name(), "creature")) {
			std::string name = child.attribute("name").as_string();
			if (!name.empty()) {
				brushSet.insert(std::move(name));
			}
		} else if (auto range = detail::TilesetItemRange(child)) {
			constexpr uint32_t maxItemId = std::numeric_limits<uint16_t>::max();
			for (uint32_t id = range->first; id <= range->second && id <= maxItemId; ++id) {
				if (id != 0) {
					itemSet.insert(static_cast<uint16_t>(id));
				}
			}
		}
	}

	for (const std::string& name : brushNames) {
		if (!name.empty() && brushSet.insert(name).second) {
			root.append_child("brush").append_attribute("name").set_value(name.c_str());
		}
	}

	for (const uint16_t id : itemIds) {
		if (id != 0 && itemSet.insert(id).second) {
			root.append_child("item").append_attribute("id").set_value(id);
		}
	}
}

// Reorders the children of a <tileset> node deterministically:
//   1. comments and unknown nodes, in their original relative order;
//   2. <brush> and <creature> nodes, by CollectionBrushLess (creature nodes are in the Creature group);
//   3. <item> nodes, ascending by first id, then by last id of their range.
// Nodes are deep-copied in the new order and the originals removed. pugixml reports only top-level copy
// failures, so deep-copy allocation failures inside children cannot be detected here.
// Returns false when a copy could not be appended; the original order is then left untouched.
[[nodiscard]] inline bool SortTilesetEntries(pugi::xml_node root, const CollectionBrushResolver& resolve) {
	enum class Group : uint8_t {
		Other,
		Brush,
		Item
	};

	struct Entry {
		pugi::xml_node node;
		Group group = Group::Other;
		CollectionBrushLookup brush;
		std::string_view name;
		std::pair<uint32_t, uint32_t> range;
	};

	std::vector<Entry> entries;
	for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
		Entry entry { .node = child };
		if (child.type() == pugi::node_element) {
			const bool isBrush = EqualsCollectionNameIgnoreCase(child.name(), "brush");
			const bool isCreature = EqualsCollectionNameIgnoreCase(child.name(), "creature");
			if (isBrush || isCreature) {
				entry.group = Group::Brush;
				entry.name = child.attribute("name").as_string();
				entry.brush = resolve(entry.name);
				if (isCreature) {
					entry.brush.category = CollectionBrushCategory::Creature;
				}
			} else if (auto range = detail::TilesetItemRange(child)) {
				entry.group = Group::Item;
				entry.range = *range;
			}
		}
		entries.push_back(entry);
	}

	std::ranges::stable_sort(entries, [](const Entry& a, const Entry& b) {
		if (a.group != b.group) {
			return a.group < b.group;
		}
		if (a.group == Group::Brush) {
			return CollectionBrushLess({ a.brush.category, a.brush.id, a.name }, { b.brush.category, b.brush.id, b.name });
		}
		if (a.group == Group::Item) {
			return a.range < b.range;
		}
		return false;
	});

	// The bundled pugixml has no append_move; the sort keys above view the originals, so copy everything before removing.
	std::vector<pugi::xml_node> copies;
	copies.reserve(entries.size());
	bool copiedAll = true;
	for (const Entry& entry : entries) {
		const pugi::xml_node copy = root.append_copy(entry.node);
		if (!copy) {
			copiedAll = false;
			break;
		}
		copies.push_back(copy);
	}
	if (!copiedAll) {
		// Allocation failure: drop the appended copies and keep the original order instead of losing entries.
		for (const pugi::xml_node copy : copies) {
			root.remove_child(copy);
		}
		return false;
	}
	for (const Entry& entry : entries) {
		root.remove_child(entry.node);
	}
	return true;
}

} // namespace rme::collection

#endif // RME_IO_COLLECTION_TILESET_XML_H_
