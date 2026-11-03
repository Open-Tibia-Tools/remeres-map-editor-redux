#include "editor/operations/search_query.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/map_region.h"
#include "map/spatial_hash_grid.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/items.h"
#include "game/srv_markers.h"
#include "brushes/creature/creature_brush.h"
#include "ui/gui.h"

#include <algorithm>
#include <format>
#include <future>
#include <ranges>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace {

SearchResultGroup& ItemGroup(std::unordered_map<uint32_t, SearchResultGroup>& group_map, const Item* item) {
	const uint16_t item_id = item->getID();
	auto& group = group_map[item_id];
	if (group.id == 0) {
		group.id = item_id;
		group.client_id = item->getClientID();
		group.name = std::string(item->getName());
		if (group.name.empty()) {
			group.name = std::format("Item #{}", item_id);
		}
		group.is_creature = false;
	}
	return group;
}

// An item whose server attribute has the filter's value, e.g. "Quest Chest: 64".
void ExtractServerNumber(
	Tile* tile,
	Item* item,
	const ServerNumberFilter& filter,
	std::unordered_map<uint32_t, SearchResultGroup>& group_map,
	const Container* parent_container
) {
	const auto matched = std::ranges::find_if(filter.attributes, [&](rme::srv::ItemAttribute attribute) {
		const int32_t* value = item->getIntegerAttribute(rme::srv::key(attribute));
		return value && *value == filter.value;
	});
	if (matched == filter.attributes.end()) {
		return;
	}

	SearchResultChildEntry child;
	child.position = tile->getPosition();
	child.formatted_pos = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
	child.kind = SearchResultKind::ServerNumber;
	child.item_id = item->getID();
	child.subtitle = std::format("{}: {}", rme::srv::info(*matched).label, filter.value);
	if (parent_container != nullptr) {
		child.subtitle += std::format(", inside {}", parent_container->getName().empty() ? "container" : parent_container->getName());
	}
	ItemGroup(group_map, item).children.push_back(std::move(child));
}

// The tiles whose moveuse.dat rules set or test the quest value.
std::optional<SearchResultGroup> ScriptGroup(const Map& map, const ServerNumberFilter& filter) {
	if (!filter.include_scripts || !map.server_markers) {
		return std::nullopt;
	}
	SearchResultGroup group;
	group.name = "Scripts (moveuse.dat)";
	for (const rme::srv::Marker& marker : map.server_markers->all()) {
		const auto matches = [&](const rme::srv::QuestReference& reference) { return reference.quest == filter.value; };
		const auto first = std::ranges::find_if(marker.quests, matches);
		if (first == marker.quests.end()) {
			continue;
		}
		const auto count = std::ranges::count_if(marker.quests, matches);
		SearchResultChildEntry child;
		child.position = marker.position;
		child.formatted_pos = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
		child.kind = SearchResultKind::ServerNumber;
		child.subtitle = count > 1 ? std::format("{} (+{} more rules)", first->text, count - 1) : first->text;
		group.children.push_back(std::move(child));
	}
	if (group.children.empty()) {
		return std::nullopt;
	}
	return group;
}

