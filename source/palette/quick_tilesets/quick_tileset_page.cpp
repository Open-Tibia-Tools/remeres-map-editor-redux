//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tileset_page.h"

#include "app/main.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/quick_tilesets/brush_reference.h"
#include "palette/quick_tilesets/quick_tileset_drop_target.h"
#include "palette/quick_tilesets/quick_tilesets.h"
#include "ui/theme.h"

#include <wx/splitter.h>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace QuickTilesets {

	namespace {

		constexpr int kGridIconSize = 32;

	} // namespace

	QuickTilesetPage::QuickTilesetPage(wxWindow* parent, std::string tab_id) :
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

		Bind(EVT_VIRTUAL_BRUSH_GRID_PICKED, &QuickTilesetPage::OnPicked, this);
		Bind(EVT_VIRTUAL_BRUSH_GRID_BEGIN_DRAG, &QuickTilesetPage::OnBeginDrag, this);
		Bind(wxEVT_CONTEXT_MENU, &QuickTilesetPage::OnContextMenu, this);

		RefreshItems();
	}

	void QuickTilesetPage::RefreshItems() {
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

	wxWindow* QuickTilesetPage::MakeGridPanel(const char* title, DynamicTilesetDefinition& set, VirtualBrushGrid*& grid) {
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

	void QuickTilesetPage::OnPicked(wxCommandEvent& event) {
		auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject());
		if (grid == brush_grid_) {
			raw_grid_->ClearSelection();
		} else if (grid == raw_grid_) {
			brush_grid_->ClearSelection();
		}
		RememberUserPick(static_cast<Brush*>(event.GetClientData()));
	}

	void QuickTilesetPage::OnBeginDrag(wxCommandEvent& event) {
		if (auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject())) {
			const std::vector<Brush*> brushes = grid->GetSelectedBrushes();
			BeginDrag(grid, brushes, tab_id_);
		}
	}

	void QuickTilesetPage::OnContextMenu(wxContextMenuEvent& event) {
		auto* grid = dynamic_cast<VirtualBrushGrid*>(event.GetEventObject());
		if (!grid) {
			event.Skip();
			return;
		}
		const wxPoint position = event.GetPosition() == wxDefaultPosition ? grid->GetSelectedCellPosition() : grid->ScreenToClient(event.GetPosition());
		const std::vector<Brush*> brushes = grid->GetSelectedBrushes();
		ShowBrushContextMenu(grid, position, brushes, tab_id_);
	}

	bool QuickTilesetPage::Drop(VirtualBrushGrid* grid, wxCoord x, wxCoord y, const DragPayload& payload) {
		const std::string destination = tab_id_;
		if (destination.empty()) {
			// Recent is not editable: a drop there starts a new set. The name dialog runs
			// after the drag loop has ended (a modal dialog inside it is unusable on GTK).
			CallAfter([this, items = payload.items]() {
				if (const std::string id = CreateTabInteractive(this); !id.empty()) {
					InsertIntoTab(id, items);
				}
			});
			return true;
		}
		std::string before;
		if (Brush* target = grid->BrushAt(x, y)) {
			if (auto reference = MakeReference(*target)) {
				before = reference->key;
			}
		}
		const bool reorder = payload.tab == destination;
		InsertIntoTab(destination, payload.items, before, reorder);
		return true;
	}

} // namespace QuickTilesets
