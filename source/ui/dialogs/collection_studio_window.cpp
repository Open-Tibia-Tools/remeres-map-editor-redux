#include "ui/dialogs/collection_studio_window.h"

#include "ui/controls/collection_item_grid.h"
#include "ui/controls/collection_brush_grid.h"
#include "ui/dialogs/collection_options_panel.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "io/collection_xml_service.h"
#include "palette/managers/collection_palette_sync.h"
#include "ui/map_window.h"
#include "util/common.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>

CollectionStudioWindow::CollectionStudioWindow(wxWindow* parent, Editor* editor) :
	wxDialog(parent, wxID_ANY, "Add to Tileset", wxDefaultPosition, wxSize(940, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	m_editor(editor) {
	SetMinSize(wxSize(800, 480));
	InitLayout();

	Bind(wxEVT_CLOSE_WINDOW, &CollectionStudioWindow::OnClose, this);
	Bind(wxEVT_CHAR_HOOK, &CollectionStudioWindow::OnCharHook, this);
}

CollectionStudioWindow::~CollectionStudioWindow() {
	Unbind(wxEVT_CLOSE_WINDOW, &CollectionStudioWindow::OnClose, this);
	Unbind(wxEVT_CHAR_HOOK, &CollectionStudioWindow::OnCharHook, this);
}

void CollectionStudioWindow::InitLayout() {
	wxBoxSizer* rootSizer = new wxBoxSizer(wxHORIZONTAL);

	// ================= Column 1: Items =================
	wxStaticBoxSizer* itemsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Items");

	TilesetGridToolbar::Actions itemActions;
	itemActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_itemGrid->SetSort(key, dir);
	};
	itemActions.clear_sort = [this]() {
		m_itemGrid->ClearSort();
	};
	itemActions.show_labels = [this](bool show) {
		m_itemGrid->SetShowLabels(show);
	};
	itemActions.icon_size = [this](int size) {
		m_itemGrid->SetIconSize(size);
		m_itemGrid->RefreshGrid();
	};
	itemActions.filter = [this](const std::string& query) {
		m_itemGrid->SetFilter(query);
	};
	itemActions.filter_cleared = [this]() {
		m_itemGrid->SetFilter("");
	};

	m_itemToolbar = new TilesetGridToolbar(
		itemsBox->GetStaticBox(),
		std::move(itemActions),
		TilesetGridToolbar::Part_Standard & ~TilesetGridToolbar::Part_FilterAll
	);
	m_itemToolbar->SetState(false, 32, false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);
	itemsBox->Add(m_itemToolbar, 0, wxEXPAND | wxBOTTOM, 4);

	m_itemGrid = new rme::ui::CollectionItemGrid(itemsBox->GetStaticBox());
	m_itemGrid->SetDiscardCallback([this](uint16_t id) {
		OnDiscardItem(id);
	});
	itemsBox->Add(m_itemGrid, 1, wxEXPAND);

	wxBoxSizer* itemsBottom = new wxBoxSizer(wxHORIZONTAL);
	m_clearItemsBtn = new wxButton(itemsBox->GetStaticBox(), wxID_ANY, "Clear Items");
	m_itemCountLabel = new wxStaticText(itemsBox->GetStaticBox(), wxID_ANY, "Items: 0");
	itemsBottom->Add(m_clearItemsBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
	itemsBottom->AddStretchSpacer(1);
	itemsBottom->Add(m_itemCountLabel, 0, wxALIGN_CENTER_VERTICAL);
	itemsBox->Add(itemsBottom, 0, wxEXPAND | wxTOP, 6);

	rootSizer->Add(itemsBox, 1, wxEXPAND | wxALL, 6);

	// ================= Column 2: Brushes =================
	wxStaticBoxSizer* brushesBox = new wxStaticBoxSizer(wxVERTICAL, this, "Brushes");

	TilesetGridToolbar::Actions brushActions;
	brushActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_brushGrid->SetSort(key, dir);
	};
	brushActions.clear_sort = [this]() {
		m_brushGrid->ClearSort();
	};
	brushActions.show_labels = [this](bool show) {
		m_brushGrid->SetShowLabels(show);
	};
	brushActions.icon_size = [this](int size) {
		m_brushGrid->SetIconSize(size);
	};
	brushActions.filter = [this](const std::string& query) {
		m_brushGrid->SetFilter(query);
	};
	brushActions.filter_cleared = [this]() {
		m_brushGrid->SetFilter("");
	};

	m_brushToolbar = new TilesetGridToolbar(
		brushesBox->GetStaticBox(),
		std::move(brushActions),
		TilesetGridToolbar::Part_Standard & ~TilesetGridToolbar::Part_FilterAll
	);
	m_brushToolbar->SetState(false, 32, false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);
	brushesBox->Add(m_brushToolbar, 0, wxEXPAND | wxBOTTOM, 4);

	m_brushGrid = new rme::ui::CollectionBrushGrid(brushesBox->GetStaticBox());
	m_brushGrid->SetDiscardCallback([this](Brush* b) {
		OnDiscardBrush(b);
	});
	brushesBox->Add(m_brushGrid, 1, wxEXPAND);

	wxBoxSizer* brushesBottom = new wxBoxSizer(wxHORIZONTAL);
	m_clearBrushesBtn = new wxButton(brushesBox->GetStaticBox(), wxID_ANY, "Clear Brushes");
	m_brushCountLabel = new wxStaticText(brushesBox->GetStaticBox(), wxID_ANY, "Brushes: 0");
	brushesBottom->Add(m_clearBrushesBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
	brushesBottom->AddStretchSpacer(1);
	brushesBottom->Add(m_brushCountLabel, 0, wxALIGN_CENTER_VERTICAL);
	brushesBox->Add(brushesBottom, 0, wxEXPAND | wxTOP, 6);

	rootSizer->Add(brushesBox, 1, wxEXPAND | wxALL, 6);

	// ================= Column 3: Options =================
	rme::ui::CollectionOptionsPanel::Callbacks callbacks;
	callbacks.onAccumulateSelection = [this]() {
		if (m_editor) {
			AppendFromSelection(m_editor);
		}
	};
	callbacks.onUndo = [this]() {
		UndoDiscard();
	};
	callbacks.onRedo = [this]() {
		RedoDiscard();
	};
	callbacks.onClearAll = [this]() {
		OnClearAll();
	};
	callbacks.onSave = [this](const std::string& paletteName, const std::string& name, bool isNew, bool append, bool includeItems, bool includeBrushes) {
		OnSave(paletteName, name, isNew, append, includeItems, includeBrushes);
	};

	m_optionsPanel = new rme::ui::CollectionOptionsPanel(this, std::move(callbacks));
	m_optionsPanel->SetMinSize(wxSize(260, -1));
	rootSizer->Add(m_optionsPanel, 0, wxEXPAND | wxALL, 6);

	SetSizer(rootSizer);

	// Button bindings
	m_clearItemsBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		OnClearItems();
	});

	m_clearBrushesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		OnClearBrushes();
	});

	UpdateUI();
}

