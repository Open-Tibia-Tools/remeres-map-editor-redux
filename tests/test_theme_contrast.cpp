#include "ui/theme_contrast.h"

#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {
    constexpr ThemeRgb kDarkText {230, 230, 230};
    constexpr ThemeRgb kDarkBg {30, 30, 30};
    constexpr ThemeRgb kLightText {0, 0, 0};
    constexpr ThemeRgb kLightBg {255, 255, 255};
    constexpr ThemeRgb kTextOnAccent {255, 255, 255};
} // namespace

TEST_CASE("PickBestContrastCandidate selects dark foreground on Warning in both Dark and Light themes", "[theme_contrast]")
{
    // Dark theme Warning: (240, 185, 64)
    const std::array<ThemeRgb, 3> dark_candidates {kDarkText, kDarkBg, kTextOnAccent};
    const ThemeRgb dark_fg = PickBestContrastCandidate(ThemeRgb {240, 185, 64}, dark_candidates);
    REQUIRE(dark_fg.r == kDarkBg.r);
    REQUIRE(dark_fg.g == kDarkBg.g);
    REQUIRE(dark_fg.b == kDarkBg.b);
    REQUIRE(ComputeWcagContrastRatio(ThemeRgb {240, 185, 64}, dark_fg) > 7.0);

    // Light theme Warning: (180, 120, 0) where Theme::Role::Background is white and Theme::Role::Text is black
    const std::array<ThemeRgb, 3> light_candidates {kLightText, kLightBg, kTextOnAccent};
    const ThemeRgb light_fg = PickBestContrastCandidate(ThemeRgb {180, 120, 0}, light_candidates);
    REQUIRE(light_fg.r == kLightText.r);
    REQUIRE(light_fg.g == kLightText.g);
    REQUIRE(light_fg.b == kLightText.b);
    REQUIRE(ComputeWcagContrastRatio(ThemeRgb {180, 120, 0}, light_fg) > 5.0);
}

TEST_CASE("PickBestContrastCandidate selects white text on PrimaryButton and dark text on Light SecondaryButton", "[theme_contrast]")
{
    const std::array<ThemeRgb, 3> dark_candidates {kDarkText, kDarkBg, kTextOnAccent};
    const ThemeRgb primary_fg = PickBestContrastCandidate(ThemeRgb {53, 108, 178}, dark_candidates);
    REQUIRE(primary_fg.r == kTextOnAccent.r);
    REQUIRE(primary_fg.g == kTextOnAccent.g);
    REQUIRE(primary_fg.b == kTextOnAccent.b);

    const std::array<ThemeRgb, 3> light_candidates {kLightText, kLightBg, kTextOnAccent};
    const ThemeRgb secondary_light_fg = PickBestContrastCandidate(ThemeRgb {224, 228, 236}, light_candidates);
    REQUIRE(secondary_light_fg.r == kLightText.r);
    REQUIRE(secondary_light_fg.g == kLightText.g);
    REQUIRE(secondary_light_fg.b == kLightText.b);
}

TEST_CASE("PickAccessibleForeground keeps high-contrast Warning on Dark and falls back on Light", "[theme_contrast]")
{
    // Dark theme Surface (45, 45, 48) + Warning (240, 185, 64) passes >= 4.5
    const ThemeRgb dark_warn = PickAccessibleForeground(ThemeRgb {45, 45, 48}, ThemeRgb {240, 185, 64}, ThemeRgb {200, 50, 50});
    REQUIRE(dark_warn.r == 240);
    REQUIRE(dark_warn.g == 185);
    REQUIRE(dark_warn.b == 64);

    // Light theme Surface (240, 240, 240) + Warning (180, 120, 0) < 4.5 -> falls back to Error (200, 0, 0)
    const ThemeRgb light_warn = PickAccessibleForeground(ThemeRgb {240, 240, 240}, ThemeRgb {180, 120, 0}, ThemeRgb {200, 0, 0});
    REQUIRE(light_warn.r == 200);
    REQUIRE(light_warn.g == 0);
    REQUIRE(light_warn.b == 0);
    REQUIRE(ComputeWcagContrastRatio(ThemeRgb {240, 240, 240}, light_warn) >= 4.5);
}
