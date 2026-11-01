//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/tile_properties_panel.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "ui/tile_properties/browse_field_list.h"
#include "ui/tile_properties/spawn_creature_panel.h"
#include "ui/tile_properties/map_flags_panel.h"
#include "ui/tile_properties/item_property_panel.h"
#include "ui/tile_properties/container_property_panel.h"
#include "ui/tile_properties/depot_property_panel.h"
#include "ui/tile_properties/teleport_property_panel.h"
#include "ui/tile_properties/door_property_panel.h"
#include "ui/tile_properties/spawn_property_panel.h"
#include "ui/tile_properties/creature_property_panel.h"
#include "map/tile.h"
#include "game/spawn.h"
#include "game/creature.h"

TilePropertiesPanel::TilePropertiesPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize),
	current_tile(nullptr),
	current_map(nullptr) {

	splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);

	left_panel = new wxPanel(splitter);
	left_sizer = new wxBoxSizer(wxVERTICAL);
	browse_field_list = new BrowseFieldList(left_panel);
	browse_field_list->SetOnItemSelectedCallback([this](Item* item) {
		OnItemSelected(item);
	});
	left_sizer->Add(browse_field_list, wxSizerFlags(1).Expand().Border(wxALL, 2));

	spawn_creature_panel = new SpawnCreaturePanel(left_panel);
	spawn_creature_panel->SetOnSpawnSelectedCallback([this]() {
		OnSpawnSelected();
	});
	spawn_creature_panel->SetOnCreatureSelectedCallback([this]() {
		OnCreatureSelected();
	});
	left_sizer->Add(spawn_creature_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	left_panel->SetSizer(left_sizer);

	right_panel = new wxPanel(splitter);
	right_sizer = new wxBoxSizer(wxVERTICAL);

	map_flags_panel = new MapFlagsPanel(right_panel);
	right_sizer->Add(map_flags_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	item_property_panel = new ItemPropertyPanel(right_panel);
	right_sizer->Add(item_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	container_property_panel = new ContainerPropertyPanel(right_panel);
	right_sizer->Add(container_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	depot_property_panel = new DepotPropertyPanel(right_panel);
	right_sizer->Add(depot_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	teleport_property_panel = new TeleportPropertyPanel(right_panel);
	right_sizer->Add(teleport_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	door_property_panel = new DoorPropertyPanel(right_panel);
	right_sizer->Add(door_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	spawn_property_panel = new SpawnPropertyPanel(right_panel);
	right_sizer->Add(spawn_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	creature_property_panel = new CreaturePropertyPanel(right_panel);
	right_sizer->Add(creature_property_panel, wxSizerFlags(0).Expand().Border(wxALL, 2));

	placeholder_text = new wxStaticText(right_panel, wxID_ANY, "Select an item to view properties");
	right_sizer->Add(placeholder_text, wxSizerFlags(1).Center().Border(wxALL, 10));
	right_panel->SetSizer(right_sizer);

	splitter->SplitVertically(left_panel, right_panel, FromDIP(250));
	splitter->SetMinimumPaneSize(FromDIP(150));

	HideAllPropertyPanels();
	placeholder_text->Show();

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(splitter, wxSizerFlags(1).Expand());
	SetSizer(main_sizer);
}

TilePropertiesPanel::~TilePropertiesPanel() {
}

void TilePropertiesPanel::SetTile(Tile* tile, Map* map) {
	current_tile = tile;
	current_map = map;

	browse_field_list->SetTile(tile, map);
	spawn_creature_panel->SetTile(tile);
	map_flags_panel->SetTile(tile, map);

	if (tile) {
		placeholder_text->SetLabelText(wxstr(std::format("Selected Tile: {}, {}, {}", tile->getX(), tile->getY(), tile->getZ())));
	} else {
		placeholder_text->SetLabelText("No tile selected");
		HideAllPropertyPanels();
	}
	right_panel->Layout();
}

void TilePropertiesPanel::SelectItem(Item* item) {
	if (browse_field_list) {
		browse_field_list->SelectItem(item);
	}
	OnItemSelected(item);
}

void TilePropertiesPanel::OnItemSelected(Item* item) {
	HideAllPropertyPanels(); // Use centralized hiding

	if (item) {
		if (item->asDepot()) {
			depot_property_panel->SetItem(item, current_tile, current_map);
			depot_property_panel->Show();
		} else if (item->asTeleport()) {
			teleport_property_panel->SetItem(item, current_tile, current_map);
			teleport_property_panel->Show();
		} else if (item->asDoor()) {
			door_property_panel->SetItem(item, current_tile, current_map);
			door_property_panel->Show();
		} else if (item->asContainer()) {
			container_property_panel->SetItem(item, current_tile, current_map);
			container_property_panel->Show();
		} else {
			item_property_panel->SetItem(item, current_tile, current_map);
			item_property_panel->Show();
		}
	} else {
		placeholder_text->Show();
	}
	right_panel->Layout();
}

void TilePropertiesPanel::OnSpawnSelected() {
	HideAllPropertyPanels();

	if (current_tile && current_tile->spawn) {
		spawn_property_panel->SetSpawn(current_tile->spawn.get(), current_tile, current_map);
		spawn_property_panel->Show();
	} else {
		placeholder_text->Show();
	}
	right_panel->Layout();
}

void TilePropertiesPanel::OnCreatureSelected() {
	HideAllPropertyPanels();

	if (current_tile && current_tile->creature) {
		creature_property_panel->SetCreature(current_tile->creature.get(), current_tile, current_map);
		creature_property_panel->Show();
	} else {
		placeholder_text->Show();
	}
	right_panel->Layout();
}

void TilePropertiesPanel::HideAllPropertyPanels() {
	item_property_panel->Hide();
	container_property_panel->Hide();
	depot_property_panel->Hide();
	teleport_property_panel->Hide();
	door_property_panel->Hide();
	spawn_property_panel->Hide();
	creature_property_panel->Hide();
	placeholder_text->Hide();
}

void TilePropertiesPanel::UpdateFromEditor(Editor* editor) {
	if (!editor) {
		return;
	}

	if (editor->selection.size() == 1) {
		Tile* tile = editor->selection.getSelectedTile();
		SetTile(tile, &editor->map);

		if (tile) {
			ItemVector items = TileOperations::getSelectedItems(tile);
			if (!items.empty()) {
				SelectItem(items.front());
			} else if (tile->creature && tile->creature->isSelected()) {
				OnCreatureSelected();
			} else if (tile->spawn && tile->spawn->isSelected()) {
				OnSpawnSelected();
			} else {
				SelectItem(nullptr);
			}
		}
	} else {
		SetTile(nullptr, nullptr);
	}
}
