//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/ground_flatten.h"

#include "rendering/core/drawing_options.h"
#include "rendering/core/game_sprite.h"

namespace rme::rendering {

std::optional<SpritePart> ResolveGroundFlattenPart(const DrawingOptions& options, bool is_ground, GameSprite& sprite) {
	if (!options.flatten_multi_tile_ground || !is_ground || (sprite.width <= 1 && sprite.height <= 1)) {
		return std::nullopt;
	}
	return FlattenGroundPart(is_ground, sprite.getPartFootprint());
}

} // namespace rme::rendering
