//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/replace_items_window.h"
#include "ui/find_item_window.h"
#include "editor/action_queue.h"
#include "rendering/core/graphics.h"
#include "ui/gui.h"
#include "util/image_manager.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "item_definitions/core/item_definition_store.h"
#include <glad/glad.h>
#include <format>
#include <nanovg.h>

// ============================================================================
// ReplaceItemsButton

ReplaceItemsButton::ReplaceItemsButton(wxWindow* parent) :
	DCButton(parent, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0),
	m_id(0) {
	////
}

ItemGroup_t ReplaceItemsButton::GetGroup() const {
	if (m_id != 0) {
		const auto it = g_item_definitions.get(m_id);
		if (it) {
			return it.group();
		}
	}
	return ITEM_GROUP_NONE;
}

void ReplaceItemsButton::SetItemId(uint16_t id) {
	if (m_id == id) {
		return;
	}

	m_id = id;

	if (m_id != 0) {
		const auto it = g_item_definitions.get(m_id);
		if (it) {
			SetSprite(it.clientId());
			return;
		}
	}

	SetSprite(0);
}

// ============================================================================
// ReplaceItemsListBox

ReplaceItemsListBox::ReplaceItemsListBox(wxWindow* parent) :
	NanoVGListBox(parent, wxID_ANY, wxLB_SINGLE),
	m_arrow_image(0),
	m_flag_image(0) {
}

bool ReplaceItemsListBox::AddItem(const ReplacingItem& item) {
	if (item.replaceId == 0 || item.withId == 0 || item.replaceId == item.withId) {
		return false;
	}

	SetItemCount(GetItemCount() + 1);
	m_items.push_back(item);
	Refresh();

	return true;
}

void ReplaceItemsListBox::MarkAsComplete(const ReplacingItem& item, uint32_t total) {
	auto it = std::find(m_items.begin(), m_items.end(), item);
	if (it != m_items.end()) {
		it->total = total;
		it->complete = true;
		Refresh();
	}
}

void ReplaceItemsListBox::RemoveSelected() {
	if (m_items.empty()) {
		return;
	}

	const int index = GetSelection();
	if (index == -1) {
		return;
	}

	m_items.erase(m_items.begin() + index);
	SetItemCount(GetItemCount() - 1);
	Refresh();
}

bool ReplaceItemsListBox::CanAdd(uint16_t replaceId, uint16_t withId) const {
	if (replaceId == 0 || withId == 0 || replaceId == withId) {
		return false;
	}

	for (const ReplacingItem& item : m_items) {
		if (replaceId == item.replaceId) {
			return false;
		}
	}
	return true;
}

void ReplaceItemsListBox::OnDrawItem(NVGcontext* vg, const wxRect& rect, size_t index) {
	ASSERT(index < m_items.size());

	if (m_arrow_image == 0) {
		m_arrow_image = IMAGE_MANAGER.GetNanoVGImage(vg, ICON_LOCATION_ARROW);
		if (m_arrow_image == 0) {
			m_arrow_image = -1;
		}
	}
	if (m_flag_image == 0) {
		m_flag_image = IMAGE_MANAGER.GetNanoVGImage(vg, IMAGE_PROTECTION_ZONE_SMALL);
		if (m_flag_image == 0) {
			m_flag_image = -1;
		}
	}

	const ReplacingItem& item = m_items.at(index);
	const auto type1 = g_item_definitions.get(item.replaceId);
	const uint16_t cid1 = type1 ? type1.clientId() : 0;
	Sprite* sprite1 = cid1 ? g_gui.gfx.getSprite(cid1) : nullptr;
	const auto tech1 = rme::rendering::TechnicalItemRegistry::Classify(item.replaceId, cid1);

	const auto type2 = g_item_definitions.get(item.withId);
	const uint16_t cid2 = type2 ? type2.clientId() : 0;
	Sprite* sprite2 = cid2 ? g_gui.gfx.getSprite(cid2) : nullptr;
	const auto tech2 = rme::rendering::TechnicalItemRegistry::Classify(item.withId, cid2);

	int x = rect.GetX();
	int y = rect.GetY();

	if (tech1 != rme::rendering::TileIndicatorType::None) {
		rme::rendering::DrawNanoVGIndicatorBadge(vg, tech1, static_cast<float>(x + 4), static_cast<float>(y + 4), 32.0f);
	} else if (sprite1) {
		int tex1 = GetOrCreateSpriteTexture(vg, sprite1);
		if (tex1 > 0) {
			NVGpaint imgPaint = nvgImagePattern(vg, x + 4, y + 4, 32, 32, 0, tex1, 1.0f);
			nvgBeginPath(vg);
			nvgRect(vg, x + 4, y + 4, 32, 32);
			nvgFillPaint(vg, imgPaint);
			nvgFill(vg);
		}
	}

	if (m_arrow_image > 0) {
		NVGpaint arrowPaint = nvgImagePattern(vg, x + 38, y + 10, 16, 16, 0, m_arrow_image, 1.0f);
		nvgBeginPath(vg);
		nvgRect(vg, x + 38, y + 10, 16, 16);
		nvgFillPaint(vg, arrowPaint);
		nvgFill(vg);
	}

	if (tech2 != rme::rendering::TileIndicatorType::None) {
		rme::rendering::DrawNanoVGIndicatorBadge(vg, tech2, static_cast<float>(x + 56), static_cast<float>(y + 4), 32.0f);
	} else if (sprite2) {
		int tex2 = GetOrCreateSpriteTexture(vg, sprite2);
		if (tex2 > 0) {
			NVGpaint imgPaint = nvgImagePattern(vg, x + 56, y + 4, 32, 32, 0, tex2, 1.0f);
			nvgBeginPath(vg);
			nvgRect(vg, x + 56, y + 4, 32, 32);
			nvgFillPaint(vg, imgPaint);
			nvgFill(vg);
		}
	}

	if (IsSelected(index)) {
		wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
	} else {
		wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
	}

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, x + 104, y + 20, std::format("Replace: {} With: {}", item.replaceId, item.withId).c_str(), nullptr);

	if (item.complete) {
		int rightX = rect.GetRight() - 100;
		if (m_flag_image > 0) {
			NVGpaint flagPaint = nvgImagePattern(vg, rightX + 70, y + 10, 16, 16, 0, m_flag_image, 1.0f);
			nvgBeginPath(vg);
			nvgRect(vg, rightX + 70, y + 10, 16, 16);
			nvgFillPaint(vg, flagPaint);
			nvgFill(vg);
		}
		nvgText(vg, rightX, y + 20, std::format("Total: {}", item.total).c_str(), nullptr);
	}
}

