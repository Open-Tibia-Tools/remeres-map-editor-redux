#ifndef RME_UI_ICONS_EDITOR_ICON_REGISTRY_H_
#define RME_UI_ICONS_EDITOR_ICON_REGISTRY_H_

#include "ui/icons/editor_icon.h"
#include <unordered_map>
#include <memory>

class EditorIconRegistry {
public:
	static bool Load();
	static EditorIcon* GetIcon(int id);
	static wxBitmap* GetBitmap(int id, SpriteSize size);
	static bool HasIcon(int id);
	static void RegisterIcon(int id, std::unique_ptr<EditorIcon> icon);
	static void Clear();

private:
	static std::unordered_map<int, std::unique_ptr<EditorIcon>>& GetStorage();
};

#endif
