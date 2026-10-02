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

    uint tier = (zoneFlags >> 21) & 7u;

    // Fixed point factors (x/256) from legacy TileColorCalculator
    // tier 0 (1 item):  192/256 = 0.7500000
    // tier 1 (2 items): 154/256 = 0.6015625
    // tier 2 (3 items): 123/256 = 0.48046875
    // tier 3 (4 items): 102/256 = 0.3984375
    // tier 4 (5+ items): 84/256 = 0.3281250
    const float factors[5] = float[5](
        0.75,
        0.6015625,
        0.48046875,
        0.3984375,
        0.328125
    );
    int idx = clamp(int(tier), 0, 4);
    float factor = factors[idx];

    outColor = vec4(factor, factor, 1.0, 1.0);
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HIGHLIGHT_ITEMS_SHADER_H_
