# Themed Button Contrast Styling

## 1. Pattern & Key Files
- `ui/theme_contrast.h`: Pure WCAG relative-luminance contrast selector (`ComputeWcagContrastRatio`, `PickBestContrastCandidate`).
- `ui/controls/themed_button.h`: Shared `ApplyThemeButtonStyle(wxButton*, Theme::Role bg, std::optional<Theme::Role> fg)` helper.
- `tests/test_theme_contrast.cpp`: Catch2 tests for Dark and Light theme contrast ratios.

## 2. When to Use It
Do:
- Color-code dialog action buttons exclusively via `Theme::Role` (`Warning`, `PrimaryButton`, `SecondaryButton`, `Accent`, `ButtonDisabled`).
- Leave `foreground_role` as `std::nullopt` on bright or theme-varying fills like `Theme::Role::Warning` so contrast picks `Text` (Light) or `Background` (Dark) automatically.
Don't:
- Don't hardcode button RGBs (`wxColour(120, 120, 120)`) or `*wxWHITE`, and don't assume `Theme::Role::Background` is always dark (in Light theme it is white).

## 3. Minimal Usage
```cpp
ApplyThemeButtonStyle(force_btn, Theme::Role::Warning);
ApplyThemeButtonStyle(switch_btn, Theme::Role::PrimaryButton, Theme::Role::TextOnAccent);
ApplyThemeButtonStyle(cancel_btn, Theme::Role::SecondaryButton, Theme::Role::Text);
```

## 4. Invariants & Pitfalls
- Always include `ui/theme_contrast.h` cleanly without `<wx/...>` headers in `theme_contrast.h` so Catch2 tests build standalone.