void ExtractItem(
	Tile* tile,
	Item* item,
	const SearchQuery& query,
	std::unordered_map<uint32_t, SearchResultGroup>& group_map,
	const Container* parent_container
) {
	if (!tile || !item) {
		return;
	}

	if (query.server_number) {
		ExtractServerNumber(tile, item, *query.server_number, group_map, parent_container);
		return;
	}

	const uint16_t item_id = item->getID();

	// Quick rejection if query specifies an ID or set of IDs
	if (query.specific_item_id > 0 && item_id != query.specific_item_id) {
		return;
	}
	if (!query.item_id_set.empty() && !query.item_id_set.contains(item_id)) {
		return;
	}

	const uint16_t uid = item->getUniqueID();
	const uint16_t aid = item->getActionID();
	const std::string_view text_view = item->getText();
	Position teleport_dest;
	std::vector<SearchContainedItem> container_items;
	std::string subtitle;

	if (parent_container != nullptr) {
		subtitle = std::format("Inside {}", parent_container->getName().empty() ? "container" : parent_container->getName());
	}

	const bool is_specific = (query.specific_item_id > 0) || (!query.item_id_set.empty());

	const auto* container = item->asContainer();
	if (container != nullptr) {
		for (const auto& inner : container->getVector()) {
			if (inner) {
				const int count = (inner->isStackable() || inner->isExtraCharged() || inner->isClientCharged()) ? inner->getCount() : 1;
				container_items.push_back(SearchContainedItem {
					.item_id = static_cast<uint16_t>(inner->getID()),
					.count = count,
				});
			}
		}

		// In general searches, skip empty containers with no UID/AID.
		// If the user explicitly searched for this container item ID, always include it.
		if (!is_specific && container_items.empty() && uid == 0 && aid == 0) {
			return;
		}
	}

	bool is_match = false;
	SearchResultKind kind = SearchResultKind::GenericItem;

	if (is_specific) {
		is_match = true;
		if (container != nullptr) {
			kind = SearchResultKind::Container;
		} else if (const auto* tp = item->asTeleport()) {
			kind = SearchResultKind::Teleport;
			teleport_dest = tp->getDestination();
		} else if (uid > 0) {
			kind = SearchResultKind::UniqueID;
		} else if (aid > 0) {
			kind = SearchResultKind::ActionID;
		} else if (!text_view.empty()) {
			kind = SearchResultKind::Writeable;
		} else {
			kind = SearchResultKind::GenericItem;
		}
	} else {
		// 1. Container (non-empty or with UID/AID)
		if (HasCategory(query.categories, SearchCategory::Container)) {
			if (container != nullptr) {
				is_match = true;
				kind = SearchResultKind::Container;
			}
		}

		// 2. Teleport (Skip dummy teleports with dest (0,0,0) and no UID / AID)
		if (!is_match && HasCategory(query.categories, SearchCategory::Teleport)) {
			if (const auto* tp = item->asTeleport()) {
				const Position dest = tp->getDestination();
				if (dest.x != 0 || dest.y != 0 || dest.z != 0 || uid > 0 || aid > 0) {
					is_match = true;
					kind = SearchResultKind::Teleport;
					teleport_dest = dest;
				}
			}
		}

		// 3. UniqueID
		if (!is_match && HasCategory(query.categories, SearchCategory::UniqueID) && uid > 0) {
			is_match = true;
			kind = SearchResultKind::UniqueID;
		}

		// 4. ActionID
		if (!is_match && HasCategory(query.categories, SearchCategory::ActionID) && aid > 0) {
			is_match = true;
			kind = SearchResultKind::ActionID;
		}

		// 5. Writeable
		if (!is_match && HasCategory(query.categories, SearchCategory::Writeable) && !text_view.empty()) {
			is_match = true;
			kind = SearchResultKind::Writeable;
		}

		// 6. Pickupable
		if (!is_match && HasCategory(query.categories, SearchCategory::Pickupable) && item->isPickupable()) {
			is_match = true;
			kind = SearchResultKind::Pickupable;
			if (subtitle.empty()) {
				subtitle = "Pickupable";
			}
		}
	}

	if (!is_match) {
		return;
	}

	auto& group = ItemGroup(group_map, item);

	SearchResultChildEntry child;
	child.position = tile->getPosition();
	child.formatted_pos = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
	child.kind = kind;
	child.item_id = item_id;
	child.unique_id = uid;
	child.action_id = aid;
	child.text = std::string(text_view);
	child.teleport_destination = teleport_dest;
	child.container_items = std::move(container_items);
	child.subtitle = std::move(subtitle);
	child.is_creature = false;
	group.children.push_back(std::move(child));
}

