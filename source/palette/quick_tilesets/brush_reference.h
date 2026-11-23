//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_BRUSH_REFERENCE_H_
#define RME_PALETTE_QUICK_TILESETS_BRUSH_REFERENCE_H_

#include "palette/quick_tilesets/brush_collection_policy.h"
#include "palette/quick_tilesets/quick_tileset_model.h"

#include <optional>
#include <span>
#include <vector>

class Brush;

// Maps live brushes to the string references stored in Quick Tilesets and back.
namespace QuickTilesets {

	[[nodiscard]] BrushKind ClassifyBrush(const Brush& brush);

	// Reference of a collectable brush, nullopt for palette-context and tool brushes.
	[[nodiscard]] std::optional<Item> MakeReference(const Brush& brush);

	// References of the collectable brushes in `brushes` (others are skipped).
	[[nodiscard]] std::vector<Item> MakeReferences(std::span<Brush* const> brushes);

	// The brush a reference names in the loaded client data, creating RAW and creature brushes on
	// demand; nullptr when the data does not have it (the reference is kept in the set).
	[[nodiscard]] Brush* Resolve(const Item& item);

} // namespace QuickTilesets

#endif
