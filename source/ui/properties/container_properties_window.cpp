//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/properties/container_properties_window.h"
#include "map/map.h"

#include "game/complexitem.h"
#include "ui/dialog_util.h"
#include "ui/properties/property_validator.h"
#include "util/image_manager.h"
#include "app/application.h"
#include "ui/find_item_window.h"
#include "ui/properties/item_properties_window.h"
#include "ui/tile_properties/container_grid_canvas.h"

ContainerPropertiesWindow::ContainerPropertiesWindow(wxWindow* win_parent, const Map* map, const Tile* tile_parent, Item* item, wxPoint pos) :
	ObjectPropertiesWindowBase(win_parent, "Container Properties", map, tile_parent, item, pos),
	action_id_field(nullptr),
	unique_id_field(nullptr),
	grid_canvas(nullptr) {
	ASSERT(edit_item);
	Container* container = dynamic_cast<Container*>(edit_item);
	ASSERT(container);

	Bind(wxEVT_BUTTON, &ContainerPropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &ContainerPropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	Bind(wxEVT_MENU, &ContainerPropertiesWindow::OnAddItem, this, CONTAINER_POPUP_MENU_ADD);
	Bind(wxEVT_MENU, &ContainerPropertiesWindow::OnEditItem, this, CONTAINER_POPUP_MENU_EDIT);
	Bind(wxEVT_MENU, &ContainerPropertiesWindow::OnRemoveItem, this, CONTAINER_POPUP_MENU_REMOVE);

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Container Properties");

	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "ID " + i2ws(item->getID())));
	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "\"" + wxstr(item->getName()) + "\""));

	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "Action ID"));
	action_id_field = newd wxSpinCtrl(boxsizer->GetStaticBox(), wxID_ANY, i2ws(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	action_id_field->SetToolTip("Action ID (0-65535). Used for scripting.");
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "Unique ID"));
	unique_id_field = newd wxSpinCtrl(boxsizer->GetStaticBox(), wxID_ANY, i2ws(edit_item->getUniqueID()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	unique_id_field->SetToolTip("Unique ID (0-65535). Must be unique on the map.");
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(0).Expand());

	// Now we add the subitems!
	wxStaticBoxSizer* contents_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Contents");

	grid_canvas = newd ContainerGridCanvas(contents_sizer->GetStaticBox(), g_settings.getBoolean(Config::USE_LARGE_CONTAINER_ICONS));
	grid_canvas->Bind(wxEVT_BUTTON, &ContainerPropertiesWindow::OnContainerItemClick, this);
	grid_canvas->Bind(wxEVT_CONTEXT_MENU, &ContainerPropertiesWindow::OnContainerItemRightClick, this);
	grid_canvas->SetContainer(edit_item);

	contents_sizer->Add(grid_canvas, wxSizerFlags(1).Expand().Border(wxALL, 2));

	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxALL, 20));
	topsizer->Add(contents_sizer, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 20));

	wxSizer* std_sizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	std_sizer->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	std_sizer->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(std_sizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_BOX_OPEN, wxSize(32, 32)));
	SetIcon(icon);
}

ContainerPropertiesWindow::~ContainerPropertiesWindow() {
	//
}

void ContainerPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	int new_uid = unique_id_field->GetValue();
	int new_aid = action_id_field->GetValue();

	if (!PropertyValidator::validateItemProperties(this, new_uid, new_aid, 0)) {
		return;
	}

	edit_item->setUniqueID(new_uid);
	edit_item->setActionID(new_aid);
	EndModal(1);
}

void ContainerPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}

void ContainerPropertiesWindow::Update() {
	if (grid_canvas) {
		grid_canvas->SetContainer(edit_item);
		grid_canvas->SetLargeSprites(g_settings.getBoolean(Config::USE_LARGE_CONTAINER_ICONS));
	}
	Layout();
	wxDialog::Update();
}