void ExtractCreature(
	Tile* tile,
	Creature* creature,
	const SearchQuery& query,
	std::unordered_map<std::string, SearchResultGroup>& creature_groups
) {
	if (!tile || !creature) {
		return;
	}

	const Brush* brush = static_cast<const Brush*>(creature->getBrush());
	if (query.specific_creature_brush != nullptr && brush != query.specific_creature_brush) {
		return;
	}
	if (!query.creature_brushes.empty()) {
		if (std::ranges::find(query.creature_brushes, brush) == query.creature_brushes.end()) {
			return;
		}
	}

	const std::string name = creature->getName().empty() ? (brush ? brush->getName() : "Unknown Creature") : creature->getName();
	auto& group = creature_groups[name];
	if (group.name.empty()) {
		group.name = name;
		group.brush = brush;
		group.is_creature = true;
		group.id = creature->getLookType().lookType;
	}

	SearchResultChildEntry child;
	child.position = tile->getPosition();
	child.formatted_pos = std::format("Position: [{}, {}, {}]", child.position.x, child.position.y, child.position.z);
	child.kind = SearchResultKind::Creature;
	child.item_id = creature->getLookType().lookType;
	child.brush = brush;
	child.is_creature = true;
	child.text = name;

	const std::string dir_name = Creature::DirID2Name(creature->getDirection());
	child.subtitle = std::format("{} | Spawn: {}s | Dir: {}",
		creature->isNpc() ? "NPC" : "Monster",
		creature->getSpawnTime(),
		dir_name.empty() ? "None" : dir_name);

	group.children.push_back(std::move(child));
}

} // namespace

