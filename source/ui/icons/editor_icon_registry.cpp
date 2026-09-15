#include "ui/icons/editor_icon_registry.h"
#include "app/definitions.h"
#include "game/sprites.h"
#include "util/image_manager.h"

namespace {
	std::unique_ptr<wxBitmap> createSelectionMarkerBitmap(int size) {
		wxImage img(size, size, false);
		img.InitAlpha();
		for (int y = 0; y < size; ++y) {
			for (int x = 0; x < size; ++x) {
				if ((x + y) % 2 == 1) {
					img.SetRGB(x, y, 0x00, 0x00, 0x80);
					img.SetAlpha(x, y, 255);
				} else {
					img.SetRGB(x, y, 0, 0, 0);
					img.SetAlpha(x, y, 0);
				}
			}
		}
		return std::make_unique<wxBitmap>(img);
	}

	std::unique_ptr<EditorIcon> makeEditorIcon(std::string_view pathSmall, std::string_view pathLarge) {
		return std::make_unique<EditorIcon>(
			std::make_unique<wxBitmap>(IMAGE_MANAGER.GetBitmap(pathSmall)),
			std::make_unique<wxBitmap>(IMAGE_MANAGER.GetBitmap(pathLarge))
		);
	}

	std::unique_ptr<EditorIcon> makeSingleEditorIcon(std::string_view path) {
		return std::make_unique<EditorIcon>(
			std::make_unique<wxBitmap>(IMAGE_MANAGER.GetBitmap(path))
		);
	}
} // namespace

std::unordered_map<int, std::unique_ptr<EditorIcon>>& EditorIconRegistry::GetStorage() {
	static std::unordered_map<int, std::unique_ptr<EditorIcon>> storage;
	return storage;
}

void EditorIconRegistry::RegisterIcon(int id, std::unique_ptr<EditorIcon> icon) {
	GetStorage()[id] = std::move(icon);
}

EditorIcon* EditorIconRegistry::GetIcon(int id) {
	auto& storage = GetStorage();
	if (auto it = storage.find(id); it != storage.end()) {
		return it->second.get();
	}
	return nullptr;
}

wxBitmap* EditorIconRegistry::GetBitmap(int id, SpriteSize size) {
	if (auto* icon = GetIcon(id)) {
		return icon->getBitmap(size);
	}
	return nullptr;
}

bool EditorIconRegistry::HasIcon(int id) {
	auto& storage = GetStorage();
	return storage.find(id) != storage.end();
}

void EditorIconRegistry::Clear() {
	GetStorage().clear();
}

