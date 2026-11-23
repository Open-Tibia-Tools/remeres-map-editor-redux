//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_DETECTED_DETECTED_PALETTES_H_
#define RME_PALETTE_DETECTED_DETECTED_PALETTES_H_

// Adds the detected palettes (items the materials XML does not cover yet) to the palette
// catalog of g_materials. They exist only at runtime: nothing is written to the data files
// unless the user saves a selection through the Collection tools. Call after the materials
// and the RAW brushes are loaded. Does nothing when Config::SHOW_DETECTED_PALETTES is off.
void AddDetectedPalettes();

#endif
