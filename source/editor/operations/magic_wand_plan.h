//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_MAGIC_WAND_PLAN_H_
#define RME_EDITOR_OPERATIONS_MAGIC_WAND_PLAN_H_

#include "map/position.h"
#include "map/position_rect.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class CarpetBrush;
class Creature;
class DoodadBrush;
class GroundBrush;
class Item;
class TableBrush;
class Tile;
class WallBrush;

// Pure magic wand decision layer: it resolves the entity under the cursor, then collects every
// visible position that belongs to it. No wxWidgets, no progress dialog, no mutation of the map:
// the caller supplies the tile lookup, the visible rectangle and the progress callback.
namespace MagicWand {

	enum class HitKind : uint8_t {
		None,
		Ground,
		Item,
		Creature,
	};

	// What SelectionPicker hit under the cursor. `tile` is the picked tile, which can differ from
	// the clicked one under pixel-perfect selection.
	struct WandHit {
		HitKind kind = HitKind::None;
		Tile* tile = nullptr;
		Item* item = nullptr;
		Creature* creature = nullptr;
	};

	// Identity for items that no brush owns (raw items, decorations, identical-looking variants).
	// Items with the same definition name form one family; nameless items fall back to their id.
	struct ItemFamily {
		std::string key;
		friend bool operator==(const ItemFamily&, const ItemFamily&) = default;
	};

	struct CreatureTarget {
		std::string name;
		friend bool operator==(const CreatureTarget&, const CreatureTarget&) = default;
	};

	using Target = std::variant<GroundBrush*, WallBrush*, CarpetBrush*, TableBrush*, DoodadBrush*, ItemFamily, CreatureTarget>;

	// One tile the wand selected. `select_ground` is true only when the ground item itself belongs
	// to the target (patch tiles of a ground brush, ground-slot doodads, ground family matches).
	struct Entry {
		Position position;
		bool select_ground = false;
	};

	struct Plan {
		std::vector<Entry> entries;
		size_t target_tiles = 0;
	};

	// Returns false to cancel the search. `total` is 0 when the search size is unknown.
	using Tick = std::function<bool(std::string_view phase, size_t done, size_t total)>;
	using TileLookup = std::function<const Tile*(const Position&)>;

	// Definition name, or "#<id>" when the item has no name (never group nameless items together).
	[[nodiscard]] std::string ItemFamilyKey(const Item& item);

	// Target of the entity under the cursor. std::nullopt when the hit has nothing selectable
	// (empty tile, or a spawn marker: spawns are deliberately out of the wand's scope).
	[[nodiscard]] std::optional<Target> ResolveTarget(const WandHit& hit);

	// Collects every visible tile belonging to `target`. Connected kinds flood by brush ownership
	// or by shared definition name; item families, doodads and creatures scan the whole viewport.
	// std::nullopt when the callback cancels.
	[[nodiscard]] std::optional<Plan> BuildPlan(
		const Target& target,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick
	);

	// Selects the entities of `target` on a tile copy that is about to be committed. Entities whose
	// definition name equals `target_name` are selected too, so a wall, a doodad and a raw item that
	// share a name belong to the same selection. Pass `DescribeTarget(target)`.
	void SelectMatching(Tile& tile, const Target& target, bool select_ground, std::string_view target_name);

	// True when both tiles select the same entities. deepCopy keeps item order, so slot by slot is exact.
	[[nodiscard]] bool SameSelection(const Tile& live, const Tile& copy);

	// Brush, family or creature name for the status bar.
	[[nodiscard]] std::string DescribeTarget(const Target& target);

} // namespace MagicWand

#endif
