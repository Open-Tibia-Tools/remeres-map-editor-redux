# Themed Button Contrast Styling

## 1. Pattern & Key Files
- `ui/theme_contrast.h`: Pure WCAG relative-luminance contrast selectors (`ComputeWcagContrastRatio`, `PickBestContrastCandidate`, `PickAccessibleForeground`).
- `ui/controls/themed_button.h`: Shared `ApplyThemeButtonStyle(wxButton*, Theme::Role bg, std::optional<Theme::Role> fg)` helper.
- `tests/test_theme_contrast.cpp`: Catch2 tests for Dark and Light theme contrast ratios.

## 2. When to Use It
Do:
- Color-code dialog action buttons exclusively via `Theme::Role` (`PrimaryButton`, `SecondaryButton`, `Accent`, `ButtonDisabled`).
- Use `PickAccessibleForeground` when showing semantic status text (`Warning` -> `Error`) over `Theme::Role::Surface` so Light theme retains >= 4.5:1 WCAG contrast.
Don't:
- Don't hardcode button RGBs (`wxColour(120, 120, 120)`) or `*wxWHITE`, and don't assume `Theme::Role::Background` is always dark (in Light theme it is white).

## 3. Minimal Usage
```cpp
ApplyThemeButtonStyle(switch_btn, Theme::Role::SecondaryButton, Theme::Role::Text);
ApplyThemeButtonStyle(force_btn, Theme::Role::PrimaryButton, Theme::Role::TextOnAccent);
const ThemeRgb warn_rgb = PickAccessibleForeground(
	ToThemeRgb(surface_bg), ToThemeRgb(Theme::Get(Theme::Role::Warning)), ToThemeRgb(Theme::Get(Theme::Role::Error))
);
```

## 4. Invariants & Pitfalls
- Keep `ui/theme_contrast.h` free of `<wx/...>` headers so standalone Catch2 test targets compile without wxWidgets linkage.
