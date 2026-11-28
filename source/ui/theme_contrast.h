#ifndef RME_THEME_CONTRAST_H_
#define RME_THEME_CONTRAST_H_

#include <cmath>
#include <cstdint>
#include <span>

struct ThemeRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

namespace rme::ui::detail {

    [[nodiscard]] inline double LinearizeSrgbChannel(uint8_t channel) noexcept
    {
        const double srgb = static_cast<double>(channel) / 255.0;
        if (srgb <= 0.04045) {
            return srgb / 12.92;
        }
        return std::pow((srgb + 0.055) / 1.055, 2.4);
    }

} // namespace rme::ui::detail

[[nodiscard]] inline double ComputeRelativeLuminance(ThemeRgb color) noexcept
{
    const double r = rme::ui::detail::LinearizeSrgbChannel(color.r);
    const double g = rme::ui::detail::LinearizeSrgbChannel(color.g);
    const double b = rme::ui::detail::LinearizeSrgbChannel(color.b);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

[[nodiscard]] inline double ComputeWcagContrastRatio(ThemeRgb a, ThemeRgb b) noexcept
{
    const double lum_a = ComputeRelativeLuminance(a);
    const double lum_b = ComputeRelativeLuminance(b);
    const double lighter = (lum_a > lum_b) ? lum_a : lum_b;
    const double darker = (lum_a < lum_b) ? lum_a : lum_b;
    return (lighter + 0.05) / (darker + 0.05);
}

[[nodiscard]] inline ThemeRgb PickBestContrastCandidate(ThemeRgb background, std::span<const ThemeRgb> candidates) noexcept
{
    if (candidates.empty()) {
        return ThemeRgb {};
    }

    ThemeRgb best = candidates.front();
    double best_ratio = ComputeWcagContrastRatio(background, best);
    for (size_t i = 1; i < candidates.size(); ++i) {
        const double ratio = ComputeWcagContrastRatio(background, candidates[i]);
        if (ratio > best_ratio) {
            best_ratio = ratio;
            best = candidates[i];
        }
    }
    return best;
}

[[nodiscard]] inline ThemeRgb
PickAccessibleForeground(ThemeRgb background, ThemeRgb preferred, ThemeRgb fallback, double min_ratio = 4.5) noexcept
{
    if (ComputeWcagContrastRatio(background, preferred) >= min_ratio) {
        return preferred;
    }
    return fallback;
}

#endif