std::vector<SearchResultGroup> SearchEngine::Execute(Map& map, const SearchQuery& query) {
	const std::string bar_label = query.query_title.empty()
		? (query.on_selection ? "Searching on selected area..." : "Searching on map...")
		: query.query_title;

	if (query.progress_callback) {
		query.progress_callback(0.0f, bar_label);
	} else {
		g_gui.CreateLoadBar(bar_label);
	}

	const bool has_creature_filter = (query.specific_creature_brush != nullptr) || (!query.creature_brushes.empty());
	const bool search_creatures = HasCategory(query.categories, SearchCategory::Creature) || has_creature_filter;

	const bool has_item_filter = (query.specific_item_id > 0) || (!query.item_id_set.empty());
	const bool search_items = (query.categories != SearchCategory::None && query.categories != SearchCategory::Creature) || has_item_filter || query.server_number.has_value();

	auto cells = map.getGrid().getSortedCells();
	if (cells.empty()) {
		if (query.progress_callback) {
			query.progress_callback(1.0f, "");
		} else {
			g_gui.DestroyLoadBar();
		}
		return {};
	}

	struct WorkerResult {
		std::unordered_map<uint32_t, SearchResultGroup> item_groups;
		std::unordered_map<std::string, SearchResultGroup> creature_groups;
	};

	const size_t total_cells = cells.size();
	const unsigned int hardware_threads = std::thread::hardware_concurrency();
	const unsigned int num_threads = std::max(1u, std::min(hardware_threads > 0 ? hardware_threads : 4u, static_cast<unsigned int>(total_cells)));
	const size_t chunk_size = (total_cells + num_threads - 1) / num_threads;

	auto process_cell_range = [&](size_t start_idx, size_t end_idx) -> WorkerResult {
		WorkerResult local;
		std::vector<Container*> container_buf;
		container_buf.reserve(64);

		for (size_t c = start_idx; c < end_idx; ++c) {
			SpatialHashGrid::GridCell* cell = cells[c].cell;
			if (!cell) {
				continue;
			}

			for (const auto& node_ptr : cell->nodes) {
				MapNode* node = node_ptr.get();
				if (!node) {
					continue;
				}

				for (int z = 0; z <= MAP_MAX_LAYER; ++z) {
					if (!node->hasFloor(z)) {
						continue;
					}

					Floor* floor = node->getFloor(z);
					if (!floor) {
						continue;
					}

					for (TileLocation& loc : floor->locs) {
						Tile* tile = loc.get();
						if (!tile) {
							continue;
						}
						if (query.on_selection && !tile->isSelected()) {
							continue;
						}

						// 1. Creatures (Monsters & NPCs)
						if (search_creatures && tile->creature) {
							ExtractCreature(tile, tile->creature.get(), query, local.creature_groups);
						}

						// 2. Items (Ground, tile items, and container contents)
						if (search_items) {
							if (tile->ground) {
								ExtractItem(tile, tile->ground.get(), query, local.item_groups, nullptr);
							}

							for (const auto& item : tile->items) {
								ExtractItem(tile, item.get(), query, local.item_groups, nullptr);

								// Search inside containers
								if (Container* c_item = item->asContainer()) {
									container_buf.clear();
									container_buf.push_back(c_item);
									size_t buf_idx = 0;
									while (buf_idx < container_buf.size()) {
										Container* parent_c = container_buf[buf_idx++];
										for (const auto& inner : parent_c->getVector()) {
											if (inner) {
												ExtractItem(tile, inner.get(), query, local.item_groups, parent_c);
												if (Container* inner_c = inner->asContainer()) {
													container_buf.push_back(inner_c);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		return local;
	};

	std::vector<std::future<WorkerResult>> futures;
	futures.reserve(num_threads);

	for (unsigned int t = 0; t < num_threads; ++t) {
		const size_t start_idx = t * chunk_size;
		const size_t end_idx = std::min(start_idx + chunk_size, total_cells);
		if (start_idx >= end_idx) {
			break;
		}

		futures.push_back(std::async(std::launch::async, [=, &process_cell_range]() {
			return process_cell_range(start_idx, end_idx);
		}));
	}

	std::unordered_map<uint32_t, SearchResultGroup> master_item_groups;
	std::unordered_map<std::string, SearchResultGroup> master_creature_groups;

	for (auto& fut : futures) {
		WorkerResult worker = fut.get();
		for (auto& [id, g] : worker.item_groups) {
			auto& target = master_item_groups[id];
			if (target.id == 0) {
				target.id = g.id;
				target.client_id = g.client_id;
				target.name = std::move(g.name);
				target.is_creature = false;
			}
			target.children.insert(
				target.children.end(),
				std::make_move_iterator(g.children.begin()),
				std::make_move_iterator(g.children.end())
			);
		}
		for (auto& [name, g] : worker.creature_groups) {
			auto& target = master_creature_groups[name];
			if (target.name.empty()) {
				target.name = name;
				target.brush = g.brush;
				target.is_creature = true;
				target.id = g.id;
			}
			target.children.insert(
				target.children.end(),
				std::make_move_iterator(g.children.begin()),
				std::make_move_iterator(g.children.end())
			);
		}
	}

	if (query.progress_callback) {
		query.progress_callback(1.0f, "");
	} else {
		g_gui.DestroyLoadBar();
	}

	// Merge all groups into a single list
	std::vector<SearchResultGroup> result;
	result.reserve(master_item_groups.size() + master_creature_groups.size());

	auto append_and_normalize = [&](auto& map) {
		for (auto& [key, g] : map) {
			g.id_str = std::to_string(g.id);
			g.lowercase_name.reserve(g.name.size());
			for (char ch : g.name) {
				g.lowercase_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
			}
			result.push_back(std::move(g));
		}
	};

	append_and_normalize(master_item_groups);
	append_and_normalize(master_creature_groups);
	if (query.server_number && !query.on_selection) {
		if (auto scripts = ScriptGroup(map, *query.server_number)) {
			std::unordered_map<uint32_t, SearchResultGroup> script_groups { { 0, std::move(*scripts) } };
			append_and_normalize(script_groups);
		}
	}

	// Default sort: highest match count first, then alphabetical name
	std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
		if (a.children.size() != b.children.size()) {
			return a.children.size() > b.children.size();
		}
		return a.name < b.name;
	});

	return result;
}
