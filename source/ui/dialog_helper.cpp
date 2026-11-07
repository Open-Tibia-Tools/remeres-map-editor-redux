//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include <wx/wx.h>

#include "ui/dialog_helper.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/tile.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "ui/properties/creature_properties_window.h"
#include "ui/properties/spawn_properties_window.h"
#include "ui/properties/container_properties_window.h"
#include "ui/properties/podium_properties_window.h"
#include "ui/properties/writable_properties_window.h"
#include "ui/properties/splash_properties_window.h"
#include "ui/properties/depot_properties_window.h"
#include "ui/properties/item_properties_window.h"

wxDialog* DialogHelper::CreateItemPropertiesDialog(wxWindow* parent, const Map* map, const Tile* tile, Item* item, wxPoint pos) {
	if (!item) {
		return nullptr;
	}

	if (dynamic_cast<Container*>(item)) {
		return new ContainerPropertiesWindow(parent, map, tile, item, pos);
	} else if (dynamic_cast<Podium*>(item)) {
		return new PodiumPropertiesWindow(parent, map, tile, item, pos);
	} else if (item->canHoldText() || item->canHoldDescription()) {
		return new WritablePropertiesWindow(parent, map, tile, item, pos);
	} else if (item->isSplash() || item->isFluidContainer()) {
		return new SplashPropertiesWindow(parent, map, tile, item, pos);
	} else if (dynamic_cast<Depot*>(item)) {
		return new DepotPropertiesWindow(parent, map, tile, item, pos);
	} else {
		return new ItemPropertiesWindow(parent, map, tile, item, pos);
	}
}

void DialogHelper::OpenProperties(Editor& editor, Tile* tile) {
	if (!tile) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
	wxDialog* w = nullptr;

	if (new_tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
		w = new SpawnPropertiesWindow(g_gui.root, &editor.map, new_tile.get(), new_tile->spawn.get());
	} else if (new_tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
		w = new CreaturePropertiesWindow(g_gui.root, &editor.map, new_tile.get(), new_tile->creature.get());
	} else {
		ItemVector selected_items = TileOperations::getSelectedItems(new_tile.get());
		Item* item = nullptr;

		for (auto* it : selected_items) {
			if (it->isSelected()) {
				item = it;
				break;
			}
		}

		if (!item) {
			item = new_tile->getTopItem();
		}

		if (item) {
			w = CreateItemPropertiesDialog(g_gui.root, &editor.map, new_tile.get(), item);
		}
	}

	if (w) {
		int ret = w->ShowModal();
		if (ret != 0 && ret != wxID_CANCEL) {
			std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor.addAction(std::move(action));
		}
		w->Destroy();
	}
}
