//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "editor/operations/draw_operations.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "ui/gui.h"
#include "brushes/managers/doodad_preview_manager.h"
#include "brushes/brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/border/optional_border_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/door/door_brush.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "app/settings.h"

namespace {

	void drawDoodad(Editor& editor, DoodadBrush* brush, Position offset, bool alt, bool dodraw) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());
		BaseMap* buffer_map = g_doodad_preview.GetBufferMap();

		Position delta_pos = offset - Position(0x8000, 0x8000, 0x8);
		PositionList tilestoborder;

		for (auto& tile_ptr : *buffer_map) {
			auto* buffer_tile = tile_ptr.get();
			Position pos = buffer_tile->getPosition() + delta_pos;
			if (!pos.isValid()) {
				continue;
			}

			auto* location = editor.map.createTileL(pos);
			auto* tile = location->get();

			if (!dodraw) {
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					brush->undraw(&editor.map, new_tile.get());
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
				tilestoborder.push_back(pos);
				continue;
			}

			if (brush->placeOnBlocking() || alt) {
				if (tile) {
					bool place = true;
					if (!brush->placeOnDuplicate() && !alt) {
						for (const auto& item : tile->items) {
							if (brush->ownsItem(item.get())) {
								place = false;
								break;
							}
						}
					}
					if (place) {
						std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
						SelectionOperations::removeDuplicateWalls(buffer_tile, new_tile.get());
						SelectionOperations::doSurroundingBorders(brush, tilestoborder, buffer_tile, new_tile.get());
						TileOperations::merge(new_tile.get(), buffer_tile);
						action->addChange(std::make_unique<Change>(std::move(new_tile)));
					}
				} else {
					std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
					SelectionOperations::removeDuplicateWalls(buffer_tile, new_tile.get());
					SelectionOperations::doSurroundingBorders(brush, tilestoborder, buffer_tile, new_tile.get());
					TileOperations::merge(new_tile.get(), buffer_tile);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			} else {
				if (tile && !tile->isBlocking()) {
					bool place = true;
					if (!brush->placeOnDuplicate() && !alt) {
						for (const auto& item : tile->items) {
							if (brush->ownsItem(item.get())) {
								place = false;
								break;
							}
						}
					}
					if (place) {
						std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
						SelectionOperations::removeDuplicateWalls(buffer_tile, new_tile.get());
						SelectionOperations::doSurroundingBorders(brush, tilestoborder, buffer_tile, new_tile.get());
						TileOperations::merge(new_tile.get(), buffer_tile);
						action->addChange(std::make_unique<Change>(std::move(new_tile)));
					}
				}
			}
		}
		batch->addAndCommitAction(std::move(action));

		if (!tilestoborder.empty()) {
			action = editor.actionQueue->createAction(batch.get());

			// Remove duplicates
			tilestoborder.sort();
			tilestoborder.unique();

			for (const auto& pos : tilestoborder) {
				Tile* tile = editor.map.getTile(pos);
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					TileOperations::borderize(new_tile.get(), &editor.map);
					TileOperations::wallize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			}
			batch->addAndCommitAction(std::move(action));
		}
		editor.addBatch(std::move(batch), 2);
	}

	template <typename T>
	void drawGroundOrEraserImpl(Editor& editor, T* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());

		std::pair<bool, GroundBrush*> param_obj;
		std::pair<bool, GroundBrush*>* param = nullptr;
		if constexpr (std::is_same_v<T, GroundBrush>) {
			if (alt) {
				param_obj = editor.replace_brush
					? std::pair<bool, GroundBrush*> { false, editor.replace_brush }
					: std::pair<bool, GroundBrush*> { true, nullptr };
				param = &param_obj;
			}
		}

		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
					TileOperations::cleanBorders(new_tile.get());
				}

				if (dodraw) {
					brush->draw(&editor.map, new_tile.get(), param);
				} else {
					brush->undraw(&editor.map, new_tile.get());
					tilestoborder.push_back(drawPos);
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				brush->draw(&editor.map, new_tile.get(), param);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}

		// Commit changes to map
		batch->addAndCommitAction(std::move(action));

		if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
			// Do borders!
			action = editor.actionQueue->createAction(batch.get());
			for (const auto& borderPos : tilestoborder) {
				auto* location = editor.map.createTileL(borderPos);
				auto* tile = location->get();
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					if (brush->template is<EraserBrush>()) {
						TileOperations::wallize(new_tile.get(), &editor.map);
						TileOperations::tableize(new_tile.get(), &editor.map);
						TileOperations::carpetize(new_tile.get(), &editor.map);
					}
					TileOperations::borderize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				} else {
					std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
					if (brush->template is<EraserBrush>()) {
						// There are no carpets/tables/walls on empty tiles...
						// new_tile->wallize(map);
						// new_tile->tableize(map);
						// new_tile->carpetize(map);
					}
					TileOperations::borderize(new_tile.get(), &editor.map);
					if (!new_tile->empty()) {
						action->addChange(std::make_unique<Change>(std::move(new_tile)));
					}
				}
			}
			batch->addAndCommitAction(std::move(action));
		}

		editor.addBatch(std::move(batch), 2);
	}

	void drawGroundOrEraser(Editor& editor, GroundBrush* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw) {
		drawGroundOrEraserImpl(editor, brush, tilestodraw, tilestoborder, alt, dodraw);
	}

	void drawGroundOrEraser(Editor& editor, EraserBrush* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw) {
		drawGroundOrEraserImpl(editor, brush, tilestodraw, tilestoborder, alt, dodraw);
	}

	void drawWall(Editor& editor, WallBrush* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());

		if (alt && dodraw) {
			// This is exempt from USE_AUTOMAGIC
			g_doodad_preview.GetBufferMap()->clear();
			BaseMap* draw_map = g_doodad_preview.GetBufferMap();

			for (const auto& drawPos : tilestodraw) {
				auto* location = editor.map.createTileL(drawPos);
				auto* tile = location->get();
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					TileOperations::cleanWalls(new_tile.get(), brush);
					brush->draw(draw_map, new_tile.get(), nullptr);
					(void)draw_map->setTile(drawPos, std::move(new_tile));
				} else {
					std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
					brush->draw(draw_map, new_tile.get(), nullptr);
					(void)draw_map->setTile(drawPos, std::move(new_tile));
				}
			}
			// Iterate over the map instead of tilestodraw to avoid duplicates!
			for (MapIterator it = draw_map->begin(); it != draw_map->end(); ++it) {
				Tile* tile = it->get();
				if (tile) {
					TileOperations::wallize(tile, draw_map);
					action->addChange(std::make_unique<Change>(std::unique_ptr<Tile>(tile)));
				}
			}
			draw_map->clear(false);
			// Commit
			batch->addAndCommitAction(std::move(action));
		} else {
			for (const auto& drawPos : tilestodraw) {
				auto* location = editor.map.createTileL(drawPos);
				auto* tile = location->get();
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					// Wall cleaning is exempt from automagic
					TileOperations::cleanWalls(new_tile.get(), brush->as<WallBrush>());
					if (dodraw) {
						brush->draw(&editor.map, new_tile.get(), nullptr);
					} else {
						brush->undraw(&editor.map, new_tile.get());
					}
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				} else if (dodraw) {
					std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
					brush->draw(&editor.map, new_tile.get(), nullptr);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			}

			// Commit changes to map
			batch->addAndCommitAction(std::move(action));

			if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
				// Do borders!
				action = editor.actionQueue->createAction(batch.get());
				for (const auto& borderPos : tilestoborder) {
					Tile* tile = editor.map.getTile(borderPos);
					if (tile) {
						std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
						TileOperations::wallize(new_tile.get(), &editor.map);
						// if(*tile == *new_tile) delete new_tile;
						action->addChange(std::make_unique<Change>(std::move(new_tile)));
					}
				}
				batch->addAndCommitAction(std::move(action));
			}
		}

		editor.addBatch(std::move(batch), 2);
	}

} // namespace

