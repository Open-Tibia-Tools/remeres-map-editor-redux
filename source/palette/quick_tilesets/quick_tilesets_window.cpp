//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tilesets_window.h"

#include "app/main.h"
#include "game/material_database.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/quick_tilesets/brush_reference.h"
#include "palette/quick_tilesets/quick_tilesets.h"
#include "ui/gui.h"
#include "ui/theme.h"

#include <wx/aui/aui.h>
#include <wx/dnd.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/splitter.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <format>
#include <functional>

namespace QuickTilesets {

	namespace {

		constexpr int kGridIconSize = 32;

		// Accepts Quick Tileset drags of the loaded client version and hands them to `handler`.
		class PayloadDropTarget : public wxDropTarget {
		public:
			using Handler = std::function<bool(wxCoord, wxCoord, const DragPayload&)>;

			explicit PayloadDropTarget(Handler handler) :
				data_(new wxCustomDataObject(wxDataFormat(DragFormatId()))),
				handler_(std::move(handler)) {
				SetDataObject(data_);
			}

			wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult) override {
				if (!GetData()) {
					return wxDragNone;
				}
				const auto payload = ParseDragPayload(std::string_view(static_cast<const char*>(data_->GetData()), data_->GetSize()));
				if (!payload || payload->version != LoadedVersion() || payload->items.empty()) {
					return wxDragNone;
				}
				return handler_(x, y, *payload) ? wxDragCopy : wxDragNone;
			}

		private:
			wxCustomDataObject* data_; // owned by wxDropTarget
			Handler handler_;
		};