void ContainerPropertiesWindow::OnContainerItemClick(wxCommandEvent& event) {
	int index = event.GetInt();

	Container* container = edit_item ? dynamic_cast<Container*>(edit_item) : nullptr;
	if (!container || !edit_map) {
		return;
	}

	if (index < static_cast<int>(container->getItemCount())) {
		OnEditItem(event);
	} else {
		OnAddItem(event);
	}
}

void ContainerPropertiesWindow::OnContainerItemRightClick(wxContextMenuEvent& event) {
	if (!grid_canvas || !edit_item) {
		return;
	}

	int sub_item_index = grid_canvas->GetSelectedIndex();
	if (sub_item_index == -1) {
		return;
	}

	Container* container = dynamic_cast<Container*>(edit_item);
	if (!container || static_cast<size_t>(sub_item_index) >= container->getVolume()) {
		return;
	}

	wxMenu menu;
	if (sub_item_index < static_cast<int>(container->getItemCount())) {
		menu.Append(CONTAINER_POPUP_MENU_EDIT, "&Edit Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN_TO_SQUARE, wxSize(16, 16)));
		menu.Append(CONTAINER_POPUP_MENU_ADD, "&Add Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
		menu.Append(CONTAINER_POPUP_MENU_REMOVE, "&Remove Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	} else {
		menu.Append(CONTAINER_POPUP_MENU_ADD, "&Add Item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	}

	if (container && container->getVolume() <= (int)container->getVector().size()) {
		if (wxMenuItem* addItem = menu.FindItem(CONTAINER_POPUP_MENU_ADD)) {
			addItem->Enable(false);
		}
	}

	PopupMenu(&menu);
}

void ContainerPropertiesWindow::OnAddItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !edit_item) {
		return;
	}

	Container* container = dynamic_cast<Container*>(edit_item);
	if (!container) {
		return;
	}

	FindItemDialog dialog(this, "Select Item");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t item_id = dialog.getResultID();
		if (item_id != 0) {
			auto& contents = container->getVector();
			uint32_t index = grid_canvas->GetSelectedIndex();

			std::unique_ptr<Item> new_item(Item::Create(item_id)); // Wrap locally
			if (new_item) {
				if (index < contents.size()) {
					contents.insert(contents.begin() + index, std::move(new_item));
				} else {
					contents.push_back(std::move(new_item));
				}
			}
			Update();
		}
	}
}

void ContainerPropertiesWindow::OnEditItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !edit_item) {
		return;
	}

	int sub_item_index = grid_canvas->GetSelectedIndex();
	if (sub_item_index == -1) {
		return;
	}

	Container* container = dynamic_cast<Container*>(edit_item);
	if (!container || static_cast<size_t>(sub_item_index) >= container->getItemCount()) {
		return;
	}

	Item* sub_item = container->getItem(sub_item_index);
	wxPoint newDialogAt = GetPosition() + FROM_DIP(this, wxPoint(20, 20));

	wxDialog* d = newd ItemPropertiesWindow(this, edit_map, nullptr, sub_item, newDialogAt);

	d->ShowModal();
	d->Destroy();
	Update();
}

void ContainerPropertiesWindow::OnRemoveItem(wxCommandEvent& WXUNUSED(event)) {
	if (!grid_canvas || !edit_item) {
		return;
	}

	int sub_item_index = grid_canvas->GetSelectedIndex();
	if (sub_item_index == -1) {
		return;
	}

	Container* container = dynamic_cast<Container*>(edit_item);
	if (!container || static_cast<size_t>(sub_item_index) >= container->getItemCount()) {
		return;
	}

	int32_t ret = DialogUtil::PopupDialog(this, "Remove Item", "Are you sure you want to remove this item from the container?", wxYES | wxNO);

	if (ret != wxID_YES) {
		return;
	}

	auto& contents = container->getVector();
	uint32_t sub_index = sub_item_index;
	if (sub_index < contents.size()) {
		contents.erase(contents.begin() + sub_index);
	}

	Update();
}
