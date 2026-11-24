#ifndef RME_UI_CONTEXT_MENU_ITEM_FACTS_H_
#define RME_UI_CONTEXT_MENU_ITEM_FACTS_H_

class Item;

namespace ContextMenu {

	// Facts about one item (a ground row or a stacked item). The per-tile menu only reads the
	// subset it needs; the per-item menu maps every flag to one row-scoped entry.
	struct SelectedItemFacts {
		bool is_rotatable = false;
		bool is_teleport = false;
		bool teleport_has_destination = false;
		bool is_door = false;
		bool is_open_door = false;
		bool is_brush_door = false;
		bool has_door_brush = false;
		bool has_visible_doodad_brush = false;
		bool has_collection_brush = false;
		bool has_visible_wall_brush = false;
		bool has_visible_carpet_brush = false;
		bool has_visible_table_brush = false;
		bool has_visible_ground_brush = false;
		bool has_raw_brush = false;
	};

	// Reads the facts of `item`. The result only holds bools and does not outlive the call.
	[[nodiscard]] SelectedItemFacts CollectItemFacts(const Item& item);

} // namespace ContextMenu

#endif
