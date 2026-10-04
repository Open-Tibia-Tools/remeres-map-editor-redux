//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/container_property_panel.h"
#include "ui/properties/container_properties_window.h" // For ContainerItemButton
#include "game/complexitem.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/dialog_util.h"
#include "ui/find_item_window.h"
#include "ui/gui_ids.h"
#include "util/image_manager.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "ui/properties/item_properties_window.h"
#include "app/settings.h"

ContainerPropertyPanel::ContainerPropertyPanel(wxWindow* parent) :
	ItemPropertyPanel(parent), grid_canvas(nullptr) {

	grid_canvas = newd ContainerGridCanvas(this, g_settings.getBoolean(Config::USE_LARGE_CONTAINER_ICONS));
	grid_canvas->Bind(wxEVT_BUTTON, &ContainerPropertyPanel::OnContainerItemClick, this);
	grid_canvas->Bind(wxEVT_CONTEXT_MENU, &ContainerPropertyPanel::OnContainerItemRightClick, this);

	// Add grid_canvas to the main sizer (inherited from ItemPropertyPanel)
	GetSizer()->Add(grid_canvas, wxSizerFlags(1).Expand().Border(wxALL, 5));

	// Context menu events
	Bind(wxEVT_MENU, &ContainerPropertyPanel::OnAddItem, this, CONTAINER_POPUP_MENU_ADD);
	Bind(wxEVT_MENU, &ContainerPropertyPanel::OnEditItem, this, CONTAINER_POPUP_MENU_EDIT);
	Bind(wxEVT_MENU, &ContainerPropertyPanel::OnRemoveItem, this, CONTAINER_POPUP_MENU_REMOVE);
}

ContainerPropertyPanel::~ContainerPropertyPanel() {
}

void ContainerPropertyPanel::SetItem(Item* item, Tile* tile, Map* map) {
	// Call base class to show Action ID / Unique ID / etc.
	ItemPropertyPanel::SetItem(item, tile, map);

	RebuildGrid();
}

void ContainerPropertyPanel::RebuildGrid() {
	if (grid_canvas) {
		grid_canvas->SetContainer(current_item);
		// Update icon scaling dynamically explicitly since properties can update
		grid_canvas->SetLargeSprites(g_settings.getBoolean(Config::USE_LARGE_CONTAINER_ICONS));
	}
	Layout();
}

void ContainerPropertyPanel::OnContainerItemClick(wxCommandEvent& event) {
	int index = event.GetInt();

	Container* container = current_item ? dynamic_cast<Container*>(current_item) : nullptr;
	if (!container || !current_tile || !current_map) {
		return;
	}

	if (index < static_cast<int>(container->getItemCount())) {
		OnEditItem(event);
	} else {
		OnAddItem(event);
	}
}

void ContainerPropertyPanel::OnContainerItemRightClick(wxContextMenuEvent& event) {
	int index = grid_canvas->GetSelectedIndex();

	Container* container = current_item ? dynamic_cast<Container*>(current_item) : nullptr;
	if (!container) {
		return;
	}

	wxMenu menu;
	if (index < static_cast<int>(container->getItemCount())) {
		menu.Append(CONTAINER_POPUP_MENU_EDIT, "&Edit Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN_TO_SQUARE, wxSize(16, 16)));
		menu.Append(CONTAINER_POPUP_MENU_ADD, "&Add Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
		menu.Append(CONTAINER_POPUP_MENU_REMOVE, "&Remove Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	} else {
		menu.Append(CONTAINER_POPUP_MENU_ADD, "&Add Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	}

	if (container->getVolume() <= (int)container->getVector().size()) {
		if (wxMenuItem* addItem = menu.FindItem(CONTAINER_POPUP_MENU_ADD)) {
			addItem->Enable(false);
		}
	}

	PopupMenu(&menu);
}

void ContainerPropertyPanel::OnAddItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !current_tile || !current_map) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	FindItemDialog dialog(this, "Select Item");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t item_id = dialog.getResultID();
		if (item_id != 0) {
			std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
			int index = current_tile->getIndexOf(current_item);
			if (index != -1) {
				Item* new_item_base = new_tile->getItemAt(index);
				if (new_item_base && new_item_base->asContainer()) {
					Container* container = static_cast<Container*>(new_item_base);
					auto& contents = container->getVector();
					uint32_t sub_index = grid_canvas->GetSelectedIndex();

					std::unique_ptr<Item> new_sub_item(Item::Create(item_id));
					if (new_sub_item) {
						if (sub_index < contents.size()) {
							contents.insert(contents.begin() + sub_index, std::move(new_sub_item));
						} else {
							contents.push_back(std::move(new_sub_item));
						}

						std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
						action->addChange(std::make_unique<Change>(std::move(new_tile)));
						editor->addAction(std::move(action));
					}
				}
			}
			RebuildGrid();
		}
	}
}

void ContainerPropertyPanel::OnEditItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !current_map || !current_tile || !current_item) {
		return;
	}

	int sub_item_index = grid_canvas->GetSelectedIndex();
	if (sub_item_index == -1) {
		return;
	}

	Container* container = current_item->asContainer();
	if (!container || static_cast<size_t>(sub_item_index) >= container->getItemCount()) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	Item* sub_item = container->getItem(sub_item_index);

	// Create a deep copy of the tile to edit items inside the container
	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
	int container_index = current_tile->getIndexOf(current_item);
	if (container_index == -1) {
		return;
	}

	Item* new_container_base = new_tile->getItemAt(container_index);
	Container* new_container = new_container_base ? new_container_base->asContainer() : nullptr;
	if (!new_container) {
		return;
	}

	Item* new_sub_item = new_container->getItem(sub_item_index);
	wxPoint newDialogAt = GetPosition() + FROM_DIP(this, wxPoint(20, 20));

	wxDialog* d = newd ItemPropertiesWindow(this, current_map, new_tile.get(), new_sub_item, newDialogAt);

	const int result = d->ShowModal();
	if (result == 1 || result == wxID_OK) {
		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
		// Selection change callback will trigger RebuildGrid
	}
	d->Destroy();
}

void ContainerPropertyPanel::OnRemoveItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !current_tile || !current_map) {
		return;
	}

	int sub_item_index = grid_canvas->GetSelectedIndex();
	if (sub_item_index == -1) {
		return;
	}

	Container* container = current_item ? current_item->asContainer() : nullptr;
	if (!container || static_cast<size_t>(sub_item_index) >= container->getItemCount()) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	int32_t ret = DialogUtil::PopupDialog(this, "Remove Item", "Are you sure you want to remove this item from the container?", wxYES | wxNO);

	if (ret != wxID_YES) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
	int index = current_tile->getIndexOf(current_item);
	if (index != -1) {
		Item* new_item_base = new_tile->getItemAt(index);
		if (new_item_base && new_item_base->asContainer()) {
			Container* container = static_cast<Container*>(new_item_base);
			auto& contents = container->getVector();
			// Since we know the index from the grid, let's use it.
			uint32_t sub_index = sub_item_index;
			if (sub_index < contents.size()) {
				contents.erase(contents.begin() + sub_index);

				std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				editor->addAction(std::move(action));
			}
		}
	}

	RebuildGrid();
}
