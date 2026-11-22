#include "app/main.h"

#include "ui/context_menu_item_facts.h"

#include "brushes/brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "game/complexitem.h"
#include "game/item.h"

namespace ContextMenu {

	SelectedItemFacts CollectItemFacts(const Item& item) {
		const Teleport* teleport = dynamic_cast<const Teleport*>(&item);
		const Brush* doodad = item.getDoodadBrush();
		const WallBrush* wall = item.getWallBrush();
		const CarpetBrush* carpet = item.getCarpetBrush();
		const TableBrush* table = item.getTableBrush();
		const GroundBrush* ground = item.getGroundBrush();

		return SelectedItemFacts {
			.is_rotatable = item.isRoteable(),
			.is_teleport = teleport != nullptr,
			.teleport_has_destination = teleport && teleport->hasDestination(),
			.is_door = item.isDoor(),
			.is_open_door = item.isOpen(),
			.is_brush_door = item.isBrushDoor(),
			.has_door_brush = item.getDoorBrush() != nullptr,
			.has_visible_doodad_brush = doodad && doodad->visibleInPalette(),
			.has_collection_brush = item.hasCollectionBrush(),
			.has_visible_wall_brush = wall && wall->visibleInPalette(),
			.has_visible_carpet_brush = carpet && carpet->visibleInPalette(),
			.has_visible_table_brush = table && table->visibleInPalette(),
			.has_visible_ground_brush = ground && ground->visibleInPalette(),
			.has_raw_brush = item.getRAWBrush() != nullptr,
		};
	}

} // namespace ContextMenu