void CollectionStudioWindow::AppendFromSelection(Editor* editor) {
	if (!editor) {
		return;
	}
	m_editor = editor;
	auto newlyExtracted = rme::collection::CollectionExtractorService::ExtractFromSelection(editor);
	m_staging.Merge(newlyExtracted);
	m_redoStack.clear();
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::DetachEditor() {
	m_editor = nullptr;
	if (m_optionsPanel) {
		m_optionsPanel->EnableAccumulate(false);
	}
}

void CollectionStudioWindow::OnDiscardItem(uint16_t itemId) {
	auto it = std::find(m_staging.itemIds.begin(), m_staging.itemIds.end(), itemId);
	if (it != m_staging.itemIds.end()) {
		DiscardAction act;
		act.kind = DiscardAction::Kind::Item;
		act.itemId = itemId;
		act.originalIndex = static_cast<size_t>(std::distance(m_staging.itemIds.begin(), it));
		m_undoStack.push_back(std::move(act));
		m_redoStack.clear();
	}
	m_staging.DiscardItem(itemId);
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::OnDiscardBrush(Brush* brush) {
	auto it = std::find(m_staging.brushes.begin(), m_staging.brushes.end(), brush);
	if (it != m_staging.brushes.end()) {
		DiscardAction act;
		act.kind = DiscardAction::Kind::Brush;
		act.brush = brush;
		act.originalIndex = static_cast<size_t>(std::distance(m_staging.brushes.begin(), it));
		m_undoStack.push_back(std::move(act));
		m_redoStack.clear();
	}
	m_staging.DiscardBrush(brush);
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::OnClearItems() {
	if (m_staging.itemIds.empty()) {
		return;
	}
	DiscardAction act;
	act.kind = DiscardAction::Kind::ClearItems;
	act.savedItems = m_staging.itemIds;
	m_undoStack.push_back(std::move(act));
	m_redoStack.clear();
	m_staging.itemIds.clear();
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::OnClearBrushes() {
	if (m_staging.brushes.empty()) {
		return;
	}
	DiscardAction act;
	act.kind = DiscardAction::Kind::ClearBrushes;
	act.savedBrushes = m_staging.brushes;
	m_undoStack.push_back(std::move(act));
	m_redoStack.clear();
	m_staging.brushes.clear();
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::OnClearAll() {
	if (m_staging.Empty()) {
		return;
	}
	DiscardAction act;
	act.kind = DiscardAction::Kind::ClearAll;
	act.savedItems = m_staging.itemIds;
	act.savedBrushes = m_staging.brushes;
	m_undoStack.push_back(std::move(act));
	m_redoStack.clear();
	m_staging.Clear();
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::UndoDiscard() {
	if (m_undoStack.empty()) {
		return;
	}

	DiscardAction act = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	switch (act.kind) {
	case DiscardAction::Kind::Item: {
		if (std::find(m_staging.itemIds.begin(), m_staging.itemIds.end(), act.itemId) == m_staging.itemIds.end()) {
			if (act.originalIndex < m_staging.itemIds.size()) {
				m_staging.itemIds.insert(m_staging.itemIds.begin() + act.originalIndex, act.itemId);
			} else {
				m_staging.itemIds.push_back(act.itemId);
			}
		}
		break;
	}
	case DiscardAction::Kind::Brush: {
		if (std::find(m_staging.brushes.begin(), m_staging.brushes.end(), act.brush) == m_staging.brushes.end()) {
			if (act.originalIndex < m_staging.brushes.size()) {
				m_staging.brushes.insert(m_staging.brushes.begin() + act.originalIndex, act.brush);
			} else {
				m_staging.brushes.push_back(act.brush);
			}
		}
		break;
	}
	case DiscardAction::Kind::ClearItems: {
		rme::collection::CollectionStagingContent restored;
		restored.itemIds = std::move(act.savedItems);
		m_staging.Merge(restored);
		break;
	}
	case DiscardAction::Kind::ClearBrushes: {
		rme::collection::CollectionStagingContent restored;
		restored.brushes = std::move(act.savedBrushes);
		m_staging.Merge(restored);
		break;
	}
	case DiscardAction::Kind::ClearAll: {
		rme::collection::CollectionStagingContent restored;
		restored.itemIds = std::move(act.savedItems);
		restored.brushes = std::move(act.savedBrushes);
		m_staging.Merge(restored);
		break;
	}
	}

	m_redoStack.push_back(std::move(act));
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::RedoDiscard() {
	if (m_redoStack.empty()) {
		return;
	}

	DiscardAction act = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	switch (act.kind) {
	case DiscardAction::Kind::Item: {
		m_staging.DiscardItem(act.itemId);
		break;
	}
	case DiscardAction::Kind::Brush: {
		m_staging.DiscardBrush(act.brush);
		break;
	}
	case DiscardAction::Kind::ClearItems: {
		act.savedItems = m_staging.itemIds;
		m_staging.itemIds.clear();
		break;
	}
	case DiscardAction::Kind::ClearBrushes: {
		act.savedBrushes = m_staging.brushes;
		m_staging.brushes.clear();
		break;
	}
	case DiscardAction::Kind::ClearAll: {
		act.savedItems = m_staging.itemIds;
		act.savedBrushes = m_staging.brushes;
		m_staging.Clear();
		break;
	}
	}

	m_undoStack.push_back(std::move(act));
	m_isDirty = true;
	UpdateUI();
}

void CollectionStudioWindow::UpdateUI() {
	if (m_itemGrid) {
		m_itemGrid->SetItems(m_staging.itemIds);
	}
	if (m_brushGrid) {
		m_brushGrid->SetBrushes(m_staging.brushes);
	}
	if (m_itemCountLabel) {
		m_itemCountLabel->SetLabel(wxString::Format("Items: %zu", m_staging.itemIds.size()));
	}
	if (m_brushCountLabel) {
		m_brushCountLabel->SetLabel(wxString::Format("Brushes: %zu", m_staging.brushes.size()));
	}
	if (m_optionsPanel) {
		m_optionsPanel->UpdateCounts(m_staging.itemIds.size(), m_staging.brushes.size());
		m_optionsPanel->EnableAccumulate(m_editor != nullptr);
		m_optionsPanel->UpdateUndoRedo(!m_undoStack.empty(), !m_redoStack.empty());
	}
	Layout();
}

void CollectionStudioWindow::OnSave(const std::string& paletteName, const std::string& name, bool isNew, bool append, bool includeItems, bool includeBrushes) {
	std::span<const uint16_t> itemsToSave = includeItems ? m_staging.itemIds : std::span<const uint16_t>{};
	std::span<Brush* const> brushesToSave = includeBrushes ? m_staging.brushes : std::span<Brush* const>{};

	auto tilesetFilePath = rme::collection::CollectionXmlService::GetTilesetFilePath(paletteName, name);

	std::string error;
	if (!rme::collection::CollectionXmlService::SaveTilesetXml(tilesetFilePath, name, itemsToSave, brushesToSave, append, error)) {
		wxMessageBox(wxString::Format("Error saving tileset:\n%s", wxstr(error)), "Add to Tileset", wxOK | wxICON_ERROR, this);
		return;
	}

	if (!rme::collection::CollectionXmlService::EnsurePaletteInclude(paletteName, name, error)) {
		wxMessageBox(wxString::Format("Error updating palettes.xml:\n%s", wxstr(error)), "Add to Tileset", wxOK | wxICON_ERROR, this);
		return;
	}

	rme::collection::CollectionPaletteSync::Sync(paletteName, name, tilesetFilePath);

	m_isDirty = false;
	m_optionsPanel->RefreshExistingTilesets();
	wxMessageBox(wxString::Format("Successfully saved tileset to %s palette!", wxstr(paletteName)), "Add to Tileset", wxOK | wxICON_INFORMATION, this);
	Close();
}

void CollectionStudioWindow::OnCharHook(wxKeyEvent& event) {
	if (event.GetKeyCode() == WXK_ESCAPE) {
		Close();
		return;
	}

	const int key = event.GetKeyCode();
	const bool ctrlDown = event.ControlDown() || event.RawControlDown();
	const bool shiftDown = event.ShiftDown();

	if (ctrlDown && (key == 'Z' || key == 'z' || key == 'Y' || key == 'y')) {
		wxWindow* focused = wxWindow::FindFocus();
		if (dynamic_cast<wxTextCtrl*>(focused)) {
			event.Skip();
			return;
		}

		if (key == 'Y' || key == 'y' || shiftDown) {
			RedoDiscard();
		} else {
			UndoDiscard();
		}
		return;
	}

	event.Skip();
}

void CollectionStudioWindow::OnClose(wxCloseEvent& event) {
	if (m_isDirty && !m_staging.Empty() && event.CanVeto()) {
		const int answer = wxMessageBox(
			"You have unsaved elements in Add to Tileset.\n\nWould you like to save them before closing?",
			"Unsaved Changes",
			wxYES_NO | wxCANCEL | wxICON_WARNING,
			this
		);
		if (answer == wxCANCEL) {
			event.Veto();
			return;
		}
		if (answer == wxYES) {
			m_optionsPanel->PerformSave();
			if (m_isDirty) {
				event.Veto();
				return;
			}
		}
	}

	if (auto* owner = wxDynamicCast(GetParent(), MapWindow)) {
		owner->ForgetCollectionStudio();
	}

	Destroy();
}
