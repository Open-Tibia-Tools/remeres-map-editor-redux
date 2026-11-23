//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_BRUSH_COLLECTION_POLICY_H_
#define RME_PALETTE_QUICK_TILESETS_BRUSH_COLLECTION_POLICY_H_

namespace QuickTilesets {

	// What a brush is, as far as Recent, Favorites and Quick Tilesets care.
	enum class BrushKind {
		Named, // ground, wall, doodad, table, carpet, ... found by name in g_brushes
		Raw,
		Creature,
		PaletteContext, // house, house exit, waypoint, town temple, zone: settings live in their palette
		Tool, // eraser, flag, door, optional border, spawn: picked from toolbars, not from sets
	};

	// Only brushes that can be stored as a reference and re-used on their own go into
	// Recent, Favorites and Quick Tilesets.
	[[nodiscard]] constexpr bool IsCollectable(BrushKind kind) noexcept {
		return kind == BrushKind::Named || kind == BrushKind::Raw || kind == BrushKind::Creature;
	}

} // namespace QuickTilesets

#endif
