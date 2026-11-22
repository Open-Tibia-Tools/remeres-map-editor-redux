//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "ui/autoborder_debug_window.h"

#include "app/settings.h"
#include "brushes/ground/auto_border.h"
#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "game/item.h"
#include "game/items.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "rendering/ui/map_display.h"
#include "ui/gui.h"
#include "ui/map_tab.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <array>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

	constexpr int kRefreshIntervalMs = 250;

	// Same order as the neighbour array of GroundBorderCalculator.
	constexpr std::array<std::string_view, 8> kNeighbourNames = { "NW", "N", "NE", "W", "E", "SW", "S", "SE" };

	std::string BrushName(const GroundBrush* brush) {
		return brush ? std::format("\"{}\" (id {})", brush->getName(), brush->getID()) : std::string("none");
	}

	std::string ItemName(uint32_t id) {
		const std::string name = g_items[static_cast<int>(id)].name;
		return name.empty() ? std::format("{}", id) : std::format("{} \"{}\"", id, name);
	}

	std::string AlignmentNames(uint32_t alignment) {
		std::string result;
		for (std::size_t i = 0; i < kNeighbourNames.size(); ++i) {
			if (alignment & (1u << i)) {
				if (!result.empty()) {
					result += ' ';
				}
				result += kNeighbourNames[i];
			}
		}
		return result.empty() ? std::string("-") : result;
	}

	std::string_view VerdictText(BorderUpdate::RewriteVerdict verdict) {
		switch (verdict) {
			case BorderUpdate::RewriteVerdict::Rewritable:
				return "removed: owned by the active brush only";
			case BorderUpdate::RewriteVerdict::NotOwnedByActive:
				return "kept: the active brush does not own this id";
			case BorderUpdate::RewriteVerdict::SharedWithContext:
				return "kept: id shared with a context brush";
			case BorderUpdate::RewriteVerdict::JointBorder:
				return "kept: specific-case result fused with a context brush's piece";
		}
		return "?";
	}

	std::string_view OutcomeText(const GroundBorderTrace::SpecificCase& entry) {
		if (!entry.owned_by_active) {
			return "skipped: block belongs to another brush";
		}
		if (!entry.outcome) {
			return "not traced";
		}
		switch (*entry.outcome) {
			case BorderUpdate::SpecificCaseOutcome::Applied:
				return "applied";
			case BorderUpdate::SpecificCaseOutcome::NotMatched:
				return "not matched";
			case BorderUpdate::SpecificCaseOutcome::Protected:
				return "refused: touches a kept item";
		}
		return "?";
	}

	std::string ItemFlags(const Item& item) {
		if (item.isBorder()) {
			return "border";
		}
		return item.isBorderLayer() ? "border layer" : "item";
	}

	void AppendStack(std::string& out, const Tile& tile) {
		if (tile.ground) {
			out += std::format("  ground {}\n", ItemName(tile.ground->getID()));
		}
		if (tile.items.empty()) {
			out += "  (no items)\n";
		}
		for (std::size_t i = 0; i < tile.items.size(); ++i) {
			const Item& item = *tile.items[i];
			out += std::format("  [{}] {} ({})\n", i, ItemName(item.getID()), ItemFlags(item));
		}
	}

	std::vector<uint32_t> ItemIds(const Tile& tile) {
		std::vector<uint32_t> ids;
		ids.reserve(tile.items.size());
		for (const auto& item : tile.items) {
			ids.push_back(item->getID());
		}
		return ids;
	}

	void AppendOwnership(std::string& out, const GroundBrush& brush) {
		const BorderUpdate::BorderOwnership& ownership = brush.getBorderOwnership();
		out += std::format("  {}: {} ids:", BrushName(&brush), ownership.items.size());
		for (const uint32_t id : ownership.items.values()) {
			out += std::format(" {}", id);
		}
		out += '\n';
		for (const BorderUpdate::FusedBorder& fused : ownership.fused) {
			out += std::format("    fused {} replaces {} when", fused.result_id, fused.replaced_id);
			for (const uint32_t source : fused.sources) {
				out += std::format(" {}", source);
			}
			out += " are present\n";
		}
	}

	std::string BuildReport(const Tile& tile, const Tile& result, const Tile& classic, const GroundBorderTrace& trace, const BorderUpdate::Options& options) {
		std::string out;
		const Position& pos = tile.getPosition();
		out += std::format("Tile {}, {}, {}\n", pos.x, pos.y, pos.z);
		out += std::format("Mode: {} / {}{}\n", options.preservesOtherBrushes() ? "current brush only" : "all brushes", options.placement == BorderUpdate::Placement::Top ? "top" : "bottom", trace.classic ? " (classic path)" : "");
		if (!g_settings.getInteger(Config::USE_AUTOMAGIC)) {
			out += "Note: automagic is off, so painting does not recalculate borders.\n";
		}
		out += std::format("Optional border flag: {}\n", trace.optional_border_before ? "set" : "not set");

		out += "\nTile ground brush and neighbours\n";
		out += std::format("  tile: {}\n", BrushName(trace.tile_brush));
		for (std::size_t i = 0; i < trace.neighbours.size(); ++i) {
			out += std::format("  {:<2}: {}\n", kNeighbourNames[i], BrushName(trace.neighbours[i]));
		}

		out += "\nOwnership\n";
		if (options.active_brush) {
			AppendOwnership(out, *options.active_brush);
		} else {
			out += "  no active ground brush\n";
		}
		if (options.preservesOtherBrushes()) {
			out += "Context brushes (their ids are protected)\n";
			if (trace.context_brushes.empty()) {
				out += "  none\n";
			}
			for (const GroundBrush* brush : trace.context_brushes) {
				AppendOwnership(out, *brush);
			}
		}

		out += "\nBorder clusters (z order, lowest first)\n";
		if (trace.clusters.empty()) {
			out += "  none\n";
		}
		for (const GroundBorderTrace::Cluster& cluster : trace.clusters) {
			const std::string border = cluster.border ? std::format("border {} (group {})", cluster.border->id, cluster.border->group) : std::string("no border");
			out += std::format("  {}, z {}, towards {}: {}\n", border, cluster.z, AlignmentNames(cluster.alignment), cluster.generated ? "generated" : "left to its own brush");
		}

		out += "\nExisting items\n";
		if (trace.before.empty()) {
			out += "  (no items)\n";
		}
		for (std::size_t i = 0; i < trace.before.size(); ++i) {
			const GroundBorderTrace::ExistingItem& item = trace.before[i];
			std::string decision;
			if (!item.is_border) {
				decision = item.is_border_layer ? "kept: border layer, not a brush border" : "kept: not a border";
			} else if (item.verdict) {
				decision = std::string(VerdictText(*item.verdict));
			} else {
				decision = item.removed ? "removed: every brush border is rebuilt" : "kept";
			}
			out += std::format("  [{}] {}: {}\n", i, ItemName(item.id), decision);
		}

		out += "\nGenerated pieces (stack order, bottom first)\n";
		if (trace.generated.empty()) {
			out += "  none\n";
		}
		for (const GroundBorderTrace::GeneratedPiece& piece : trace.generated) {
			out += std::format("  {}: {}\n", ItemName(piece.id), piece.skipped ? "skipped, already shown by a kept item" : "inserted");
		}
		out += std::format("  inserted at index {} of the kept items\n", trace.insertion_index);

		out += "\nSpecific cases\n";
		if (trace.specific_cases.empty()) {
			out += "  none\n";
		}
		for (const GroundBorderTrace::SpecificCase& entry : trace.specific_cases) {
			const GroundBrush::SpecificCaseBlock& block = *entry.block->specific_cases[entry.case_index];
			std::string match;
			for (const uint16_t id : block.items_to_match) {
				match += std::format(" {}", id);
			}
			const std::string border = entry.block->autoborder ? std::format("border {}", entry.block->autoborder->id) : std::string("no border");
			out += std::format("  {} case {}: match{}{}, {} -> {}{}{}: {}\n", border, entry.case_index, match, block.match_group ? std::format(" (group {})", block.match_group) : std::string(), block.to_replace_id, block.with_id, block.delete_all ? ", delete all" : "", block.keepBorder ? ", keep border" : "", OutcomeText(entry));
		}

		out += "\nResult\n";
		AppendStack(out, result);
		if (ItemIds(result) == ItemIds(tile)) {
			out += "  same as the tile now\n";
		}

		if (!trace.classic) {
			out += "\nClassic recalculation (all brushes, bottom) would give\n";
			AppendStack(out, classic);
		}

		return out;
	}

} // namespace

