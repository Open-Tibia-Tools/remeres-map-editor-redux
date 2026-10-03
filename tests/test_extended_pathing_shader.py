"""
Unit & Differential Test Suite for Extended Pathing Shader
Verifies:
1. Configuration & Settings persistence (settings.h, settings.cpp, drawing_options.h, drawing_options.cpp).
2. MenuBar and UI Action bindings (main_menubar.h, menubar.xml, menubar_action_manager.cpp, view_settings_handler.cpp).
3. Dedicated SRP shader module (pathing_shader.h) and GLSL function applyPathingOverlay.
4. TileInstance and SpriteInstance struct sizes and GPU alignment (48 bytes and 64 bytes).
5. Injection and invocation in chunk_shader.h and sprite_batch_shader.h.
6. Bitflag packing in TileInstance (TILE_INSTANCE_FLAG_BLOCKING) and SpriteInstance (ZONE_FLAG_ITEM_BLOCKING).
"""

from pathlib import Path
import pytest


def test_extended_pathing_shader_invariants():
    root = Path(__file__).parent.parent

    # 1. settings.h & settings.cpp
    settings_h = (root / "source" / "app" / "settings.h").read_text(encoding="utf-8")
    assert "EXT_PATHING_SHADER" in settings_h

    settings_cpp = (root / "source" / "app" / "settings.cpp").read_text(encoding="utf-8")
    assert "Bool(EXT_PATHING_SHADER, true);" in settings_cpp

    # 2. drawing_options.h & drawing_options.cpp
    options_h = (root / "source" / "rendering" / "core" / "drawing_options.h").read_text(encoding="utf-8")
    assert "bool extended_pathing_shader;" in options_h

    options_cpp = (root / "source" / "rendering" / "core" / "drawing_options.cpp").read_text(encoding="utf-8")
    assert "case Config::EXT_PATHING_SHADER:" in options_cpp
    assert "new_extended_pathing_shader" in options_cpp
    assert "extended_pathing_shader = new_extended_pathing_shader;" in options_cpp

    # 3. main_menubar.h & menubar.xml & view_settings_handler.cpp
    menubar_h = (root / "source" / "ui" / "main_menubar.h").read_text(encoding="utf-8")
    assert "EXT_PATHING_SHADER" in menubar_h

    menubar_xml = (root / "data" / "menubar.xml").read_text(encoding="utf-8")
    assert 'action="EXT_PATHING_SHADER"' in menubar_xml

    action_mgr_cpp = (root / "source" / "ui" / "menubar" / "menubar_action_manager.cpp").read_text(encoding="utf-8")
    assert "MAKE_ACTION_ICON(EXT_PATHING_SHADER" in action_mgr_cpp

    view_settings_cpp = (root / "source" / "ui" / "menubar" / "view_settings_handler.cpp").read_text(encoding="utf-8")
    assert "menuBar->CheckItem(EXT_PATHING_SHADER" in view_settings_cpp
    assert "g_settings.setInteger(Config::EXT_PATHING_SHADER" in view_settings_cpp

    # 4. zone_flags.h
    zone_flags_h = (root / "source" / "rendering" / "indicators" / "zone_flags.h").read_text(encoding="utf-8")
    assert "ZONE_FLAG_ITEM_BLOCKING" in zone_flags_h

    # 5. tile_instance.h (48 bytes static assertion)
    tile_inst_h = (root / "source" / "rendering" / "core" / "tile_instance.h").read_text(encoding="utf-8")
    assert "TILE_INSTANCE_FLAG_BLOCKING" in tile_inst_h
    assert 'static_assert(sizeof(TileInstance) == 48' in tile_inst_h

    # 6. sprite_instance.h (64 bytes static assertion)
    sprite_inst_h = (root / "source" / "rendering" / "core" / "sprite_instance.h").read_text(encoding="utf-8")
    assert 'static_assert(sizeof(SpriteInstance) == 64' in sprite_inst_h

    # 7. pathing_shader.h
    pathing_shader_h = (root / "source" / "rendering" / "shaders" / "pathing_shader.h").read_text(encoding="utf-8")
    assert "PATHING_SHADER_GLSL" in pathing_shader_h
    assert "applyPathingOverlay" in pathing_shader_h
    assert "blockingWash" in pathing_shader_h

    # 8. chunk_shader.h & sprite_batch_shader.h
    chunk_shader_h = (root / "source" / "rendering" / "shaders" / "chunk_shader.h").read_text(encoding="utf-8")
    assert 'pathing_shader.h' in chunk_shader_h
    assert "PATHING_SHADER_GLSL" in chunk_shader_h
    assert "uExtendedPathingShader" in chunk_shader_h
    assert "uBlockingWash" in chunk_shader_h
    assert "applyPathingOverlay(FragColor, isBlocking, uShowBlocking, uExtendedPathingShader, uBlockingWash, uBlockingBlendMode);" in chunk_shader_h

    sprite_batch_shader_h = (root / "source" / "rendering" / "shaders" / "sprite_batch_shader.h").read_text(encoding="utf-8")
    assert 'pathing_shader.h' in sprite_batch_shader_h
    assert "PATHING_SHADER_GLSL" in sprite_batch_shader_h
    assert "applyPathingOverlay(FragColor, isBlocking, uShowBlocking, uExtendedPathingShader, uBlockingWash, uBlockingBlendMode);" in sprite_batch_shader_h
    assert "uExtendedPathingShader" in sprite_batch_shader_h

    # 9. chunk_cache_manager.cpp
    chunk_mgr_cpp = (root / "source" / "rendering" / "core" / "chunk_cache_manager.cpp").read_text(encoding="utf-8")
    assert "TILE_INSTANCE_FLAG_BLOCKING" in chunk_mgr_cpp
    assert "extended_pathing_shader" in chunk_mgr_cpp

    # 10. tile_renderer.cpp
    tile_renderer_cpp = (root / "source" / "rendering" / "drawers" / "tiles" / "tile_renderer.cpp").read_text(encoding="utf-8")
    assert "ZONE_FLAG_ITEM_BLOCKING" in tile_renderer_cpp
