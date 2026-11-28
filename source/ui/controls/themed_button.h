#ifndef RME_UI_THEMED_BUTTON_H_
#define RME_UI_THEMED_BUTTON_H_

#include "ui/theme.h"
#include "ui/theme_contrast.h"

#include <wx/button.h>
#include <array>
#include <optional>

inline ThemeRgb ToThemeRgb(const wxColour& colour) noexcept
{
    return ThemeRgb {
        .r = colour.Red(),
        .g = colour.Green(),
        .b = colour.Blue(),
    };
}

inline void ApplyThemeButtonStyle(wxButton* button, Theme::Role background_role, std::optional<Theme::Role> foreground_role = std::nullopt)
{
    if (!button) {
        return;
    }

    const wxColour bg = Theme::Get(background_role);
    button->SetBackgroundColour(bg);

    if (foreground_role.has_value()) {
        button->SetForegroundColour(Theme::Get(*foreground_role));
        return;
    }

    const std::array<ThemeRgb, 3> candidates {
        ToThemeRgb(Theme::Get(Theme::Role::Text)),
        ToThemeRgb(Theme::Get(Theme::Role::Background)),
        ToThemeRgb(Theme::Get(Theme::Role::TextOnAccent)),
    };
    const ThemeRgb best = PickBestContrastCandidate(ToThemeRgb(bg), candidates);
    button->SetForegroundColour(wxColour(best.r, best.g, best.b));
}

#endif
