#include <cstdlib>
#include <iostream>

#define TEST_ASSERT(cond) \
	do { \
		if (!(cond)) { \
			std::cout << "Assertion failed: " #cond " at " __FILE__ ":" << __LINE__ << "\n"; \
			std::exit(1); \
		} \
	} while (false)

#include "app/application.h"
#include "app/main.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "editor/editor.h"
#include "editor/copybuffer.h"
#include "editor/action_queue.h"
#include "editor/operations/selection_operations.h"
#include "game/creature.h"
#include "brushes/doodad/doodad_brush_items.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "game/creatures.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/tile.h"

// These two symbols live in application.cpp, which is intentionally excluded
// from the test link so its main() does not conflict with the test entry point.
Application& wxGetApp() {
	static Application* application = nullptr;
	return *application;
}

void Application::Unload() {
}

namespace {

class TestWallBrush : public WallBrush {
public:
	void setRedirect(WallBrush* brush) {
		redirect_to = brush;
	}
};

void addRotatable(ServerItemId id, ServerItemId rotate_to) {
	ResolvedItemDefinitionRow row;
	row.server_id = id;
	row.client_id = id;
	row.flags = flagMask(ItemFlag::Rotatable);
	row.rotate_to = static_cast<uint16_t>(rotate_to);
	g_item_definitions.append(std::move(row));
}

} // namespace

