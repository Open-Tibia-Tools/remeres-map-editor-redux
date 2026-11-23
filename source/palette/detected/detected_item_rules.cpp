//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/detected/detected_item_rules.h"

bool IsUnregisteredBorder(const DetectedItemFacts& item) {
	return !item.meta && !item.ground && item.ground_border_piece && !item.registered_border && !item.brush_owned;
}
