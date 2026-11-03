#ifndef RME_EDITOR_OPERATIONS_SEARCH_QUERY_H_
#define RME_EDITOR_OPERATIONS_SEARCH_QUERY_H_

#include "map/position.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

class Map;
class Tile;
class Item;
class Creature;
class Brush;

enum class SearchCategory : uint32_t {
	None        = 0,
	UniqueID    = 1 << 0,
	ActionID    = 1 << 1,
	Container   = 1 << 2,
	Writeable   = 1 << 3,
	Teleport    = 1 << 4,
	Creature    = 1 << 5, // Monsters & NPCs
	Pickupable  = 1 << 6,
	Everything  = UniqueID | ActionID | Container | Writeable | Teleport
};

inline SearchCategory operator|(SearchCategory a, SearchCategory b) {
	return static_cast<SearchCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SearchCategory operator&(SearchCategory a, SearchCategory b) {
	return static_cast<SearchCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasCategory(SearchCategory mask, SearchCategory cat) {
	return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(cat)) != 0;
}

enum class SearchResultKind : uint8_t {
	GenericItem,
	UniqueID,
	ActionID,
	Container,
	Writeable,
	Teleport,
	Creature,
	Pickupable
};

struct SearchContainedItem {
	uint16_t item_id = 0;
	int count = 1;
};

struct SearchResultChildEntry {
	Position position;
	std::string formatted_pos;
	SearchResultKind kind = SearchResultKind::GenericItem;
	uint16_t item_id = 0;
	uint16_t unique_id = 0;
	uint16_t action_id = 0;
	std::string text;
	Position teleport_destination;
	std::vector<SearchContainedItem> container_items;
	std::string subtitle;
	const Brush* brush = nullptr; // For creature brush rendering if creature
	bool is_creature = false;
};

struct SearchResultGroup {
	uint32_t id = 0;
	uint16_t client_id = 0;
	std::string name;
	std::string lowercase_name;
	std::string id_str;
	const Brush* brush = nullptr;
	bool is_creature = false;
	bool expanded = false;
	std::vector<SearchResultChildEntry> children;
};

struct SearchQuery {
	SearchCategory categories = SearchCategory::None;
	bool on_selection = false;
	uint16_t specific_item_id = 0;
	std::vector<uint16_t> item_ids;
	std::unordered_set<uint16_t> item_id_set;
	const Brush* specific_creature_brush = nullptr;
	std::vector<const Brush*> creature_brushes;
	std::string filter_text;
	std::string query_title;
	std::function<void(float, std::string_view)> progress_callback = nullptr;
};

class SearchEngine {
public:
	static std::vector<SearchResultGroup> Execute(Map& map, const SearchQuery& query);
};

#endif // RME_EDITOR_OPERATIONS_SEARCH_QUERY_H_