void DrawOperations::draw(Editor& editor, Position offset, bool alt, bool dodraw) {
	Brush* brush = g_gui.GetCurrentBrush();
	if (!brush) {
		return;
	}

	if (brush->is<DoodadBrush>()) {
		drawDoodad(editor, brush->as<DoodadBrush>(), offset, alt, dodraw);
	} else if (brush->is<HouseExitBrush>()) {
		HouseExitBrush* house_exit_brush = brush->as<HouseExitBrush>();
		if (!house_exit_brush->canDraw(&editor.map, offset)) {
			return;
		}

		House* house = editor.map.houses.getHouse(house_exit_brush->getHouseID());
		if (!house) {
			return;
		}

		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());
		action->addChange(std::unique_ptr<Change>(Change::Create(house, offset)));
		batch->addAndCommitAction(std::move(action));
		editor.addBatch(std::move(batch), 2);
	} else if (brush->is<WaypointBrush>()) {
		WaypointBrush* waypoint_brush = brush->as<WaypointBrush>();
		if (!waypoint_brush->canDraw(&editor.map, offset)) {
			return;
		}

		Waypoint* waypoint = editor.map.waypoints.getWaypoint(waypoint_brush->getWaypoint());
		if (!waypoint || waypoint->pos == offset) {
			return;
		}

		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());
		action->addChange(std::unique_ptr<Change>(Change::Create(waypoint, offset)));
		batch->addAndCommitAction(std::move(action));
		editor.addBatch(std::move(batch), 2);
	} else if (brush->is<WallBrush>()) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());
		// This will only occur with a size 0, when clicking on a tile (not drawing)
		Tile* tile = editor.map.getTile(offset);
		std::unique_ptr<Tile> new_tile;
		if (tile) {
			new_tile = TileOperations::deepCopy(tile, editor.map);
		} else {
			new_tile = editor.map.allocator(editor.map.createTileL(offset));
		}

		if (dodraw) {
			bool b = true;
			brush->as<WallBrush>()->draw(&editor.map, new_tile.get(), &b);
		} else {
			brush->as<WallBrush>()->undraw(&editor.map, new_tile.get());
		}
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		batch->addAndCommitAction(std::move(action));
		editor.addBatch(std::move(batch), 2);
	} else if (brush->is<SpawnBrush>() || brush->is<CreatureBrush>()) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());

		Tile* tile = editor.map.getTile(offset);
		std::unique_ptr<Tile> new_tile;
		if (tile) {
			new_tile = TileOperations::deepCopy(tile, editor.map);
		} else {
			new_tile = editor.map.allocator(editor.map.createTileL(offset));
		}
		int param;
		if (!brush->is<CreatureBrush>()) {
			param = g_gui.GetBrushSize();
		}
		if (dodraw) {
			brush->draw(&editor.map, new_tile.get(), &param);
		} else {
			brush->undraw(&editor.map, new_tile.get());
		}
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		batch->addAndCommitAction(std::move(action));
		editor.addBatch(std::move(batch), 2);
	}
}

