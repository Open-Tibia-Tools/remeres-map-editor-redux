//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tilesets_window.h"

#include "app/main.h"
#include "palette/quick_tilesets/quick_tileset_drop_target.h"
#include "palette/quick_tilesets/quick_tileset_page.h"
#include "palette/quick_tilesets/quick_tilesets.h"
#include "ui/gui.h"
#include "ui/theme.h"

#include <wx/aui/aui.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/textdlg.h>

#include <string>
#include <utility>

namespace QuickTilesets {

	namespace {

		// Commands of the "Options" menu, numbered from wxID_HIGHEST + 1.
		enum class EditMenuId : int {
			Rename = wxID_HIGHEST + 1,
			Delete,
			MoveLeft,
			MoveRight,
			Float,
			DockLeft,
			DockRight,
		};

		constexpr int ToId(EditMenuId id) {
			return static_cast<int>(id);
		}

	} // namespace

	QuickTilesetsWindow::QuickTilesetsWindow(wxWindow* parent) :
		wxPanel(parent) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		auto* layout = new wxBoxSizer(wxVERTICAL);
		auto* row = new wxBoxSizer(wxHORIZONTAL);

		auto* add = new wxButton(this, wxID_ANY, "+ Quick Tileset");
		row->Add(add, wxSizerFlags(1).Border(wxALL, Theme::Grid(1)));

		auto* list = new wxButton(this, wxID_ANY, wxString::FromUTF8("Sets ▾"));
		row->Add(list, wxSizerFlags().Border(wxALL, Theme::Grid(1)));

		auto* edit = new wxButton(this, wxID_ANY, "Options");
		row->Add(edit, wxSizerFlags().Border(wxALL, Theme::Grid(1)));

		layout->Add(row, wxSizerFlags().Expand());
		book_ = new wxNotebook(this, wxID_ANY);
		layout->Add(book_, wxSizerFlags(1).Expand());
		SetSizer(layout);
		SetMinSize(FromDIP(wxSize(240, 180)));