		// Inserts `items` into tab `id` before `before`; returns whether the tab changed.
		bool InsertIntoTab(std::string_view id, const std::vector<Item>& items, std::string_view before, bool reorder) {
			return Apply([&](State& state) {
				Tab* tab = FindTab(state, id);
				if (!tab) {
					return false;
				}
				const auto previous = tab->items;
				Insert(*tab, items, before, reorder);
				return tab->items != previous;
			}, ChangeScope::Items);
		}

	} // namespace

	class QuickTilesetsWindow::Page : public wxPanel {
	public:
		Page(wxWindow* parent, std::string tab_id) :
			wxPanel(parent), tab_id_(std::move(tab_id)) {
			SetBackgroundColour(Theme::Get(Theme::Role::Surface));
			auto* layout = new wxBoxSizer(wxVERTICAL);
			split_ = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
			split_->SetMinimumPaneSize(FromDIP(70));
			split_->SetSashGravity(GetState().split);

			wxWindow* brush_panel = MakeGridPanel("Brushes", brushes_, brush_grid_);
			wxWindow* raw_panel = MakeGridPanel("RAW", raws_, raw_grid_);
			split_->SplitVertically(brush_panel, raw_panel);
			layout->Add(split_, wxSizerFlags(1).Expand());

			hint_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
			hint_->SetForegroundColour(Theme::Get(Theme::Role::Text));
			layout->Add(hint_, wxSizerFlags().Expand().Border(wxALL, Theme::Grid(1)));
			SetSizer(layout);

			split_->Bind(wxEVT_SPLITTER_SASH_POS_CHANGED, [this](wxSplitterEvent& event) {
				const int width = split_->GetClientSize().x;
				if (width > 0) {
					const double split = std::clamp(static_cast<double>(event.GetSashPosition()) / width, 0.15, 0.85);
					Apply([split](State& state) {
						if (state.split == split) {
							return false;
						}
						state.split = split;
						return true;
					}, ChangeScope::Layout);
				}
				event.Skip();
			});
			CallAfter([this]() {
				split_->SetSashPosition(static_cast<int>(split_->GetClientSize().x * GetState().split));
			});

			Bind(EVT_VIRTUAL_BRUSH_GRID_PICKED, &Page::OnPicked, this);
			Bind(EVT_VIRTUAL_BRUSH_GRID_BEGIN_DRAG, &Page::OnBeginDrag, this);
			Bind(wxEVT_CONTEXT_MENU, &Page::OnContextMenu, this);

			RefreshItems();
		}

		[[nodiscard]] const std::string& TabId() const {
			return tab_id_;
		}

		void RefreshItems() {
			const State& state = GetState();
			const Tab* tab = tab_id_.empty() ? nullptr : FindTab(state, tab_id_);
			const std::vector<Item>& items = tab ? tab->items : state.recent;

			brushes_.brushes.clear();
			raws_.brushes.clear();
			size_t missing = 0;
			for (const Item& item : items) {
				if (Brush* brush = Resolve(item)) {
					(item.isRaw() ? raws_ : brushes_).brushes.push_back(brush);
				} else {
					++missing;
				}
			}
			brush_grid_->ReloadBrushes();
			raw_grid_->ReloadBrushes();

			if (missing > 0) {
				hint_->SetLabel(wxstr(std::format("Unavailable in this data: {} (kept in the set)", missing)));
			} else if (tab_id_.empty()) {
				hint_->SetLabel(wxstr(std::format("Brushes: {} / {}   RAW: {} / {}", brushes_.size(), kRecentBrushLimit, raws_.size(), kRecentRawLimit)));
			} else {
				hint_->SetLabel(wxstr(std::format("Brushes: {}   RAW: {}", brushes_.size(), raws_.size())));
			}
			Layout();
		}

	private:
		wxWindow* MakeGridPanel(const char* title, DynamicTilesetDefinition& set, VirtualBrushGrid*& grid) {
			set.name = title;
			auto* panel = new wxPanel(split_);
			panel->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
			auto* sizer = new wxBoxSizer(wxVERTICAL);
			auto* label = new wxStaticText(panel, wxID_ANY, title);
			label->SetForegroundColour(Theme::Get(Theme::Role::Text));
			sizer->Add(label, wxSizerFlags().Border(wxALL, Theme::Grid(1)));

			grid = new VirtualBrushGrid(panel, &set, kGridIconSize);
			grid->SetMinSize(FromDIP(wxSize(60, 60)));
			VirtualBrushGrid* target = grid;
			grid->SetDropTarget(new PayloadDropTarget([this, target](wxCoord x, wxCoord y, const DragPayload& payload) {
				return Drop(target, x, y, payload);
			}));
			sizer->Add(grid, wxSizerFlags(1).Expand());
			panel->SetSizer(sizer);
			return panel;
		}

		void OnPicked(wxCommandEvent& event) {
			auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject());
			if (grid == brush_grid_) {
				raw_grid_->ClearSelection();
			} else if (grid == raw_grid_) {
				brush_grid_->ClearSelection();
			}
			RememberUserPick(static_cast<Brush*>(event.GetClientData()));
		}

		void OnBeginDrag(wxCommandEvent& event) {
			if (auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject())) {
				const std::vector<Brush*> brushes = grid->GetSelectedBrushes();
				BeginDrag(grid, brushes, tab_id_);
			}
		}

		void OnContextMenu(wxContextMenuEvent& event) {
			auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject());
			if (!grid) {
				event.Skip();
				return;
			}
			const wxPoint position = event.GetPosition() == wxDefaultPosition ? grid->GetSelectedCellPosition() : grid->ScreenToClient(event.GetPosition());
			const std::vector<Brush*> brushes = grid->GetSelectedBrushes();
			ShowBrushContextMenu(grid, position, brushes, tab_id_);
		}

		bool Drop(VirtualBrushGrid* grid, wxCoord x, wxCoord y, const DragPayload& payload) {
			std::string destination = tab_id_;
			std::string before;
			if (destination.empty()) {
				// Recent is not editable: a drop there starts a new set. The name dialog runs
				// after the drag loop has ended (a modal dialog inside it is unusable on GTK).
				CallAfter([this, items = payload.items]() {
					if (const std::string id = CreateTabInteractive(this); !id.empty()) {
						InsertIntoTab(id, items, {}, false);
					}
				});
				return true;
			}
			if (Brush* target = grid->BrushAt(x, y)) {
				if (auto reference = MakeReference(*target)) {
					before = reference->key;
				}
			}
			const bool reorder = payload.tab == destination;
			InsertIntoTab(destination, payload.items, before, reorder);
			return true;
		}

		std::string tab_id_;
		wxSplitterWindow* split_ = nullptr;
		DynamicTilesetDefinition brushes_;
		DynamicTilesetDefinition raws_;
		VirtualBrushGrid* brush_grid_ = nullptr;
		VirtualBrushGrid* raw_grid_ = nullptr;
		wxStaticText* hint_ = nullptr;
	};

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
				if (auto* page = dynamic_cast<Page*>(book_->GetCurrentPage())) {
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
		book_->AddPage(new Page(book_, {}), "Recent");
		const State& state = GetState();
		const Tab* active = ActiveTab(state);
		size_t selection = 0;
		for (const Tab& tab : state.tabs) {
			book_->AddPage(new Page(book_, tab.id), wxstr(EscapeMnemonics(tab.name)));
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
			static_cast<Page*>(book_->GetPage(index))->RefreshItems();
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
		enum : int {
			kRename = wxID_HIGHEST + 1,
			kDelete,
			kMoveLeft,
			kMoveRight,
			kFloat,
			kDockLeft,
			kDockRight,
		};

		const State& state = GetState();
		const Tab* active = ActiveTab(state);
		const std::string id = active ? active->id : std::string {};
		const std::string name = active ? active->name : std::string {};

		wxMenu menu;
		if (active) {
			menu.Append(kRename, "Rename...");
			menu.Append(kDelete, "Delete set...");
			menu.Append(kMoveLeft, "Move set left");
			menu.Append(kMoveRight, "Move set right");
			menu.AppendSeparator();
		}
		menu.Append(kFloat, "Floating window");
		menu.Append(kDockLeft, "Dock left");
		menu.Append(kDockRight, "Dock right");

		const int chosen = anchor->GetPopupMenuSelectionFromUser(menu, wxPoint(0, anchor->GetSize().y));
		switch (chosen) {
			case kRename: {
				wxTextEntryDialog dialog(this, "Name of the set:", "Quick Tilesets", wxString::FromUTF8(name));
				if (dialog.ShowModal() == wxID_OK) {
					const std::string new_name = dialog.GetValue().ToStdString(wxConvUTF8);
					Apply([&](State& s) { return RenameTab(s, id, new_name); }, ChangeScope::Tabs);
				}
				break;
			}
			case kDelete:
				if (wxMessageBox("Delete this Quick Tileset? Items on the map stay unchanged.", "Quick Tilesets", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) == wxYES) {
					Apply([&](State& s) { return DeleteTab(s, id); }, ChangeScope::Tabs);
				}
				break;
			case kMoveLeft:
			case kMoveRight:
				Apply([&](State& s) { return MoveTab(s, id, chosen == kMoveLeft ? -1 : 1); }, ChangeScope::Tabs);
				break;
			case kFloat:
			case kDockLeft:
			case kDockRight: {
				if (!g_gui.aui_manager) {
					break;
				}
				wxAuiPaneInfo& pane = g_gui.aui_manager->GetPane(this);
				if (chosen == kFloat) {
					pane.Float();
				} else if (chosen == kDockLeft) {
					pane.Dock().Left();
				} else {
					pane.Dock().Right();
				}
				pane.Show();
				g_gui.aui_manager->Update();
				break;
			}
			default:
				break;
		}
	}

	bool QuickTilesetsWindow::DropOnTab(int x, int y, const DragPayload& payload) {
		const int index = book_->HitTest(wxPoint(x, y));
		// Page 0 is Recent, which is not editable.
		if (index <= 0 || index >= static_cast<int>(book_->GetPageCount())) {
			return false;
		}
		const std::string id = static_cast<Page*>(book_->GetPage(static_cast<size_t>(index)))->TabId();
		InsertIntoTab(id, payload.items, {}, false);
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
