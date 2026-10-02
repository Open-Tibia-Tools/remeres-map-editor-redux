#ifndef RME_RENDERING_SHADERS_HIGHLIGHT_ITEMS_SHADER_H_
#define RME_RENDERING_SHADERS_HIGHLIGHT_ITEMS_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing shader-driven item highlighting overlays.
 *
 * Tints ground underneath tiles with items based on item count,
 * replicating the legacy multiplicative color shift (reducing red and green)
 * fully on the GPU without triggering chunk cache invalidations.
 */
inline constexpr std::string_view HIGHLIGHT_ITEMS_SHADER_GLSL = R"(
uniform int uShowHighlightItems;

bool evaluateHighlightItems(uint zoneFlags, out vec4 outColor) {
    if (uShowHighlightItems == 0 || (zoneFlags & (1u << 20)) == 0u) {
        return false;
    }

    // Default 1-item highlight factor (192/256 = 0.75)
    outColor = vec4(0.75, 0.75, 1.0, 1.0);
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HIGHLIGHT_ITEMS_SHADER_H_