AutoborderDebugWindow* AutoborderDebugWindow::instance = nullptr;

void AutoborderDebugWindow::Toggle(wxWindow* parent) {
	if (!instance) {
		instance = new AutoborderDebugWindow(parent);
	}
	if (instance->IsShown()) {
		instance->StopTracking();
		instance->Hide();
		return;
	}
	instance->Show();
	instance->Raise();
	instance->StartTracking();
}

AutoborderDebugWindow::AutoborderDebugWindow(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Autoborder Debug", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	timer(this) {
	wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* options_row = new wxBoxSizer(wxHORIZONTAL);
	follow_settings = new wxCheckBox(this, wxID_ANY, "Use brush settings");
	follow_settings->SetValue(true);
	follow_settings->SetToolTip("Use the scope and placement from the brush tool options, like painting does.");
	options_row->Add(follow_settings, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));

	options_row->Add(new wxStaticText(this, wxID_ANY, "Scope:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));
	scope_choice = new wxChoice(this, wxID_ANY);
	scope_choice->Append("All brushes");
	scope_choice->Append("Current brush only");
	options_row->Add(scope_choice, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));

	options_row->Add(new wxStaticText(this, wxID_ANY, "Placement:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));
	placement_choice = new wxChoice(this, wxID_ANY);
	placement_choice->Append("Bottom");
	placement_choice->Append("Top");
	options_row->Add(placement_choice, wxSizerFlags().CenterVertical());
	root->Add(options_row, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

	wxBoxSizer* actions_row = new wxBoxSizer(wxHORIZONTAL);
	lock_tile = new wxCheckBox(this, wxID_ANY, "Lock tile");
	lock_tile->SetToolTip("Keep the current tile instead of following the map cursor.");
	actions_row->Add(lock_tile, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));
	apply_button = new wxButton(this, wxID_ANY, "Apply to this tile");
	apply_button->SetToolTip("Recalculate the borders of this tile with these options, as one undoable action.");
	actions_row->Add(apply_button, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));
	wxButton* copy_button = new wxButton(this, wxID_ANY, "Copy report");
	actions_row->Add(copy_button, wxSizerFlags().CenterVertical());
	root->Add(actions_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8)));

	summary = new wxStaticText(this, wxID_ANY, wxEmptyString);
	root->Add(summary, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8)));

	report = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxHSCROLL);
	report->SetFont(wxFont(wxFontInfo().Family(wxFONTFAMILY_TELETYPE)));
	root->Add(report, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8)));

	SetSizer(root);
	SetSize(FromDIP(wxSize(640, 720)));

	SyncChoicesFromOptions(CurrentOptions(g_gui.GetCurrentBrush()));

	follow_settings->Bind(wxEVT_CHECKBOX, &AutoborderDebugWindow::OnOptionChanged, this);
	scope_choice->Bind(wxEVT_CHOICE, &AutoborderDebugWindow::OnOptionChanged, this);
	placement_choice->Bind(wxEVT_CHOICE, &AutoborderDebugWindow::OnOptionChanged, this);
	lock_tile->Bind(wxEVT_CHECKBOX, &AutoborderDebugWindow::OnLockChanged, this);
	apply_button->Bind(wxEVT_BUTTON, &AutoborderDebugWindow::OnApply, this);
	copy_button->Bind(wxEVT_BUTTON, &AutoborderDebugWindow::OnCopy, this);
	Bind(wxEVT_TIMER, &AutoborderDebugWindow::OnTimer, this, timer.GetId());
	Bind(wxEVT_CLOSE_WINDOW, &AutoborderDebugWindow::OnClose, this);
}

