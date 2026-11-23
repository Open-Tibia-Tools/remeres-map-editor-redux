//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_DETECTED_DETECTED_ITEM_RULES_H_
#define RME_PALETTE_DETECTED_DETECTED_ITEM_RULES_H_

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// What the detected palettes need to know about one item, taken from the loaded DAT/OTB flags
// and from the brushes built out of the materials XML. Kept free of editor globals so the rules
// below can be tested in isolation.
struct DetectedItemFacts {
	uint32_t server_id = 0;
	uint32_t client_id = 0;
	bool meta = false;
	// DAT/OTB group ground.
	bool ground = false;
	// DAT "ground border" (always on bottom with top order 1): the layer RME borders live on.
	bool ground_border_piece = false;
	// Referenced by a border in borders.xml or by an inline border of a ground brush.
	bool registered_border = false;
	// Owned by a ground, wall, carpet, table, door, doodad or collection brush.
	bool brush_owned = false;
	// DAT "unpassable" (block solid).
	bool unpassable = false;
	// Owned by a ground brush, and whether that brush has any border rule (inner, outer,
	// optional or a specific border block).
	bool ground_brush_owned = false;
	bool ground_brush_has_border = false;
	// DAT "block missiles".
	bool block_missiles = false;
};

// A ground border piece that no border and no brush in the materials XML refers to.
[[nodiscard]] bool IsUnregisteredBorder(const DetectedItemFacts& item);

enum class GroundBorderState {
	NotAFloor,
	Bordered,
	// In a ground brush that has no border rule at all.
	BrushWithoutBorder,
	// In no ground brush.
	NoGroundBrush,
};

// Walkable ground items only: unpassable grounds (water, mountains) are NotAFloor.
[[nodiscard]] GroundBorderState ClassifyGroundBorder(const DetectedItemFacts& item);

enum class MountainPiece {
	None,
	// Unpassable, missile-blocking ground (the stock "mountain" brush uses 919).
	Ground,
	// Unpassable, missile-blocking ground border piece (mountain sides and corners).
	Border,
};

// A mountain piece that no brush or border uses yet.
[[nodiscard]] MountainPiece ClassifyUnregisteredMountain(const DetectedItemFacts& item);

struct MountainBrushItem {
	uint32_t server_id = 0;
	bool exists = false;
	bool ground = false;
};

// Why a ground brush whose name contains "mountain" cannot paint correctly, or an empty
// string when it is fine: it has no items, or an item id is missing from the item
// definitions, or an item is not a ground. Other brushes always return an empty string.
[[nodiscard]] std::string DescribeBrokenMountainBrush(std::string_view brush_name, std::span<const MountainBrushItem> items);

#endif
