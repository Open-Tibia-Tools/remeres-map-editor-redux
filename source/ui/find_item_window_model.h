#ifndef RME_FIND_ITEM_WINDOW_MODEL_H_
#define RME_FIND_ITEM_WINDOW_MODEL_H_

#include "item_definitions/core/item_definition_types.h"

#include <wx/gdicmn.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class Brush;
class CreatureBrush;
class RAWBrush;

enum class AdvancedFinderCatalogKind : uint8_t {
	Item = 0,
	Creature,
};

enum class AdvancedFinderDefaultAction : uint8_t {
	SelectItem = 0,
	SearchMap,
};

enum class AdvancedFinderTypeFilter : uint8_t {
	Depot = 0,
	Mailbox,
	TrashHolder,
	Container,
	Door,
	MagicField,
	Teleport,
	Bed,
	Key,
	Podium,
	Weapon,
	Ammo,
	Armor,
	Rune,
	Creature,
	Count,
};

enum class AdvancedFinderPropertyFilter : uint8_t {
	Unpassable = 0,
	Unmovable,
	BlockMissiles,
	BlockPathfinder,
	HasElevation,
	FloorChange,
	FullTile,
	Count,
};

enum class AdvancedFinderInteractionFilter : uint8_t {
	Readable = 0,
	Writeable,
	Pickupable,
	ForceUse,
	DistRead,
	Rotatable,
	Hangable,
	Count,
};

enum class AdvancedFinderVisualFilter : uint8_t {
	HasLight = 0,
	Animation,
	AlwaysTop,
	IgnoreLook,
	HasCharges,
	ClientCharges,
	Decays,
	HasSpeed,
	Count,
};

using AdvancedFinderFilterMask = uint32_t;

template <typename Enum>
constexpr AdvancedFinderFilterMask advancedFinderBit(Enum value) {
	return AdvancedFinderFilterMask { 1 } << static_cast<std::underlying_type_t<Enum>>(value);
}

struct AdvancedFinderQuery {
	std::string text;
	AdvancedFinderFilterMask type_mask = 0;
	AdvancedFinderFilterMask property_mask = 0;
	AdvancedFinderFilterMask interaction_mask = 0;
	AdvancedFinderFilterMask visual_mask = 0;

	[[nodiscard]] bool hasAnyFilter() const {
		return type_mask != 0 || property_mask != 0 || interaction_mask != 0 || visual_mask != 0;
	}
};

struct AdvancedFinderCatalogRow {
	AdvancedFinderCatalogKind kind = AdvancedFinderCatalogKind::Item;
	Brush* brush = nullptr;
	RAWBrush* raw_brush = nullptr;
	CreatureBrush* creature_brush = nullptr;
	ServerItemId server_id = 0;
	ClientItemId client_id = 0;
	std::string label;
	std::string lower_label;
	std::vector<std::string> name_tokens;
	std::vector<std::string> search_terms;
	AdvancedFinderFilterMask type_mask = 0;
	AdvancedFinderFilterMask property_mask = 0;
	AdvancedFinderFilterMask interaction_mask = 0;
	AdvancedFinderFilterMask visual_mask = 0;

	[[nodiscard]] bool isItem() const {
		return kind == AdvancedFinderCatalogKind::Item;
	}

	[[nodiscard]] bool isCreature() const {
		return kind == AdvancedFinderCatalogKind::Creature;
	}
};

struct AdvancedFinderSelectionKey {
	AdvancedFinderCatalogKind kind = AdvancedFinderCatalogKind::Item;
	ServerItemId server_id = 0;
	std::string creature_name;
};

struct AdvancedFinderPersistedState {
	AdvancedFinderQuery query;
	AdvancedFinderSelectionKey selection;
	wxPoint position = wxDefaultPosition;
	wxSize size = wxDefaultSize;
};

AdvancedFinderPersistedState LoadAdvancedFinderPersistedState();
void SaveAdvancedFinderPersistedState(const AdvancedFinderPersistedState& state);
std::vector<AdvancedFinderCatalogRow> BuildAdvancedFinderCatalog(bool include_creatures);
std::vector<size_t> FilterAdvancedFinderCatalog(const std::vector<AdvancedFinderCatalogRow>& catalog, const AdvancedFinderQuery& query);
AdvancedFinderSelectionKey MakeAdvancedFinderSelectionKey(const AdvancedFinderCatalogRow& row);
bool AdvancedFinderSelectionMatches(const AdvancedFinderCatalogRow& row, const AdvancedFinderSelectionKey& selection);
std::vector<Brush*> FilterBrushesWithAdvancedFinder(const std::vector<Brush*>& brushes, const std::string& queryText);

#endif