int main() {
	WallBrush::init();

	{
		CreatureType type;
		CreatureBrush brush(&type);
		TEST_ASSERT(brush.getMaxVariation() == 4);
		TEST_ASSERT(brush.getDirection() == SOUTH);
		TEST_ASSERT(brush.getVariation() == 2);

		brush.setVariation(0);
		TEST_ASSERT(brush.getDirection() == NORTH);
		brush.setVariation(1);
		TEST_ASSERT(brush.getDirection() == EAST);
		brush.setVariation(3);
		TEST_ASSERT(brush.getDirection() == WEST);
		brush.setVariation(99);
		TEST_ASSERT(brush.getDirection() == WEST);
	}

	g_item_definitions.clear();

	{
		TestWallBrush wall;
		wall.items.addWallItem(0, 1000, 100);
		wall.items.addWallItem(0, 1001, 100);
		wall.items.addWallItem(0, 1002, 100);
		wall.items.addWallItem(1, 2000, 100);

		for (ServerItemId id : { 1000, 1001, 1002, 2000 }) {
			ResolvedItemDefinitionRow row;
			row.server_id = id;
			row.client_id = id;
			row.flags = flagMask(ItemFlag::IsWall);
			row.border_alignment = static_cast<BorderType>(0);
			g_item_definitions.append(std::move(row));

			ItemEditorData editor_data;
			editor_data.brush = &wall;
			g_item_definitions.setEditorData(id, editor_data);
		}

		TEST_ASSERT(wall.findAlignmentForItem(1000) == static_cast<BorderType>(0));
		TEST_ASSERT(wall.findNextWallVariant(1000) == 1001);
		TEST_ASSERT(wall.findNextWallVariant(1001) == 1002);
		TEST_ASSERT(wall.findNextWallVariant(1002) == 1000);
		TEST_ASSERT(!wall.findNextWallVariant(9999).has_value());

		Tile tile(0, 0, 0);
		tile.addItem(std::make_unique<Item>(1000, 0));
		bool special = true;
		wall.draw(nullptr, &tile, &special);
		TEST_ASSERT(tile.items.front()->getID() == 1001);
		wall.draw(nullptr, &tile, &special);
		TEST_ASSERT(tile.items.front()->getID() == 1002);
		wall.draw(nullptr, &tile, &special);
		TEST_ASSERT(tile.items.front()->getID() == 1000);

		TestWallBrush windows;
		windows.items.addWallItem(0, 3000, 100);
		windows.items.addWallItem(0, 3001, 100);
		wall.setRedirect(&windows);

		TEST_ASSERT(wall.findNextWallVariant(1002) == 3000);
		TEST_ASSERT(wall.findNextWallVariant(3001) == 1000);
	}

	static TestWallBrush rotated;
	rotated.items.addWallItem(WALL_SOUTH_END, 4000, 100);
	rotated.items.addWallItem(WALL_SOUTH_END, 4001, 100);
	rotated.items.addWallItem(WALL_WEST_END, 4010, 100);
	rotated.items.addWallItem(WALL_WEST_END, 4011, 100);
	rotated.items.addWallItem(WALL_EAST_END, 4020, 100);
	rotated.items.addWallItem(WALL_EAST_END, 4021, 100);
	rotated.items.addWallItem(WALL_NORTHEAST_DIAGONAL, 4030, 100);
	rotated.items.addWallItem(WALL_NORTH_END, 4040, 100);

	for (ServerItemId id : { 4000, 4001, 4010, 4011, 4020, 4021, 4030, 4040 }) {
		ResolvedItemDefinitionRow row;
		row.server_id = id;
		row.client_id = id;
		row.flags = flagMask(ItemFlag::IsWall);
		if (id == 4000 || id == 4001) row.border_alignment = WALL_SOUTH_END;
		else if (id == 4010 || id == 4011) row.border_alignment = WALL_WEST_END;
		else if (id == 4020 || id == 4021) row.border_alignment = WALL_EAST_END;
		else if (id == 4030) row.border_alignment = WALL_NORTHEAST_DIAGONAL;
		else if (id == 4040) row.border_alignment = WALL_NORTH_END;
		g_item_definitions.append(std::move(row));

		ItemEditorData editor_data;
		editor_data.brush = &rotated;
		g_item_definitions.setEditorData(id, editor_data);
	}


	static TestWallBrush door_wall;
	door_wall.items.addDoorItem(WALL_HORIZONTAL, 5000, WALL_DOOR_NORMAL, false);
	door_wall.items.addDoorItem(WALL_HORIZONTAL, 5001, WALL_DOOR_NORMAL, false);

	for (const auto& [id, open] : { std::pair<ServerItemId, bool> { 5000, false }, { 5001, true } }) {
		ResolvedItemDefinitionRow row;
		row.server_id = id;
		row.client_id = id;
		row.flags = flagMask(ItemFlag::IsWall) | flagMask(ItemFlag::IsBrushDoor);
		row.border_alignment = WALL_HORIZONTAL;
		if (open) {
			row.flags |= flagMask(ItemFlag::IsOpen);
		}
		g_item_definitions.append(std::move(row));

		ItemEditorData editor_data;
		editor_data.brush = &door_wall;
		g_item_definitions.setEditorData(id, editor_data);
	}

	Tile test_door_tile(0, 0, 0);
	test_door_tile.addItem(std::make_unique<Item>(5000, 0));

	DoorBrush door(WALL_DOOR_NORMAL);
	bool alt = true;
	door.draw(nullptr, &test_door_tile, &alt);
	TEST_ASSERT(test_door_tile.items.front()->getID() == 5001);
	door.draw(nullptr, &test_door_tile, &alt);
	TEST_ASSERT(test_door_tile.items.front()->getID() == 5000);

	addRotatable(100, 101);
	addRotatable(101, 102);
	addRotatable(102, 103);
	addRotatable(103, 100);

	{
		Item item(100, 0);
		TEST_ASSERT(item.isRoteable());
		item.doRotate();
		TEST_ASSERT(item.getID() == 101);
		item.doRotateBackwards();
		TEST_ASSERT(item.getID() == 100);
		item.doRotateBackwards();
		TEST_ASSERT(item.getID() == 103);
	}

	{
		Item item(200, 0);
		item.doRotate();
		TEST_ASSERT(item.getID() == 200);
		item.doRotateBackwards();
		TEST_ASSERT(item.getID() == 200);
	}

	{
		ResolvedItemDefinitionRow row;
		row.server_id = 300;
		row.client_id = 300;
		row.rotate_to = 301;
		g_item_definitions.append(std::move(row));

		Item item(300, 0);
		TEST_ASSERT(item.isRoteable());
		item.doRotate();
		TEST_ASSERT(item.getID() == 301);
	}

	{
		DoodadBrush doodad;
		TEST_ASSERT(doodad.getVariation() == 0);
		TEST_ASSERT(doodad.getTotalVariants() == 0);
		doodad.setVariation(5);
		TEST_ASSERT(doodad.getVariation() == 5);
		doodad.setVariation(-1);
		TEST_ASSERT(doodad.getVariation() == 0);

		DoodadBrushItems items;
		TEST_ASSERT(items.getTotalVariants() == 0);

		auto* alt = items.createAlternative();
		items.addSingleToBlock(alt, std::make_unique<Item>(2740, 0), 10);
		items.addSingleToBlock(alt, std::make_unique<Item>(2741, 0), 10);
		items.addSingleToBlock(alt, std::make_unique<Item>(2742, 0), 10);

		TEST_ASSERT(items.getTotalVariants() == 3);

		auto v0 = items.getVariant(0);
		TEST_ASSERT(v0.single_item != nullptr);
		TEST_ASSERT(v0.single_item->getID() == 2740);

		auto v1 = items.getVariant(1);
		TEST_ASSERT(v1.single_item != nullptr);
		TEST_ASSERT(v1.single_item->getID() == 2741);

		auto v2 = items.getVariant(2);
		TEST_ASSERT(v2.single_item != nullptr);
		TEST_ASSERT(v2.single_item->getID() == 2742);

		// Check wrapping forward and backward
		auto v3 = items.getVariant(3);
		TEST_ASSERT(v3.single_item->getID() == 2740);

		auto v_prev = items.getVariant(-1);
		TEST_ASSERT(v_prev.single_item->getID() == 2742);

		// Advance 10 times with 'X' and step back with 'Z'
		std::vector<uint16_t> history;
		int var = 0;
		for (int i = 0; i < 11; ++i) {
			auto v = items.getVariant(var);
			TEST_ASSERT(v.single_item != nullptr);
			history.push_back(v.single_item->getID());
			++var;
		}
		--var;
		for (int i = 10; i >= 0; --i) {
			auto v = items.getVariant(var);
			TEST_ASSERT(v.single_item != nullptr);
			TEST_ASSERT(v.single_item->getID() == history[i]);
			var = std::max(0, var - 1);
		}
	}

	// Buffer tile preservation on doodad draw (merge of copy preserves template)
	{
		BaseMap map;
		Tile* buffer_tile = map.createTile(100, 100, 7);
		buffer_tile->addItem(std::make_unique<Item>(2740, 0));
		TEST_ASSERT(buffer_tile->items.size() == 1);

		Tile* dest_tile = map.createTile(200, 200, 7);
		TEST_ASSERT(dest_tile->items.empty());

		auto buffer_copy = TileOperations::deepCopy(buffer_tile, map);
		TileOperations::merge(dest_tile, buffer_copy.get(), &map);

		// buffer_tile must STILL have its item!
		TEST_ASSERT(buffer_tile->items.size() == 1);
		TEST_ASSERT(buffer_tile->items.front()->getID() == 2740);

		// dest_tile must have received the item
		TEST_ASSERT(dest_tile->items.size() == 1);
		TEST_ASSERT(dest_tile->items.front()->getID() == 2740);
	}

	// Multi-entity in-place selection rotation (chairs, creatures, walls, doors across tiles)
	{
		CopyBuffer copybuffer;
		MapVersion version;
		Editor editor(copybuffer, version);

		// Tile 1: (10, 10, 7) with rotatable item 100 (selected) and unselected item 9999
		Tile* t1 = editor.map.createTile(10, 10, 7);
		auto unselected_item = std::make_unique<Item>(9999, 0);
		t1->addItem(std::move(unselected_item));
		auto rot_item = std::make_unique<Item>(100, 0);
		Item* p_rot_item = rot_item.get();
		t1->addItem(std::move(rot_item));
		editor.selection.add(t1, p_rot_item);

		// Tile 2: (10, 11, 7) with creature facing SOUTH (selected)
		Tile* t2 = editor.map.createTile(10, 11, 7);
		CreatureType ctype;
		auto creature = std::make_unique<Creature>(&ctype);
		creature->setDirection(SOUTH);
		Creature* p_creature = creature.get();
		t2->creature = std::move(creature);
		editor.selection.add(t2, p_creature);

		// Tile 3: (11, 10, 7) with wall item 4001 (selected)
		Tile* t3 = editor.map.createTile(11, 10, 7);
		auto wall_item = std::make_unique<Item>(4001, 0);
		Item* p_wall_item = wall_item.get();
		t3->addItem(std::move(wall_item));
		editor.selection.add(t3, p_wall_item);

		// Tile 4: (11, 11, 7) with door item 5000 (selected)
		Tile* t4 = editor.map.createTile(11, 11, 7);
		auto door_item = std::make_unique<Item>(5000, 0);
		Item* p_door_item = door_item.get();
		t4->addItem(std::move(door_item));
		editor.selection.add(t4, p_door_item);

		TEST_ASSERT(editor.selection.size() == 4);

		// Rotate clockwise ('X' key)
		bool rotated_cw = SelectionOperations::rotateSelection(editor, true);
		TEST_ASSERT(rotated_cw);

		// Verify tiles stayed in place
		Tile* cur_t1 = editor.map.getTile(10, 10, 7);
		Tile* cur_t2 = editor.map.getTile(10, 11, 7);
		Tile* cur_t3 = editor.map.getTile(11, 10, 7);
		Tile* cur_t4 = editor.map.getTile(11, 11, 7);
		TEST_ASSERT(cur_t1 != nullptr);
		TEST_ASSERT(cur_t2 != nullptr);
		TEST_ASSERT(cur_t3 != nullptr);
		TEST_ASSERT(cur_t4 != nullptr);

		// Verify contents rotated in-place
		TEST_ASSERT(cur_t1->items.size() == 2);
		TEST_ASSERT(cur_t1->items[0]->getID() == 9999); // unselected item preserved!
		TEST_ASSERT(cur_t1->items[1]->getID() == 101);  // 100 rotated to 101

		TEST_ASSERT(cur_t2->creature != nullptr);
		TEST_ASSERT(cur_t2->creature->getDirection() == WEST); // SOUTH -> WEST

		TEST_ASSERT(cur_t3->items.size() == 1);
		TEST_ASSERT(cur_t3->items[0]->getID() == 4001); // wall 4001 remains unchanged (walls do not rotate)

		TEST_ASSERT(cur_t4->items.size() == 1);
		TEST_ASSERT(cur_t4->items[0]->getID() == 5001); // door 5000 -> 5001 (toggled open)

		// Rotate counter-clockwise ('Z' key)
		bool rotated_ccw = SelectionOperations::rotateSelection(editor, false);
		TEST_ASSERT(rotated_ccw);

		cur_t1 = editor.map.getTile(10, 10, 7);
		cur_t2 = editor.map.getTile(10, 11, 7);
		cur_t3 = editor.map.getTile(11, 10, 7);
		cur_t4 = editor.map.getTile(11, 11, 7);

		TEST_ASSERT(cur_t1->items[1]->getID() == 100);  // 101 rotated back to 100
		TEST_ASSERT(cur_t2->creature->getDirection() == SOUTH); // WEST -> SOUTH
		TEST_ASSERT(cur_t3->items[0]->getID() == 4001); // 4011 -> 4001
		TEST_ASSERT(cur_t4->items[0]->getID() == 5000); // 5001 -> 5000 (toggled closed)

		// Undo test
		editor.actionQueue->undo();
		cur_t1 = editor.map.getTile(10, 10, 7);
		cur_t2 = editor.map.getTile(10, 11, 7);
		TEST_ASSERT(cur_t1->items[1]->getID() == 101); // undone to first rotation step
		editor.actionQueue->undo();
		cur_t1 = editor.map.getTile(10, 10, 7);
		TEST_ASSERT(cur_t1->items[1]->getID() == 100); // undone to initial state!
	}

	std::cout << "universal rotator tests passed\n";
	return 0;
}
