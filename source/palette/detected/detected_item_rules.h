//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_DETECTED_DETECTED_ITEM_RULES_H_
#define RME_PALETTE_DETECTED_DETECTED_ITEM_RULES_H_

#include <cstdint>

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
};

// A ground border piece that no border and no brush in the materials XML refers to.
[[nodiscard]] bool IsUnregisteredBorder(const DetectedItemFacts& item);

#endif
