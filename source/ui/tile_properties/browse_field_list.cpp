//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/browse_field_list.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "ui/tile_properties/tile_properties_panel.h"
#include "util/image_manager.h"
#include "util/virtual_item_grid.h"
#include "palette/controls/tileset_grid_toolbar.h"

#include "game/item.h"

#include <format>
#include <optional>
#include <utility>
#include <vector>

class TilePropertiesItemGrid : public VirtualItemGrid {
public:
	explicit TilePropertiesItemGrid(wxWindow* parent) :
		VirtualItemGrid(parent, wxID_ANY) { }

	void SetTile(Tile* tile) {
		current_tile = tile;
		RebuildItems();
	}

	void SelectItem(Item* item) {
		for (size_t i = 0; i < items.size(); ++i) {
			if (items[i] == item) {
				SetSelection(static_cast<int>(i));
				return;
			}
		}
		SetSelection(-1);
	}

	void SetOnItemSelectedCallback(std::function<void(Item*)> cb) {
		on_item_selected_cb = std::move(cb);
	}

	size_t GetItemCount() const override {
		return items.size();
	}

	uint16_t GetItem(size_t index) const override {
		return index < items.size() ? items[index]->getID() : 0;
	}

	wxString GetItemName(size_t index) const override {
		if (index >= items.size()) {
			return {};
		}
		const Item* item = items[index];
		return wxstr(std::format("{} - {}", item->getID(), item->getName()));
	}

	wxString GetItemTooltip(size_t index) const override {
		return GetItemName(index);
	}

	std::optional<uint16_t> GetItemSubtype(size_t index) const override {
		if (index >= items.size()) {
			return std::nullopt;
		}
		return items[index]->getSubtype();
	}

	void OnItemSelected(int index) override {
		if (on_item_selected_cb) {
			on_item_selected_cb(index >= 0 && index < static_cast<int>(items.size()) ? items[index] : nullptr);
		}
	}

	wxSize DoGetBestClientSize() const override {
		return FromDIP(wxSize(180, 300));
	}

protected:
	void RebuildItems() {
		items.clear();
		if (current_tile) {
			items.reserve(current_tile->items.size() + (current_tile->ground ? 1 : 0));
			if (current_tile->ground) {
				items.push_back(current_tile->ground.get());
			}
			for (const auto& item : current_tile->items) {
				items.push_back(item.get());
			}
		}
		SetSelection(-1);
		RefreshGrid();
	}

	Tile* current_tile = nullptr;
	std::vector<Item*> items;
	std::function<void(Item*)> on_item_selected_cb;
};

// ----------------------------------------------------------------------------

BrowseFieldList::BrowseFieldList(wxWindow* parent) :
	wxPanel(parent, wxID_ANY), current_tile(nullptr), current_map(nullptr) {

	wxStaticBoxSizer* main_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Items");

	wxBoxSizer* toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);
	btn_up = new wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_up->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ARROW_UP));
	btn_up->SetToolTip("Move item up in stack");

	btn_down = new wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_down->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ARROW_DOWN));
	btn_down->SetToolTip("Move item down in stack");

	btn_delete = new wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_delete->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_TRASH_CAN));
	btn_delete->SetToolTip("Delete item");

	toolbar_sizer->Add(btn_up, wxSizerFlags(0).Border(wxRIGHT, 2));
	toolbar_sizer->Add(btn_down, wxSizerFlags(0).Border(wxRIGHT, 2));
	toolbar_sizer->AddStretchSpacer(1);
	toolbar_sizer->Add(btn_delete, wxSizerFlags(0));

	main_sizer->Add(toolbar_sizer, wxSizerFlags(0).Expand().Border(wxALL, 2));

	TilesetGridToolbar::Actions actions;
	actions.show_labels = [this](bool show) {
		item_grid->SetShowLabels(show);
	};
	actions.icon_size = [this](int size) {
		if (item_grid->IsListMode()) {
			item_grid->SetListMode(false);
		}
		item_grid->SetIconSize(size);
		grid_toolbar->SetState(item_grid->GetShowLabels(), size, false, TilesetSortKey::ID, TilesetSortDirection::Ascending, false, false);
	};
	actions.list_mode = [this](bool list) {
		item_grid->SetListMode(list);
		grid_toolbar->SetState(item_grid->GetShowLabels(), item_grid->GetIconSize(), false, TilesetSortKey::ID, TilesetSortDirection::Ascending, false, list);
	};
	actions.grid_spacing = [this](int spacing) {
		item_grid->SetGridSpacing(spacing);
	};

	grid_toolbar = new TilesetGridToolbar(
		main_sizer->GetStaticBox(),
		std::move(actions),
		TilesetGridToolbar::Part_Labels | TilesetGridToolbar::Part_Size | TilesetGridToolbar::Part_Spacing
	);
	grid_toolbar->SetGridSpacing(4);
	grid_toolbar->SetState(false, 32, false, TilesetSortKey::ID, TilesetSortDirection::Ascending, false, false);
	grid_toolbar->ApplyTheme();
	main_sizer->Add(grid_toolbar, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 2));

	item_grid = new TilePropertiesItemGrid(main_sizer->GetStaticBox());
	item_grid->SetBrushIndicators(false);
	main_sizer->Add(item_grid, wxSizerFlags(1).Expand());

	SetSizer(main_sizer);

	item_grid->SetOnItemSelectedCallback([this](Item* item) {
		OnItemSelected(item);
	});
	btn_up->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickUp, this);
	btn_down->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickDown, this);
	btn_delete->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickDelete, this);
}

