//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "map/tile_stack_rows.h"
#include "ui/tile_properties/browse_field_list.h"
#include "ui/context_menu_item_facts.h"
#include "ui/item_context_actions.h"
#include "ui/item_context_menu_model.h"
#include "ui/item_popup_menu.h"
#include "ui/tile_item_actions.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
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

	int RowAtClientPoint(const wxPoint& point) const;
	// Client point a context menu for `row` opens at: the left edge of the row, vertically centred.
	wxPoint RowMenuAnchor(int row) const;

protected:
	std::vector<Item*> items;
	Tile* current_tile;
};

TilePropertiesListBox::TilePropertiesListBox(wxWindow* parent, wxWindowID id) :
	NanoVGListBox(parent, id, wxLB_SINGLE), current_tile(nullptr) {
	SetMinSize(FromDIP(wxSize(220, 180)));
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

	const std::string label = std::format("{} - {}", item->getID(), item->getName());

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

int TilePropertiesListBox::RowAtClientPoint(const wxPoint& point) const {
	return HitTest(point.x, point.y + GetScrollPosition());
}

wxPoint TilePropertiesListBox::RowMenuAnchor(int row) const {
	const wxRect rect = GetItemRect(row);
	return wxPoint(rect.GetLeft(), rect.GetTop() - GetScrollPosition() + rect.GetHeight() / 2);
}

// ----------------------------------------------------------------------------

BrowseFieldList::BrowseFieldList(wxWindow* parent) :
	wxPanel(parent, wxID_ANY), current_tile(nullptr), current_map(nullptr) {

	wxStaticBoxSizer* main_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Items");

	// Toolbar
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

	// Listbox
	item_list = new TilePropertiesListBox(main_sizer->GetStaticBox(), wxID_ANY);
	main_sizer->Add(item_list, wxSizerFlags(1).Expand());

	SetSizer(main_sizer);

	// Events
	item_list->Bind(wxEVT_LISTBOX, &BrowseFieldList::OnItemSelected, this);
	item_list->Bind(wxEVT_CONTEXT_MENU, &BrowseFieldList::OnContextMenu, this);
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
	UpdateButtons(selection);
}

void BrowseFieldList::OnItemSelected(wxCommandEvent& event) {
	ApplyListSelection();
	event.Skip(); // Let parent handle property panel swap if needed
}

void BrowseFieldList::ApplyListSelection() {
	int selection = item_list->GetSelection();
	Item* selected_item = nullptr;
	UpdateButtons(selection);
	if (selection != wxNOT_FOUND) {
		selected_item = item_list->GetItem(selection);
	}

	if (on_item_selected_cb) {
		on_item_selected_cb(selected_item);
	}

	// Issue #3: Sync selection with the editor. A panel row is one item, so no
	// BORDER_IS_GROUND expansion: the commands act on exactly this row.
	Editor* editor = g_gui.GetCurrentEditor();
	if (editor && current_tile && selected_item) {
		editor->selection.setSingleItem(current_tile, selected_item);
	}
}

void BrowseFieldList::OnContextMenu(wxContextMenuEvent& event) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor || !current_tile || current_map != &editor->map) {
		return;
	}

	// A keyboard-opened menu (Shift+F10 / Menu key) has no position and acts on the selected row.
	const bool from_keyboard = event.GetPosition() == wxDefaultPosition;
	const wxPoint click_point = from_keyboard ? wxDefaultPosition : item_list->ScreenToClient(event.GetPosition());
	const int hit_row = from_keyboard ? wxNOT_FOUND : item_list->RowAtClientPoint(click_point);
	const bool has_ground = current_tile->ground != nullptr;
	const auto row = TileStackRows::ContextMenuRow(
		from_keyboard, hit_row, item_list->GetSelection(), has_ground, current_tile->items.size()
	);
	if (!row) {
		return;
	}

	// Same mode switch as a right click on the map, so Copy works from the panel too.
	if (g_gui.IsDrawingMode()) {
		g_gui.SetSelectionMode();
	}

	// The clicked row becomes the editor selection first, so every command works on exactly that item.
	item_list->SetSelection(*row);
	ApplyListSelection();
	Item* item = item_list->GetItem(*row);
	if (!current_tile || !item) {
		return;
	}

	const Position position = current_tile->getPosition();
	const wxPoint menu_point = from_keyboard ? item_list->RowMenuAnchor(*row) : click_point;

	ItemPopupMenu menu;
	menu.Update(ItemContextMenu::Facts { .item = ContextMenu::CollectItemFacts(*item) });
	const int menu_id = item_list->GetPopupMenuSelectionFromUser(menu, menu_point);
	if (menu_id == wxID_NONE || g_gui.GetCurrentEditor() != editor) {
		return;
	}

	ItemContextActions::Execute(menu_id, *editor, position, *row);
	editor->actionQueue->resetTimer();
}

void BrowseFieldList::OnClickUp(wxCommandEvent& event) {
	if (!current_tile) {
		return;
	}
	const int selection = item_list->GetSelection();
	const bool has_ground = current_tile->ground != nullptr;
	if (!TileStackRows::CanMoveUp(selection, has_ground, current_tile->items.size())) {
		return;
	}
	const auto index_in_items = TileStackRows::Resolve(selection, has_ground, current_tile->items.size())->item_index;

	Editor* editor = g_gui.GetCurrentEditor();
	if (editor) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		std::swap(new_tile->items[index_in_items], new_tile->items[index_in_items - 1]);
		new_tile->items[index_in_items - 1]->select();

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}

void BrowseFieldList::OnClickDown(wxCommandEvent& event) {
	if (!current_tile) {
		return;
	}
	const int selection = item_list->GetSelection();
	const bool has_ground = current_tile->ground != nullptr;
	if (!TileStackRows::CanMoveDown(selection, has_ground, current_tile->items.size())) {
		return;
	}
	const auto index_in_items = TileStackRows::Resolve(selection, has_ground, current_tile->items.size())->item_index;

	Editor* editor = g_gui.GetCurrentEditor();
	if (editor) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, editor->map);
		std::swap(new_tile->items[index_in_items], new_tile->items[index_in_items + 1]);
		new_tile->items[index_in_items + 1]->select();

		std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor->addAction(std::move(action));
	}
}

void BrowseFieldList::OnClickDelete(wxCommandEvent& WXUNUSED(event)) {
	if (!current_tile) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	TileItemActions::DeleteRow(*editor, current_tile, item_list->GetSelection());
}

void BrowseFieldList::UpdateButtons(int selection) {
	const bool has_tile = current_tile != nullptr;
	const bool has_ground = has_tile && current_tile->ground != nullptr;
	const std::size_t item_count = has_tile ? current_tile->items.size() : 0;
	btn_up->Enable(TileStackRows::CanMoveUp(selection, has_ground, item_count));
	btn_down->Enable(TileStackRows::CanMoveDown(selection, has_ground, item_count));
	btn_delete->Enable(TileStackRows::Resolve(selection, has_ground, item_count).has_value());
}
