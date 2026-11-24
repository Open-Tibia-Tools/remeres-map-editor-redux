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

	// How the wand collects a target. The mode is a persistent user choice (Selection Method
	// menu) and applies to every target kind, so the tool behaves predictably instead of
	// flooding grounds while scattering doodads.
	enum class SearchMode : uint8_t {
		Viewport, // every match in the visible rectangle, adjacent or not
		Neighbour, // only the matches connected to the click, diagonals included
	};

	// Maps a stored setting onto a mode; every unknown value falls back to Viewport.
	[[nodiscard]] constexpr SearchMode SanitizeSearchMode(int value) noexcept {
		return value == static_cast<int>(SearchMode::Neighbour) ? SearchMode::Neighbour : SearchMode::Viewport;
	}

	// Identity for items that no brush owns (raw items, decorations, identical-looking variants).
	// Items with the same definition name form one family; items without a definition name share
	// the empty name and form one family too.
	struct ItemFamily {
		std::string key;
		friend bool operator==(const ItemFamily&, const ItemFamily&) = default;
	};

	struct CreatureTarget {
		std::string name;
		friend bool operator==(const CreatureTarget&, const CreatureTarget&) = default;
	};

	using Target = std::variant<GroundBrush*, WallBrush*, CarpetBrush*, TableBrush*, DoodadBrush*, ItemFamily, CreatureTarget>;

	// The names that decide identity. `primary` is the brush name, the family key or the creature
	// name (also the status-bar text). `item` is the definition name of the item the user clicked:
	// brush names come from materials.xml while definition names come from the client data, so a
	// ground, a border or a wall variant only joins its siblings when the clicked item's own name
	// is compared too. The empty name is a name like any other, so nameless entities are each
	// other's siblings.
	struct TargetNames {
		std::string primary;
		std::string item;
	};

	// One tile the wand selected. `select_ground` is true only when the ground item itself belongs
	// to the target (patch tiles of a ground brush, ground-slot doodads, ground family matches).
	struct Entry {
		Position position;
		bool select_ground = false;
	};

	// Matched-tile count of the plan: what the wand considered part of the target (the ground
	// patch without its ring, every entry of the other kinds). The status bar reports the
	// changed-tile count instead (see MagicWand::Select); the tests pin this as the plan contract.
	struct Plan {
		std::vector<Entry> entries;
		size_t target_tiles = 0;
	};

	// Returns false to cancel the search. `total` is 0 when the search size is unknown.
	using Tick = std::function<bool(std::string_view phase, size_t done, size_t total)>;
	using TileLookup = std::function<const Tile*(const Position&)>;

	// The item's definition name verbatim; nameless items share the empty name as their key.
	[[nodiscard]] std::string ItemFamilyKey(const Item& item);
	// Target of the entity under the cursor plus the identity names it was resolved from.
	// `origin` is where the collection starts: the clicked tile, or the neighbour ground that
	// produced a clicked ring border. std::nullopt when the hit has nothing selectable (empty
	// tile, or a spawn marker: spawns are deliberately out of the wand's scope).
	struct ResolvedTarget {
		Target target;
		TargetNames names;
		Position origin;
	};

	[[nodiscard]] std::optional<ResolvedTarget> ResolveTarget(const WandHit& hit, const TileLookup& lookup);

	// True when the entity's definition name is one of the target's identity names. ASCII
	// case-insensitive; the empty name is a name like any other, so nameless entities match
	// each other.
	[[nodiscard]] bool NameMatches(const TargetNames& names, std::string_view entity_name);

	// Collects the visible tiles belonging to `target` under `mode`:
	//  - Viewport: every matching tile of the visible rectangle; a ground also takes the border
	//    pieces of its brush found there.
	//  - Neighbour: the eight-connected flood around the origin (diagonally touching tiles are
	//    neighbours, so a checkerboard of the same ground is one patch); a ground takes the patch
	//    and the border pieces on the patch and its ring.
	// std::nullopt when the callback cancels.
	[[nodiscard]] std::optional<Plan> BuildPlan(
		const Target& target,
		const TargetNames& names,
		SearchMode mode,
		const TileLookup& lookup,
		const Position& origin,
		const PositionRect& viewport,
		const Tick& tick
	);

	// Selects the entities of `target` on a tile copy that is about to be committed. Entities whose
	// definition name matches one of `names` are selected too, so a wall, a doodad and a raw item
	// that share a name belong to the same selection. Creatures only ever match creatures.
	void SelectMatching(Tile& tile, const Target& target, const TargetNames& names, bool select_ground);

	// True when both tiles select the same entities. deepCopy keeps item order, so slot by slot is exact.
	[[nodiscard]] bool SameSelection(const Tile& live, const Tile& copy);

} // namespace MagicWand

#endif
