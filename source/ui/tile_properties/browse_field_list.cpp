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
#include "util/nanovg_listbox.h"
#include "rendering/core/graphics.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include <glad/glad.h>
#include <nanovg.h>
#include <format>
#include <vector>

#include "game/item.h"

class TilePropertiesListBox : public NanoVGListBox {
public:
	TilePropertiesListBox(wxWindow* parent, wxWindowID id);
	~TilePropertiesListBox();

	void SetTile(Tile* tile);
	void SelectItem(Item* item);
	void UpdateItems();

	void OnDrawItem(NVGcontext* vg, const wxRect& rect, size_t index) override;
	int OnMeasureItem(size_t index) const override;
	Item* GetItem(size_t index) const;

protected:
	std::vector<Item*> items;
	Tile* current_tile;
};

TilePropertiesListBox::TilePropertiesListBox(wxWindow* parent, wxWindowID id) :
	NanoVGListBox(parent, id, wxLB_SINGLE), current_tile(nullptr) {
	SetMinSize(FromDIP(wxSize(200, 180)));
}

TilePropertiesListBox::~TilePropertiesListBox() {
}

void TilePropertiesListBox::SetTile(Tile* tile) {
	current_tile = tile;
	UpdateItems();
}

void TilePropertiesListBox::SelectItem(Item* item) {
	for (size_t i = 0; i < items.size(); ++i) {
		if (items[i] == item) {
			SetSelection(i);
			return;
		}
	}
	SetSelection(wxNOT_FOUND);
}

void TilePropertiesListBox::UpdateItems() {
	items.clear();
	if (current_tile) {
		items.reserve(current_tile->items.size() + (current_tile->ground ? 1 : 0));
		if (current_tile->ground) {
			items.push_back(current_tile->ground.get());
		}
		for (auto it = current_tile->items.begin(); it != current_tile->items.end(); ++it) {
			items.push_back(it->get());
		}
	}
	SetItemCount(items.size());
	Refresh();
}

void TilePropertiesListBox::OnDrawItem(NVGcontext* vg, const wxRect& rect, size_t n) {
	if (n >= items.size()) {
		return;
	}
	Item* item = items[n];

	const auto tech = rme::rendering::TechnicalItemRegistry::Classify(item->getID(), item->getClientID());
	if (tech != rme::rendering::TileIndicatorType::None) {
		const int icon_size = 32;
		rme::rendering::DrawNanoVGIndicatorBadge(
			vg,
			tech,
			static_cast<float>(rect.x),
			static_cast<float>(rect.y),
			static_cast<float>(icon_size)
		);
	} else {
		Sprite* sprite = g_gui.gfx.getSprite(item->getClientID());
		if (sprite) {
			int tex = GetOrCreateSpriteTexture(vg, sprite);
			if (tex > 0) {
				int icon_size = 32;
				NVGpaint imgPaint = nvgImagePattern(vg, rect.x, rect.y, icon_size, icon_size, 0, tex, 1.0f);
				nvgBeginPath(vg);
				nvgRect(vg, rect.x, rect.y, icon_size, icon_size);
				nvgFillPaint(vg, imgPaint);
				nvgFill(vg);
			}
		}
	}

	if (IsSelected(n)) {
		wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
	} else {
		wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
	}

	std::string label = std::format("{} - {}", item->getID(), item->getName());

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, rect.x + 40, rect.y + rect.height / 2.0f, label.c_str(), nullptr);
}

int TilePropertiesListBox::OnMeasureItem(size_t n) const {
	return FromDIP(32);
}

Item* TilePropertiesListBox::GetItem(size_t index) const {
	if (index < items.size()) {
		return items[index];
	}
	return nullptr;
}

// ----------------------------------------------------------------------------