		add->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			static_cast<void>(CreateTabInteractive(this));
		});
		list->Bind(wxEVT_BUTTON, [this, list](wxCommandEvent&) { ShowTabList(list); });
		edit->Bind(wxEVT_BUTTON, [this, edit](wxCommandEvent&) { ShowEditMenu(edit); });

		book_->SetDropTarget(new PayloadDropTarget([this](wxCoord x, wxCoord y, const DragPayload& payload) {
			return DropOnTab(x, y, payload);
		}));
		book_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& event) {
			if (!rebuilding_) {
				if (auto* page = dynamic_cast<QuickTilesetPage*>(book_->GetCurrentPage())) {
					const std::string id = page->TabId();
					Apply([&id](State& state) {
						if (state.active == id) {
							return false;
						}
						state.active = id;
						return true;
					}, ChangeScope::Layout);
				}
			}
			event.Skip();
		});

		Rebuild();
	}

	void QuickTilesetsWindow::QueueRefresh(bool rebuild_tabs) {
		rebuild_queued_ = rebuild_queued_ || rebuild_tabs;
		if (refresh_queued_) {
			return;
		}
		refresh_queued_ = true;
		CallAfter([this]() {
			refresh_queued_ = false;
			if (IsDragging()) {
				return; // BeginDrag queues a refresh when the drag ends
			}
			if (std::exchange(rebuild_queued_, false)) {
				Rebuild();
			} else {
				RefreshPages();
			}
		});
	}

	void QuickTilesetsWindow::Rebuild() {
		rebuilding_ = true;
		book_->DeleteAllPages();
		book_->AddPage(new QuickTilesetPage(book_, {}), "Recent");
		const State& state = GetState();
		const Tab* active = ActiveTab(state);
		size_t selection = 0;
		for (const Tab& tab : state.tabs) {
			book_->AddPage(new QuickTilesetPage(book_, tab.id), wxstr(EscapeMnemonics(tab.name)));
			if (&tab == active) {
				selection = book_->GetPageCount() - 1;
			}
		}
		book_->ChangeSelection(selection);
		rebuilding_ = false;
		Layout();
	}

	void QuickTilesetsWindow::RefreshPages() {
		for (size_t index = 0; index < book_->GetPageCount(); ++index) {
			static_cast<QuickTilesetPage*>(book_->GetPage(index))->RefreshItems();
		}
	}

	void QuickTilesetsWindow::ShowTabList(wxButton* anchor) {
		wxMenu menu;
		const int first_id = wxID_HIGHEST + 1;
		for (size_t index = 0; index < book_->GetPageCount(); ++index) {
			wxString label = book_->GetPageText(index);
			wxMenuItem* item = menu.AppendCheckItem(first_id + static_cast<int>(index), label);
			item->Check(static_cast<int>(index) == book_->GetSelection());
		}
		const int chosen = anchor->GetPopupMenuSelectionFromUser(menu, wxPoint(0, anchor->GetSize().y));
		if (chosen != wxID_NONE) {
			book_->SetSelection(static_cast<size_t>(chosen - first_id));
		}
	}

	void QuickTilesetsWindow::ShowEditMenu(wxButton* anchor) {
		const State& state = GetState();
		const Tab* active = ActiveTab(state);
		const std::string id = active ? active->id : std::string {};
		const std::string name = active ? active->name : std::string {};

		wxMenu menu;
		if (active) {
			menu.Append(ToId(EditMenuId::Rename), "Rename...");
			menu.Append(ToId(EditMenuId::Delete), "Delete set...");
			menu.Append(ToId(EditMenuId::MoveLeft), "Move set left");
			menu.Append(ToId(EditMenuId::MoveRight), "Move set right");
			menu.AppendSeparator();
		}
		menu.Append(ToId(EditMenuId::Float), "Floating window");
		menu.Append(ToId(EditMenuId::DockLeft), "Dock left");
		menu.Append(ToId(EditMenuId::DockRight), "Dock right");

		const int chosen = anchor->GetPopupMenuSelectionFromUser(menu, wxPoint(0, anchor->GetSize().y));
		if (chosen < ToId(EditMenuId::Rename) || chosen > ToId(EditMenuId::DockRight)) {
			return;
		}
		const auto command = static_cast<EditMenuId>(chosen);
		switch (command) {
			case EditMenuId::Rename: {
				wxTextEntryDialog dialog(this, "Name of the set:", "Quick Tilesets", wxString::FromUTF8(name));
				if (dialog.ShowModal() == wxID_OK) {
					const std::string new_name = dialog.GetValue().ToStdString(wxConvUTF8);
					Apply([&](State& s) { return RenameTab(s, id, new_name); }, ChangeScope::Tabs);
				}
				break;
			}
			case EditMenuId::Delete:
				if (wxMessageBox("Delete this Quick Tileset? Items on the map stay unchanged.", "Quick Tilesets", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) == wxYES) {
					Apply([&](State& s) { return DeleteTab(s, id); }, ChangeScope::Tabs);
				}
				break;
			case EditMenuId::MoveLeft:
			case EditMenuId::MoveRight:
				Apply([&](State& s) { return MoveTab(s, id, command == EditMenuId::MoveLeft ? -1 : 1); }, ChangeScope::Tabs);
				break;
			case EditMenuId::Float:
			case EditMenuId::DockLeft:
			case EditMenuId::DockRight: {
				if (!g_gui.aui_manager) {
					break;
				}
				wxAuiPaneInfo& pane = g_gui.aui_manager->GetPane(this);
				if (command == EditMenuId::Float) {
					pane.Float();
				} else if (command == EditMenuId::DockLeft) {
					pane.Dock().Left();
				} else {
					pane.Dock().Right();
				}
				pane.Show();
				g_gui.aui_manager->Update();
				break;
			}
		}
	}

	bool QuickTilesetsWindow::DropOnTab(int x, int y, const DragPayload& payload) {
		const int index = book_->HitTest(wxPoint(x, y));
		// Page 0 is Recent, which is not editable.
		if (index <= 0 || index >= static_cast<int>(book_->GetPageCount())) {
			return false;
		}
		const std::string id = static_cast<QuickTilesetPage*>(book_->GetPage(static_cast<size_t>(index)))->TabId();
		InsertIntoTab(id, payload.items);
		Apply([&id](State& state) {
			if (state.active == id) {
				return false;
			}
			state.active = id;
			return true;
		}, ChangeScope::Tabs);
		return true;
	}

} // namespace QuickTilesets