int ReplaceItemsListBox::OnMeasureItem(size_t index) const {
	return FromDIP(40);
}

// ============================================================================
// ReplaceItemsDialog

ReplaceItemsDialog::ReplaceItemsDialog(wxWindow* parent, bool selectionOnly) :
	wxDialog(parent, wxID_ANY, (selectionOnly ? "Replace Items on Selection" : "Replace Items"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
	selectionOnly(selectionOnly) {
	SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer* list_sizer = new wxFlexGridSizer(0, 2, 0, 0);
	list_sizer->SetFlexibleDirection(wxBOTH);
	list_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	list_sizer->SetMinSize(wxSize(-1, FromDIP(300)));

	list = new ReplaceItemsListBox(this);
	list->SetMinSize(FromDIP(wxSize(480, 320)));

	list_sizer->Add(list, 0, wxALL | wxEXPAND, 5);
	sizer->Add(list_sizer, 1, wxALL | wxEXPAND, 5);

	wxBoxSizer* items_sizer = new wxBoxSizer(wxHORIZONTAL);
	items_sizer->SetMinSize(wxSize(-1, FromDIP(40)));

	replace_button = new ReplaceItemsButton(this);
	items_sizer->Add(replace_button, 0, wxALL, 5);

	wxBitmapBundle bundle = IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW);
	arrow_bitmap = new wxStaticBitmap(this, wxID_ANY, bundle);
	items_sizer->Add(arrow_bitmap, 0, wxTOP, 15);

	with_button = new ReplaceItemsButton(this);
	items_sizer->Add(with_button, 0, wxALL, 5);

	items_sizer->Add(0, 0, 1, wxEXPAND, 5);

	progress = new wxGauge(this, wxID_ANY, 100);
	progress->SetValue(0);
	items_sizer->Add(progress, 0, wxALL, 5);

	sizer->Add(items_sizer, 1, wxALL | wxEXPAND, 5);

	wxBoxSizer* buttons_sizer = new wxBoxSizer(wxHORIZONTAL);

	add_button = new wxButton(this, wxID_ANY, "Add");
	add_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	add_button->SetToolTip("Add replacement rule to list");
	add_button->Enable(false);
	buttons_sizer->Add(add_button, 0, wxALL, 5);

	remove_button = new wxButton(this, wxID_ANY, "Remove");
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	remove_button->SetToolTip("Remove selected rule");
	remove_button->Enable(false);
	buttons_sizer->Add(remove_button, 0, wxALL, 5);

	buttons_sizer->Add(0, 0, 1, wxEXPAND, 5);

	execute_button = new wxButton(this, wxID_ANY, "Execute");
	execute_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLAY));
	execute_button->SetToolTip("Execute all replacement rules");
	execute_button->Enable(false);
	buttons_sizer->Add(execute_button, 0, wxALL, 5);

	close_button = new wxButton(this, wxID_ANY, "Close");
	close_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	close_button->SetToolTip("Close this window");
	buttons_sizer->Add(close_button, 0, wxALL, 5);

	sizer->Add(buttons_sizer, 1, wxALL | wxLEFT | wxRIGHT | wxSHAPED, 5);

	SetSizer(sizer);
	Layout();
	Centre(wxBOTH);

	// Connect Events
	list->Bind(wxEVT_LISTBOX, &ReplaceItemsDialog::OnListSelected, this);
	replace_button->Bind(wxEVT_LEFT_DOWN, &ReplaceItemsDialog::OnReplaceItemClicked, this);
	with_button->Bind(wxEVT_LEFT_DOWN, &ReplaceItemsDialog::OnWithItemClicked, this);
	add_button->Bind(wxEVT_BUTTON, &ReplaceItemsDialog::OnAddButtonClicked, this);
	remove_button->Bind(wxEVT_BUTTON, &ReplaceItemsDialog::OnRemoveButtonClicked, this);
	execute_button->Bind(wxEVT_BUTTON, &ReplaceItemsDialog::OnExecuteButtonClicked, this);
	close_button->Bind(wxEVT_BUTTON, &ReplaceItemsDialog::OnCancelButtonClicked, this);

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_SYNC));
}