AutoborderDebugWindow::~AutoborderDebugWindow() {
	timer.Stop();
	if (instance == this) {
		instance = nullptr;
	}
}

void AutoborderDebugWindow::StartTracking() {
	last_report.clear();
	UpdateReport();
	timer.Start(kRefreshIntervalMs);
}

void AutoborderDebugWindow::StopTracking() {
	timer.Stop();
}

void AutoborderDebugWindow::OnClose(wxCloseEvent& event) {
	if (event.CanVeto()) {
		event.Veto();
		StopTracking();
		Hide();
		return;
	}
	event.Skip();
}

void AutoborderDebugWindow::OnTimer(wxTimerEvent& WXUNUSED(event)) {
	UpdateReport();
}

void AutoborderDebugWindow::OnOptionChanged(wxCommandEvent& WXUNUSED(event)) {
	UpdateReport();
}

void AutoborderDebugWindow::OnLockChanged(wxCommandEvent& WXUNUSED(event)) {
	locked_position.reset();
	if (lock_tile->GetValue()) {
		locked_position = TargetPosition();
	}
	UpdateReport();
}

void AutoborderDebugWindow::OnCopy(wxCommandEvent& WXUNUSED(event)) {
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(report->GetValue()));
		wxTheClipboard->Close();
	}
}

