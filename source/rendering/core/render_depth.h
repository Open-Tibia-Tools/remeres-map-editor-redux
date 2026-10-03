//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_RENDER_DEPTH_H_
#define RME_RENDERING_CORE_RENDER_DEPTH_H_

#include <algorithm>
#include <cstdint>

namespace rme::rendering {

enum class RenderSublayer : uint8_t {
	Ground = 0,
	Border = 1,
	GroundOverlay = 2,
	BottomItem = 3,
	CommonItem = 4,
	DynamicEntity = 8,
	TopItem = 10,
	Overlay = 12
};

/**
 * @brief Computes monotonic normalized depth [0.0, 1.0] for 2.5D isometric rendering.
 *
 * In 2.5D orthographic projection, tiles with larger diagonal coordinate (map_x + map_y)
 * are closer to the camera (South-East in front of North-West).
 *
 * Closer fragments are mapped to strictly SMALLER depth values in [0.0, 1.0], matching
 * standard OpenGL depth testing with glDepthFunc(GL_LEQUAL).
 *
 * Max map coordinate diagonal supported: 131072 (65536 x 65536 tiles).
 * 16 discrete sublayer steps per tile fit easily into a 24-bit depth buffer (2,097,152
 * values out of 16,777,216), guaranteeing zero Z-fighting across all elements.
 */
inline constexpr float calculateTileDepth(int map_x, int map_y, RenderSublayer sublayer, int elevation_step = 0) noexcept {
	constexpr float kMaxIndex = 2097152.0f; // 131072 * 16
	const int d = std::max(0, map_x + map_y);
	int sub = static_cast<int>(sublayer);
	if (sublayer == RenderSublayer::CommonItem) {
		sub += std::clamp(elevation_step, 0, 3);
	}
	const int index = std::clamp(d * 16 + sub, 0, static_cast<int>(kMaxIndex));
	return 1.0f - (static_cast<float>(index) + 1.0f) / (kMaxIndex + 2.0f);
}

} // namespace rme::rendering

#endif // RME_RENDERING_CORE_RENDER_DEPTH_H_