BrowseFieldList::BrowseFieldList(wxWindow* parent) :
	wxPanel(parent, wxID_ANY), current_tile(nullptr), current_map(nullptr) {

	wxStaticBoxSizer* main_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Items");

	// Toolbar
	wxBoxSizer* toolbar_sizer = newd wxBoxSizer(wxHORIZONTAL);
	btn_up = newd wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_up->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_ARROW_UP, wxSize(16, 16)));
	btn_up->SetToolTip("Move item up in stack");

	btn_down = newd wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_down->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_ARROW_DOWN, wxSize(16, 16)));
	btn_down->SetToolTip("Move item down in stack");

	btn_delete = newd wxButton(main_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_delete->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, wxSize(16, 16)));
	btn_delete->SetToolTip("Delete item");

	toolbar_sizer->Add(btn_up, wxSizerFlags(0).Border(wxRIGHT, 2));
	toolbar_sizer->Add(btn_down, wxSizerFlags(0).Border(wxRIGHT, 2));
	toolbar_sizer->AddStretchSpacer(1);
	toolbar_sizer->Add(btn_delete, wxSizerFlags(0));

	main_sizer->Add(toolbar_sizer, wxSizerFlags(0).Expand().Border(wxALL, 2));

	// Listbox
	item_list = newd TilePropertiesListBox(main_sizer->GetStaticBox(), wxID_ANY);
	main_sizer->Add(item_list, wxSizerFlags(1).Expand());

	SetSizer(main_sizer);

	// Events
	item_list->Bind(wxEVT_LISTBOX, &BrowseFieldList::OnItemSelected, this);
	btn_up->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickUp, this);
	btn_down->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickDown, this);
	btn_delete->Bind(wxEVT_BUTTON, &BrowseFieldList::OnClickDelete, this);
}

BrowseFieldList::~BrowseFieldList() {
}

void BrowseFieldList::SetTile(Tile* tile, Map* map) {
	current_tile = tile;
	current_map = map;
	item_list->SetTile(tile);

	// Reset UI state
	btn_up->Enable(false);
	btn_down->Enable(false);
	btn_delete->Enable(false);
}

void BrowseFieldList::SelectItem(Item* item) {
	item_list->SelectItem(item);
	int selection = item_list->GetSelection();
	if (selection != wxNOT_FOUND) {
		btn_up->Enable(selection > 0);
		btn_down->Enable(selection < item_list->GetItemCount() - 1);
		btn_delete->Enable(true);
	} else {
		btn_up->Enable(false);
		btn_down->Enable(false);
		btn_delete->Enable(false);
	}
}

void BrowseFieldList::OnItemSelected(wxCommandEvent& event) {
	int selection = item_list->GetSelection();
	Item* selected_item = nullptr;
	if (selection != wxNOT_FOUND) {
		btn_up->Enable(selection > 0);
		btn_down->Enable(selection < item_list->GetItemCount() - 1);
		btn_delete->Enable(true);
		selected_item = item_list->GetItem(selection);
	} else {
		btn_up->Enable(false);
		btn_down->Enable(false);
		btn_delete->Enable(false);
	}

	if (on_item_selected_cb) {
		on_item_selected_cb(selected_item);
	}

	// Issue #3: Sync selection with the editor
	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile && selected_item) {
		editor->selection.start(Selection::INTERNAL);
		editor->selection.clear();
		editor->selection.add(current_tile, selected_item);
		editor->selection.finish(Selection::INTERNAL);
	}

	event.Skip(); // Let parent handle property panel swap if needed
}

void BrowseFieldList::OnClickUp(wxCommandEvent& event) {
	int selection = item_list->GetSelection();
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
		Position pos = current_tile->getPosition();
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		std::swap(new_tile->items[index_in_items], new_tile->items[index_in_items - 1]);
		new_tile->items[index_in_items - 1]->select();

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}

void BrowseFieldList::OnClickDown(wxCommandEvent& event) {
	int selection = item_list->GetSelection();
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
	int selection = item_list->GetSelection();
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
		Position pos = current_tile->getPosition();
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