BrowseFieldList::~BrowseFieldList() {
}

void BrowseFieldList::SetTile(Tile* tile, Map* map) {
	current_tile = tile;
	current_map = map;
	item_grid->SetTile(tile);
	UpdateButtons();
}

void BrowseFieldList::SelectItem(Item* item) {
	item_grid->SelectItem(item);
	UpdateButtons();
}

void BrowseFieldList::OnItemSelected(Item* item) {
	UpdateButtons();

	if (on_item_selected_cb) {
		on_item_selected_cb(item);
	}

	// Issue #3: Sync selection with the editor
	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile && item) {
		editor->selection.start(Selection::INTERNAL);
		editor->selection.clear();
		editor->selection.add(current_tile, item);
		editor->selection.finish(Selection::INTERNAL);
	}
}

void BrowseFieldList::UpdateButtons() {
	const int selection = item_grid->GetSelection();
	const int count = static_cast<int>(item_grid->GetItemCount());
	const int first_item_index = (current_tile && current_tile->ground) ? 1 : 0;

	btn_up->Enable(selection > first_item_index);
	btn_down->Enable(selection >= first_item_index && selection < count - 1);
	btn_delete->Enable(selection >= first_item_index);
}

void BrowseFieldList::OnClickUp(wxCommandEvent& event) {
	int selection = item_grid->GetSelection();
	if (selection == wxNOT_FOUND || selection <= 0) {
		return;
	}

	if (!current_tile) {
		return;
	}
	int index_in_items = selection - (current_tile->ground ? 1 : 0);
	if (index_in_items <= 0) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		std::swap(new_tile->items[index_in_items], new_tile->items[index_in_items - 1]);
		new_tile->items[index_in_items - 1]->select();

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}

void BrowseFieldList::OnClickDown(wxCommandEvent& event) {
	int selection = item_grid->GetSelection();
	if (selection == wxNOT_FOUND) {
		return;
	}

	if (!current_tile) {
		return;
	}
	int index_in_items = selection - (current_tile->ground ? 1 : 0);
	if (index_in_items < 0 || index_in_items >= (int)current_tile->items.size() - 1) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		std::swap(new_tile->items[index_in_items], new_tile->items[index_in_items + 1]);
		new_tile->items[index_in_items + 1]->select();

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}

void BrowseFieldList::OnClickDelete(wxCommandEvent& event) {
	int selection = item_grid->GetSelection();
	if (selection == wxNOT_FOUND) {
		return;
	}

	if (current_tile->ground && selection == 0) {
		return;
	}

	int index_in_items = selection - (current_tile->ground ? 1 : 0);
	if (index_in_items < 0 || index_in_items >= (int)current_tile->items.size()) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		new_tile->items.erase(new_tile->items.begin() + index_in_items);

		if (index_in_items > 0) {
			new_tile->items[index_in_items - 1]->select();
		} else if (new_tile->ground) {
			new_tile->ground->select();
		} else if (!new_tile->items.empty()) {
			new_tile->items[0]->select();
		}

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}