ReplaceItemsDialog::~ReplaceItemsDialog() {
}

void ReplaceItemsDialog::UpdateWidgets() {
	const uint16_t replaceId = replace_button->GetItemId();
	const uint16_t withId = with_button->GetItemId();
	add_button->Enable(list->CanAdd(replaceId, withId));
	if (add_button->IsEnabled()) {
		add_button->SetToolTip("Add replacement rule to list");
	} else {
		add_button->SetToolTip("Select replacement and target items to add.");
	}

	remove_button->Enable(list->GetCount() != 0 && list->GetSelection() != wxNOT_FOUND);
	if (remove_button->IsEnabled()) {
		remove_button->SetToolTip("Remove selected rule");
	} else {
		remove_button->SetToolTip("Select a rule to remove.");
	}

	execute_button->Enable(list->GetCount() != 0);
	if (execute_button->IsEnabled()) {
		execute_button->SetToolTip("Execute all replacement rules");
	} else {
		execute_button->SetToolTip("Add rules to list first.");
	}
}

void ReplaceItemsDialog::OnListSelected(wxCommandEvent& WXUNUSED(event)) {
	remove_button->Enable(list->GetCount() != 0 && list->GetSelection() != wxNOT_FOUND);
}

void ReplaceItemsDialog::OnReplaceItemClicked(wxMouseEvent& WXUNUSED(event)) {
	FindItemDialog dialog(this, "Replace Item");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t id = dialog.getResultID();
		if (id != with_button->GetItemId()) {
			replace_button->SetItemId(id);
			UpdateWidgets();
		}
	}
	dialog.Destroy();
}

void ReplaceItemsDialog::OnWithItemClicked(wxMouseEvent& WXUNUSED(event)) {
	if (replace_button->GetItemId() == 0) {
		return;
	}

	FindItemDialog dialog(this, "With Item");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t id = dialog.getResultID();
		if (id != replace_button->GetItemId()) {
			with_button->SetItemId(id);
			UpdateWidgets();
		}
	}
	dialog.Destroy();
}

void ReplaceItemsDialog::OnAddButtonClicked(wxCommandEvent& WXUNUSED(event)) {
	const uint16_t replaceId = replace_button->GetItemId();
	const uint16_t withId = with_button->GetItemId();
	if (list->CanAdd(replaceId, withId)) {
		ReplacingItem item;
		item.replaceId = replaceId;
		item.withId = withId;
		if (list->AddItem(item)) {
			replace_button->SetItemId(0);
			with_button->SetItemId(0);
			UpdateWidgets();
		}
	}
}

void ReplaceItemsDialog::OnRemoveButtonClicked(wxCommandEvent& WXUNUSED(event)) {
	list->RemoveSelected();
	UpdateWidgets();
}

void ReplaceItemsDialog::OnExecuteButtonClicked(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	const auto& items = list->GetItems();
	if (items.empty()) {
		return;
	}

	replace_button->Enable(false);
	with_button->Enable(false);
	add_button->Enable(false);
	remove_button->Enable(false);
	execute_button->Enable(false);
	close_button->Enable(false);
	progress->SetValue(0);

	MapTab* tab = dynamic_cast<MapTab*>(GetParent());
	if (!tab) {
		return;
	}

	Editor* editor = tab->GetEditor();

	int done = 0;
	for (const ReplacingItem& info : items) {
		ItemFinder finder(info.replaceId, (uint32_t)g_settings.getInteger(Config::REPLACE_SIZE));

		// search on map
		foreach_ItemOnMap(editor->map, finder, selectionOnly);

		uint32_t total = 0;
		std::vector<std::pair<Tile*, Item*>>& result = finder.result;

		if (!result.empty()) {
			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_REPLACE_ITEMS);
			for (const auto& [tile, itemToReplace] : result) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor->map);
				int index = tile->getIndexOf(itemToReplace);
				ASSERT(index != wxNOT_FOUND);
				Item* item = new_tile->getItemAt(index);
				ASSERT(item && item->getID() == itemToReplace->getID());
				TileOperations::transformItem(item, info.withId, new_tile.get());
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				total++;
			}
			editor->actionQueue->addAction(std::move(action));
		}

		done++;
		const int value = static_cast<int>((static_cast<double>(done) / items.size()) * 100);
		progress->SetValue(std::clamp<int>(value, 0, 100));
		list->MarkAsComplete(info, total);
	}

	tab->Refresh();
	close_button->Enable(true);
	UpdateWidgets();
}

void ReplaceItemsDialog::OnCancelButtonClicked(wxCommandEvent& WXUNUSED(event)) {
	Close();
}
