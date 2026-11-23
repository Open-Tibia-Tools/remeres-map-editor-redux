//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_GROUND_FLATTEN_H_
#define RME_RENDERING_CORE_GROUND_FLATTEN_H_

#include "rendering/core/sprite_footprint.h"

#include <optional>

class GameSprite;
struct DrawingOptions;

namespace rme::rendering {

// Part of a ground sprite to draw a second time on its anchor tile under the
// "Flatten Multi-Tile Ground" display rule (see FlattenGroundPart), or nullopt.
// 1x1 sprites return before the footprint is built.
[[nodiscard]] std::optional<SpritePart> ResolveGroundFlattenPart(const DrawingOptions& options, bool is_ground, GameSprite& sprite);

} // namespace rme::rendering

#endif
