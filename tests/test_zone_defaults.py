"""
Unit test suite verifying default zone colors, opacities, blend modes, and borders disabled default.
"""

from pathlib import Path


def test_zone_defaults():
    root = Path(__file__).parent.parent

    # 1. settings.cpp
    settings_cpp = (root / "source" / "app" / "settings.cpp").read_text(encoding="utf-8")
    assert "Bool(ZONE_BORDERS_ENABLED, false);" in settings_cpp
    assert "Int(ZONE_NOPVP_COLOR_G, 219);" in settings_cpp
    assert "Int(ZONE_BLOCKING_COLOR_R, 128);" in settings_cpp
    assert "Int(ZONE_BLOCKING_COLOR_A, 89);" in settings_cpp
    assert "Int(ZONE_BLOCKING_BLEND_MODE, 0);" in settings_cpp
    assert "Int(ZONE_SPAWN_COLOR_R, 255);" in settings_cpp
    assert "Int(ZONE_SPAWN_COLOR_G, 0);" in settings_cpp
    assert "Int(ZONE_SPAWN_COLOR_B, 128);" in settings_cpp
    assert "Int(ZONE_SPAWN_COLOR_A, 89);" in settings_cpp
    assert "Int(ZONE_SPAWN_BLEND_MODE, 1);" in settings_cpp

    # 2. drawing_options.h
    options_h = (root / "source" / "rendering" / "core" / "drawing_options.h").read_text(encoding="utf-8")
    assert "bool show_zone_borders = false;" in options_h
    assert "int zone_blocking_blend_mode = 0;" in options_h
    assert "int zone_spawn_blend_mode = 1;" in options_h
    assert "int house_active_blend_mode = 1;" in options_h
    assert "int house_inactive_blend_mode = 1;" in options_h

    # 3. drawing_options.cpp
    options_cpp = (root / "source" / "rendering" / "core" / "drawing_options.cpp").read_text(encoding="utf-8")
    assert "show_zone_borders = false;" in options_cpp
    assert "zone_blocking_blend_mode = 0;" in options_cpp
    assert "zone_spawn_blend_mode = 1;" in options_cpp
    assert "house_active_blend_mode = 1;" in options_cpp
    assert "house_inactive_blend_mode = 1;" in options_cpp
    assert "128.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 89.0f / 255.0f" in options_cpp
    assert "255.0f / 255.0f, 0.0f / 255.0f, 128.0f / 255.0f, 89.0f / 255.0f" in options_cpp

    # 4. graphics_page.cpp
    graphics_cpp = (root / "source" / "app" / "preferences" / "graphics_page.cpp").read_text(encoding="utf-8")
    assert "zone_borders_enabled_chkbox->SetValue(false);" in graphics_cpp
    assert "zone_blocking_color_pick->SetColour(wxColour(128, 0, 0));" in graphics_cpp
    assert "zone_blocking_opacity_spin->SetValue(35);" in graphics_cpp
    assert "zone_blocking_blend_choice->SetSelection(0);" in graphics_cpp
    assert "zone_spawn_color_pick->SetColour(wxColour(255, 0, 128));" in graphics_cpp
    assert "zone_spawn_opacity_spin->SetValue(35);" in graphics_cpp
    assert "zone_spawn_blend_choice->SetSelection(1);" in graphics_cpp

    # 5. sprite_batch_shader.h
    sprite_batch_shader_h = (root / "source" / "rendering" / "shaders" / "sprite_batch_shader.h").read_text(encoding="utf-8")
    assert "int zone_blocking_blend_mode = 0)" in sprite_batch_shader_h

    # 6. config.toml
    config_toml = (root / "config.toml").read_text(encoding="utf-8")
    assert "[zones]" in config_toml
    assert "zone_borders_enabled = false" in config_toml
    assert "zone_blocking_color_r = 128" in config_toml
    assert "zone_blocking_color_a = 89" in config_toml
    assert "zone_blocking_blend_mode = 0" in config_toml
    assert "zone_spawn_color_r = 255" in config_toml
    assert "zone_spawn_color_b = 128" in config_toml
    assert "zone_spawn_color_a = 89" in config_toml
    assert "zone_spawn_blend_mode = 1" in config_toml