std::optional<Position> AutoborderDebugWindow::TargetPosition() const {
	if (locked_position) {
		return locked_position;
	}
	MapTab* tab = g_gui.GetCurrentMapTab();
	if (!tab || !tab->GetCanvas()) {
		return std::nullopt;
	}
	return tab->GetCanvas()->GetCursorPosition();
}

BorderUpdate::Options AutoborderDebugWindow::CurrentOptions(const Brush* brush) const {
	if (follow_settings->GetValue()) {
		return DrawOperations::groundBorderOptions(brush);
	}
	BorderUpdate::Options options;
	options.placement = placement_choice->GetSelection() == 1 ? BorderUpdate::Placement::Top : BorderUpdate::Placement::Bottom;
	if (scope_choice->GetSelection() == 1 && brush && brush->is<GroundBrush>()) {
		options.scope = BorderUpdate::Scope::CurrentBrushOnly;
		options.active_brush = brush->as<GroundBrush>();
	}
	return options;
}

void AutoborderDebugWindow::SyncChoicesFromOptions(const BorderUpdate::Options& options) {
	// Without a ground brush the settings scope still shows, but means all brushes.
	const bool scoped = options.scope == BorderUpdate::Scope::CurrentBrushOnly || (follow_settings->GetValue() && g_settings.getBoolean(Config::AUTOBORDER_CURRENT_BRUSH_ONLY));
	scope_choice->SetSelection(scoped ? 1 : 0);
	placement_choice->SetSelection(options.placement == BorderUpdate::Placement::Top ? 1 : 0);
}

void AutoborderDebugWindow::UpdateReport() {
	Brush* brush = g_gui.GetCurrentBrush();
	const bool follow = follow_settings->GetValue();
	if (follow) {
		SyncChoicesFromOptions(DrawOperations::groundBorderOptions(brush));
	}
	scope_choice->Enable(!follow);
	placement_choice->Enable(!follow);

	const BorderUpdate::Options options = CurrentOptions(brush);
	const GroundBrush* ground_brush = brush && brush->is<GroundBrush>() ? brush->as<GroundBrush>() : nullptr;

	wxString header = wxString::FromUTF8(std::format("Brush: {}", ground_brush ? BrushName(ground_brush) : std::string("not a ground brush")));
	if (!ground_brush) {
		header += " - \"current brush only\" acts like \"all brushes\"";
	}

	Editor* editor = g_gui.GetCurrentEditor();
	const std::optional<Position> position = TargetPosition();
	const Tile* tile = editor && position ? editor->map.getTile(*position) : nullptr;

	wxString text;
	if (!editor) {
		text = "No map is open.";
	} else if (!tile) {
		text = "No tile under the cursor. Move the cursor over a tile on the map.";
	} else {
		std::unique_ptr<Tile> result = TileOperations::deepCopy(tile, editor->map);
		GroundBorderTrace trace;
		BorderUpdate::Options traced_options = options;
		// The trace always names the active brush, even when it does not scope the update.
		if (!traced_options.active_brush) {
			traced_options.active_brush = ground_brush;
		}
		GroundBorderCalculator::calculate(&editor->map, result.get(), options, &trace);

		std::unique_ptr<Tile> classic = TileOperations::deepCopy(tile, editor->map);
		GroundBorderCalculator::calculate(&editor->map, classic.get(), {});

		text = wxString::FromUTF8(BuildReport(*tile, *result, *classic, trace, traced_options));
	}

	summary->SetLabel(header);
	apply_button->Enable(tile != nullptr);
	if (text != last_report) {
		last_report = text;
		report->ChangeValue(text);
	}
}

void AutoborderDebugWindow::OnApply(wxCommandEvent& WXUNUSED(event)) {
	Editor* editor = g_gui.GetCurrentEditor();
	const std::optional<Position> position = TargetPosition();
	Tile* tile = editor && position ? editor->map.getTile(*position) : nullptr;
	if (!tile) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor->map);
	GroundBorderCalculator::calculate(&editor->map, new_tile.get(), CurrentOptions(g_gui.GetCurrentBrush()));
	if (ItemIds(*new_tile) == ItemIds(*tile)) {
		return;
	}

	std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_BORDERIZE);
	action->addChange(std::make_unique<Change>(std::move(new_tile)));
	editor->addAction(std::move(action));
	g_gui.RefreshView();
	UpdateReport();
}