bool EditorIconRegistry::Load() {
	Clear();

	RegisterIcon(EDITOR_SPRITE_SELECTION_MARKER, std::make_unique<EditorIcon>(
		createSelectionMarkerBitmap(16),
		createSelectionMarkerBitmap(32)
	));

	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_1x1, makeEditorIcon(IMAGE_CIRCULAR_1_SMALL, IMAGE_CIRCULAR_1));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_3x3, makeEditorIcon(IMAGE_CIRCULAR_2_SMALL, IMAGE_CIRCULAR_2));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_5x5, makeEditorIcon(IMAGE_CIRCULAR_3_SMALL, IMAGE_CIRCULAR_3));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_7x7, makeEditorIcon(IMAGE_CIRCULAR_4_SMALL, IMAGE_CIRCULAR_4));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_9x9, makeEditorIcon(IMAGE_CIRCULAR_5_SMALL, IMAGE_CIRCULAR_5));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_15x15, makeEditorIcon(IMAGE_CIRCULAR_6_SMALL, IMAGE_CIRCULAR_6));
	RegisterIcon(EDITOR_SPRITE_BRUSH_CD_19x19, makeEditorIcon(IMAGE_CIRCULAR_7_SMALL, IMAGE_CIRCULAR_7));

	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_1x1, makeEditorIcon(IMAGE_RECTANGULAR_1_SMALL, IMAGE_RECTANGULAR_1));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_3x3, makeEditorIcon(IMAGE_RECTANGULAR_2_SMALL, IMAGE_RECTANGULAR_2));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_5x5, makeEditorIcon(IMAGE_RECTANGULAR_3_SMALL, IMAGE_RECTANGULAR_3));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_7x7, makeEditorIcon(IMAGE_RECTANGULAR_4_SMALL, IMAGE_RECTANGULAR_4));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_9x9, makeEditorIcon(IMAGE_RECTANGULAR_5_SMALL, IMAGE_RECTANGULAR_5));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_15x15, makeEditorIcon(IMAGE_RECTANGULAR_6_SMALL, IMAGE_RECTANGULAR_6));
	RegisterIcon(EDITOR_SPRITE_BRUSH_SD_19x19, makeEditorIcon(IMAGE_RECTANGULAR_7_SMALL, IMAGE_RECTANGULAR_7));

	RegisterIcon(EDITOR_SPRITE_OPTIONAL_BORDER_TOOL, makeEditorIcon(IMAGE_OPTIONAL_BORDER_SMALL, IMAGE_OPTIONAL_BORDER));
	RegisterIcon(EDITOR_SPRITE_ERASER, makeEditorIcon(IMAGE_ERASER_SMALL, IMAGE_ERASER));
	RegisterIcon(EDITOR_SPRITE_PZ_TOOL, makeEditorIcon(IMAGE_PROTECTION_ZONE_SMALL, IMAGE_PROTECTION_ZONE));
	RegisterIcon(EDITOR_SPRITE_PVPZ_TOOL, makeEditorIcon(IMAGE_PVP_ZONE_SMALL, IMAGE_PVP_ZONE));
	RegisterIcon(EDITOR_SPRITE_NOLOG_TOOL, makeEditorIcon(IMAGE_NO_LOGOUT_ZONE_SMALL, IMAGE_NO_LOGOUT_ZONE));
	RegisterIcon(EDITOR_SPRITE_NOPVP_TOOL, makeEditorIcon(IMAGE_NO_PVP_ZONE_SMALL, IMAGE_NO_PVP_ZONE));

	RegisterIcon(EDITOR_SPRITE_DOOR_NORMAL, makeEditorIcon(IMAGE_DOOR_NORMAL_SMALL, IMAGE_DOOR_NORMAL));
	RegisterIcon(EDITOR_SPRITE_DOOR_LOCKED, makeEditorIcon(IMAGE_DOOR_LOCKED_SMALL, IMAGE_DOOR_LOCKED));
	RegisterIcon(EDITOR_SPRITE_DOOR_MAGIC, makeEditorIcon(IMAGE_DOOR_MAGIC_SMALL, IMAGE_DOOR_MAGIC));
	RegisterIcon(EDITOR_SPRITE_DOOR_QUEST, makeEditorIcon(IMAGE_DOOR_QUEST_SMALL, IMAGE_DOOR_QUEST));
	RegisterIcon(EDITOR_SPRITE_DOOR_NORMAL_ALT, makeEditorIcon(IMAGE_DOOR_NORMAL_ALT_SMALL, IMAGE_DOOR_NORMAL_ALT));
	RegisterIcon(EDITOR_SPRITE_DOOR_ARCHWAY, makeEditorIcon(IMAGE_DOOR_ARCHWAY_SMALL, IMAGE_DOOR_ARCHWAY));
	RegisterIcon(EDITOR_SPRITE_WINDOW_NORMAL, makeEditorIcon(IMAGE_WINDOW_NORMAL_SMALL, IMAGE_WINDOW_NORMAL));
	RegisterIcon(EDITOR_SPRITE_WINDOW_HATCH, makeEditorIcon(IMAGE_WINDOW_HATCH_SMALL, IMAGE_WINDOW_HATCH));

	RegisterIcon(EDITOR_SPRITE_SELECTION_GEM, makeSingleEditorIcon(IMAGE_GEM_EDIT));
	RegisterIcon(EDITOR_SPRITE_DRAWING_GEM, makeSingleEditorIcon(IMAGE_GEM_MOVE));

	return true;
}