void DrawOperations::draw(Editor& editor, const PositionVector& tilestodraw, bool alt, bool dodraw) {
	Brush* brush = g_gui.GetCurrentBrush();
	if (!brush) {
		return;
	}

#ifdef __DEBUG__
	if (brush->is<GroundBrush>() || brush->is<WallBrush>()) {
		// Wrong function, end call
		return;
	}
#endif

	std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_DRAW);

	if (brush->is<OptionalBorderBrush>()) {
		// We actually need to do borders, but on the same tiles we draw to
		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				if (dodraw) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					brush->draw(&editor.map, new_tile.get());
					TileOperations::borderize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				} else if (!dodraw && tile->hasOptionalBorder()) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					brush->undraw(&editor.map, new_tile.get());
					TileOperations::borderize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				brush->draw(&editor.map, new_tile.get());
				TileOperations::borderize(new_tile.get(), &editor.map);
				if (new_tile->empty()) {
					continue;
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}
	} else {

		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				if (dodraw) {
					brush->draw(&editor.map, new_tile.get(), &alt);
				} else {
					brush->undraw(&editor.map, new_tile.get());
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				brush->draw(&editor.map, new_tile.get(), &alt);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}
	}
	editor.addAction(std::move(action), 2);
}

void DrawOperations::draw(Editor& editor, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw) {
	Brush* brush = g_gui.GetCurrentBrush();
	if (!brush) {
		return;
	}

	if (brush->is<GroundBrush>()) {
		drawGroundOrEraser(editor, brush->as<GroundBrush>(), tilestodraw, tilestoborder, alt, dodraw);
	} else if (brush->is<EraserBrush>()) {
		drawGroundOrEraser(editor, brush->as<EraserBrush>(), tilestodraw, tilestoborder, alt, dodraw);
	} else if (brush->is<TableBrush>() || brush->is<CarpetBrush>()) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());

		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				if (dodraw) {
					brush->draw(&editor.map, new_tile.get(), nullptr);
				} else {
					brush->undraw(&editor.map, new_tile.get());
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				brush->draw(&editor.map, new_tile.get(), nullptr);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}

		// Commit changes to map
		batch->addAndCommitAction(std::move(action));

		// Do borders!
		action = editor.actionQueue->createAction(batch.get());
		for (const auto& borderPos : tilestoborder) {
			Tile* tile = editor.map.getTile(borderPos);
			if (brush->is<TableBrush>()) {
				if (tile && tile->hasTable()) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					TileOperations::tableize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			} else if (brush->is<CarpetBrush>()) {
				if (tile && tile->hasCarpet()) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					TileOperations::carpetize(new_tile.get(), &editor.map);
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			}
		}
		batch->addAndCommitAction(std::move(action));

		editor.addBatch(std::move(batch), 2);
	} else if (brush->is<WallBrush>()) {
		drawWall(editor, brush->as<WallBrush>(), tilestodraw, tilestoborder, alt, dodraw);
	} else if (brush->is<DoorBrush>()) {
		std::unique_ptr<BatchAction> batch = editor.actionQueue->createBatch(ACTION_DRAW);
		std::unique_ptr<Action> action = editor.actionQueue->createAction(batch.get());
		DoorBrush* door_brush = brush->as<DoorBrush>();

		// Loop is kind of redundant since there will only ever be one index.
		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				// Wall cleaning is exempt from automagic
				if (brush->is<WallBrush>()) {
					TileOperations::cleanWalls(new_tile.get(), brush->as<WallBrush>());
				}
				if (dodraw) {
					door_brush->draw(&editor.map, new_tile.get(), &alt);
				} else {
					door_brush->undraw(&editor.map, new_tile.get());
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				door_brush->draw(&editor.map, new_tile.get(), &alt);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}

		// Commit changes to map
		batch->addAndCommitAction(std::move(action));

		if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
			// Do borders!
			action = editor.actionQueue->createAction(batch.get());
			for (const auto& borderPos : tilestoborder) {
				Tile* tile = editor.map.getTile(borderPos);
				if (tile) {
					std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
					TileOperations::wallize(new_tile.get(), &editor.map);
					// if(*tile == *new_tile) delete new_tile;
					action->addChange(std::make_unique<Change>(std::move(new_tile)));
				}
			}
			batch->addAndCommitAction(std::move(action));
		}

		editor.addBatch(std::move(batch), 2);
	} else {
		std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_DRAW);
		for (const auto& drawPos : tilestodraw) {
			auto* location = editor.map.createTileL(drawPos);
			auto* tile = location->get();
			if (tile) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				if (dodraw) {
					brush->draw(&editor.map, new_tile.get());
				} else {
					brush->undraw(&editor.map, new_tile.get());
				}
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (dodraw) {
				std::unique_ptr<Tile> new_tile(editor.map.allocator(location));
				brush->draw(&editor.map, new_tile.get());
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}
		editor.addAction(std::move(action), 2);
	}
}
