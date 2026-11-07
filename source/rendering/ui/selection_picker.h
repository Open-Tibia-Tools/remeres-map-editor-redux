#ifndef RME_RENDERING_UI_SELECTION_PICKER_H_
#define RME_RENDERING_UI_SELECTION_PICKER_H_

#include <cstdint>

class Creature;
class Editor;
class Item;
class Spawn;
class Tile;
class Position;

enum class SelectionMethod : uint8_t {
	Standard,
	PixelPerfect,
};

struct SelectionPixelOffset {
	int x = 0;
	int y = 0;
};

enum class SelectionHitKind : uint8_t {
	None,
	Ground,
	Item,
	Creature,
	Spawn,
};

struct SelectionHit {
	SelectionHitKind kind = SelectionHitKind::None;
	Tile* tile = nullptr;
	Item* item = nullptr;
	Creature* creature = nullptr;
	Spawn* spawn = nullptr;

	[[nodiscard]] bool HasEntity() const {
		return kind != SelectionHitKind::None;
	}
};

class SelectionPicker {
public:
	static SelectionHit Pick(Editor& editor, const Position& clicked_pos, const SelectionPixelOffset& pixel, SelectionMethod method);
	static SelectionMethod ActiveMethod();
};

#endif
