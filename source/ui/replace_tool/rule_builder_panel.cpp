#include "app/main.h"
#include "ui/replace_tool/rule_builder_panel.h"
#include "ui/replace_tool/brush_auto_matcher.h"
#include "ui/replace_tool/rule_chance_dialog.h"
#include "item_definitions/core/item_definition_store.h"
#include "brushes/brush.h"
#include "util/image_manager.h"
#include "ui/theme.h"

#include <algorithm>
#include <format>

using namespace rme::replace_tool;

namespace {

constexpr const char* kDragPrefix = "RME_ITEM:";
constexpr const char* kRuleCellPrefix = "RME_RULE:";

enum MenuId {
	ID_ACTION_REPLACE = 1,
	ID_ACTION_ADD,
	ID_ACTION_REMOVE,
	ID_CHANCE,
	ID_REMOVE_CELL,
	ID_DUPLICATE_ROW,
	ID_REMOVE_ROW,
};

std::vector<uint16_t> ParseItemIds(const wxString& payload) {
	std::vector<uint16_t> ids;
	for (const wxString& part : wxSplit(payload, ',')) {
		unsigned long value = 0;
		if (part.ToULong(&value) && value > 0 && value <= 0xFFFF) {
			ids.push_back(static_cast<uint16_t>(value));
		}
	}
	return ids;
}

std::string ItemName(uint16_t id) {
	const auto definition = g_item_definitions.get(id);
	if (definition && !definition.name().empty()) {
		return std::format("{} - {}", id, definition.name());
	}
	return std::format("{}", id);
}

} // namespace

RuleBuilderPanel::ItemDropTarget::ItemDropTarget(RuleBuilderPanel* panel) :
	m_panel(panel) { }

bool RuleBuilderPanel::ItemDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data) {
	m_panel->ClearDragHover();

	if (data.StartsWith(kRuleCellPrefix)) {
		return m_panel->ApplyRuleMove(x, y, data.AfterFirst(':'));
	}

	if (!data.StartsWith(kDragPrefix)) {
		return false;
	}

	const std::vector<uint16_t> ids = ParseItemIds(data.AfterFirst(':'));
	if (ids.empty()) {
		return false;
	}
	m_panel->ApplyDrop(x, y, ids);
	return true;
}

wxDragResult RuleBuilderPanel::ItemDropTarget::OnDragOver(wxCoord x, wxCoord y, wxDragResult def) {
	m_panel->UpdateDragHover(x, y);
	return def;
}

void RuleBuilderPanel::ItemDropTarget::OnLeave() {
	m_panel->ClearDragHover();
}

RuleBuilderPanel::RuleBuilderPanel(wxWindow* parent, Listener* listener) :
	NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
	m_listener(listener) {

	SetDropTarget(new ItemDropTarget(this));
	SetScrollStep(40);

	Bind(wxEVT_LEFT_DOWN, &RuleBuilderPanel::OnLeftDown, this);
	Bind(wxEVT_LEFT_UP, &RuleBuilderPanel::OnLeftUp, this);
	Bind(wxEVT_LEFT_DCLICK, &RuleBuilderPanel::OnLeftDoubleClick, this);
	Bind(wxEVT_MOTION, &RuleBuilderPanel::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &RuleBuilderPanel::OnLeave, this);
	Bind(wxEVT_CONTEXT_MENU, &RuleBuilderPanel::OnContextMenu, this);
	Bind(wxEVT_KEY_DOWN, &RuleBuilderPanel::OnKeyDown, this);
	Bind(wxEVT_SIZE, &RuleBuilderPanel::OnSize, this);
}

RuleBuilderPanel::~RuleBuilderPanel() = default;

wxSize RuleBuilderPanel::DoGetBestClientSize() const {
	return FromDIP(wxSize(560, 420));
}

void RuleBuilderPanel::Clear() {
	m_model.Clear();
	m_selection = {};
	NotifyRulesChanged();
}

void RuleBuilderPanel::SetRules(const std::vector<RuleGroup>& rules) {
	m_model.SetRules(rules);
	m_selection = {};
	NotifyRulesChanged();
}

std::vector<RuleGroup> RuleBuilderPanel::GetRules() const {
	return m_model.GetRules();
}

void RuleBuilderPanel::AddRuleWithSource(uint16_t itemId) {
	m_model.AddRuleWithSource(itemId);
	m_selection = { static_cast<int>(m_model.GetRuleCount()) - 1, itemId != 0 ? 0 : -1, -1 };
	NotifyRulesChanged();
}

bool RuleBuilderPanel::AddRuleIfNewSource(uint16_t itemId) {
	if (m_model.AddRuleIfNewSource(itemId)) {
		m_selection = { static_cast<int>(m_model.GetRuleCount()) - 1, itemId != 0 ? 0 : -1, -1 };
		NotifyRulesChanged();
		return true;
	}
	return false;
}

void RuleBuilderPanel::AddRulesWithSources(const std::vector<uint16_t>& itemIds) {
	if (m_model.AddRulesWithSources(itemIds) > 0) {
		m_selection = { static_cast<int>(m_model.GetRuleCount()) - 1, 0, -1 };
		NotifyRulesChanged();
	}
}

size_t RuleBuilderPanel::AutoMatchUnmatchedRules(Brush* targetBrush) {
	const size_t matched = BrushAutoMatcher::AutoMatchUnmatchedRules(m_model, targetBrush);
	if (matched > 0) {
		NotifyRulesChanged();
	}
	return matched;
}

void RuleBuilderPanel::SetShowLabels(bool show) {
	if (m_showLabels == show) {
		return;
	}
	m_showLabels = show;
	m_labelCache.Clear();
	LayoutRows();
	Refresh();
}

void RuleBuilderPanel::SetIconSize(int sizePx) {
	sizePx = std::clamp(sizePx, 16, 128);
	if (m_iconSize == sizePx) {
		return;
	}
	m_iconSize = sizePx;
	m_labelCache.Clear();
	LayoutRows();
	Refresh();
}

void RuleBuilderPanel::Undo() {
	if (m_model.Undo()) {
		m_selection = {};
		NotifyRulesChanged();
	}
}

void RuleBuilderPanel::Redo() {
	if (m_model.Redo()) {
		m_selection = {};
		NotifyRulesChanged();
	}
}

void RuleBuilderPanel::LayoutRows() {
	const int clientW = GetClientSize().x;
	int rowPitch = 40;
	m_layouts = RuleBuilderLayout::ComputeLayout(
		m_model,
		clientW,
		m_iconSize,
		m_showLabels,
		m_rowsBottom,
		m_totalHeight,
		rowPitch
	);
	SetScrollStep(rowPitch);
	UpdateScrollbar(m_totalHeight);
}

wxRect RuleBuilderPanel::DropZoneRect() const {
	const int clientW = std::max(GetClientSize().x, FromDIP(320));
	return RuleBuilderLayout::DropZoneRect(m_rowsBottom, clientW, m_iconSize);
}

Hit RuleBuilderPanel::HitTest(int x, int y) const {
	const int absY = y + GetScrollPosition();
	return RuleBuilderLayout::HitTest(m_layouts, DropZoneRect(), m_model, x, absY);
}

uint16_t RuleBuilderPanel::SelectedItemId() const {
	const RuleGroup* rule = m_model.GetRule(m_selection.row);
	if (!rule) {
		return 0;
	}
	if (m_selection.sourceIndex >= 0 && m_selection.sourceIndex < static_cast<int>(rule->sources.size())) {
		return rule->sources[m_selection.sourceIndex];
	}
	if (m_selection.targetIndex >= 0 && m_selection.targetIndex < static_cast<int>(rule->targets.size())) {
		return rule->targets[m_selection.targetIndex].id;
	}
	return 0;
}

void RuleBuilderPanel::NotifyRulesChanged() {
	LayoutRows();
	Refresh();
	if (m_listener) {
		m_listener->OnRuleChanged();
	}
}

void RuleBuilderPanel::NotifyCellSelected() {
	Refresh();
	if (m_listener) {
		m_listener->OnRuleItemSelected(SelectedItemId());
	}
}

void RuleBuilderPanel::OpenChanceEditor(int row) {
	const RuleGroup* rule = m_model.GetRule(row);
	if (!rule || rule->targets.empty()) {
		return;
	}

	RuleChanceDialog dialog(this, rule->targets);
	if (dialog.ShowModal() == wxID_OK) {
		m_model.SetCustomTargets(row, dialog.GetTargets());
		NotifyRulesChanged();
	}
}

void RuleBuilderPanel::RemoveSelectedCell() {
	if (m_selection.row < 0 || m_selection.row >= static_cast<int>(m_model.GetRuleCount())) {
		return;
	}

	m_model.RemoveCell(m_selection.row, m_selection.sourceIndex, m_selection.targetIndex);
	m_selection = {};
	NotifyRulesChanged();
	NotifyCellSelected();
}

void RuleBuilderPanel::DuplicateSelectedRow() {
	if (m_selection.row < 0 || m_selection.row >= static_cast<int>(m_model.GetRuleCount())) {
		return;
	}

	m_model.DuplicateRow(m_selection.row);
	++m_selection.row;
	NotifyRulesChanged();
}

void RuleBuilderPanel::ApplyDrop(int x, int y, const std::vector<uint16_t>& itemIds) {
	if (itemIds.empty()) {
		return;
	}

	const Hit hit = HitTest(x, y);
	int outRow = -1;
	int outSource = -1;
	int outTarget = -1;

	const bool isNewRule = hit.type == HitType::NewRule;
	const bool isSource = hit.type == HitType::Source || hit.type == HitType::RemoveSource || hit.type == HitType::AddSource;

	if (m_model.ApplyDrop(hit.row, isSource, hit.index, isNewRule, itemIds, outRow, outSource, outTarget)) {
		m_selection = { outRow, outSource, outTarget };
		NotifyRulesChanged();
		NotifyCellSelected();
	} else if (outRow >= 0) {
		m_selection = { outRow, outSource, outTarget };
		NotifyCellSelected();
	}
}

bool RuleBuilderPanel::ApplyRuleMove(int x, int y, const wxString& payload) {
	const wxArrayString parts = wxSplit(payload, ':');
	if (parts.size() != 3) {
		return false;
	}

	unsigned long rowVal = 0;
	unsigned long idxVal = 0;
	if (!parts[0].ToULong(&rowVal) || !parts[2].ToULong(&idxVal)) {
		return false;
	}

	const int fromRow = static_cast<int>(rowVal);
	const int fromIndex = static_cast<int>(idxVal);
	const bool fromSource = parts[1] == "s";

	const Hit hit = HitTest(x, y);
	const bool toSource = hit.type == HitType::Source || hit.type == HitType::RemoveSource || hit.type == HitType::AddSource;
	const bool toTarget = hit.type == HitType::Target || hit.type == HitType::RemoveTarget || hit.type == HitType::AddTarget;
	if (!toSource && !toTarget) {
		return false;
	}

	int insertedIndex = -1;
	if (m_model.MoveCell(fromRow, fromSource, fromIndex, hit.row, toSource, hit.index, insertedIndex)) {
		m_selection = { hit.row, toSource ? insertedIndex : -1, toSource ? -1 : insertedIndex };
		NotifyRulesChanged();
		NotifyCellSelected();
		return true;
	}
	return false;
}

void RuleBuilderPanel::UpdateDragHover(int x, int y) {
	const Hit hit = HitTest(x, y);
	if (hit.type != m_dragHover.type || hit.row != m_dragHover.row || hit.index != m_dragHover.index) {
		m_dragHover = hit;
		m_externalDrag = true;
		Refresh();
	}
}

void RuleBuilderPanel::ClearDragHover() {
	if (!m_externalDrag && m_dragHover.type == HitType::None) {
		return;
	}
	m_externalDrag = false;
	m_dragHover = {};
	Refresh();
}

void RuleBuilderPanel::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	RenderContext ctx;
	ctx.vg = vg;
	ctx.width = width;
	ctx.height = height;
	ctx.scrollY = GetScrollPosition();
	ctx.iconSize = m_iconSize;
	ctx.showLabels = m_showLabels;
	ctx.labelCache = &m_labelCache;
	ctx.getItemTexture = [this](uint16_t id) { return GetOrCreateItemImage(id); };
	ctx.getCachedTexture = [this](uint64_t key) { return GetCachedImage(key); };
	ctx.createTexture = [this](uint64_t key, const unsigned char* data, int w, int h) {
		return GetOrCreateImage(key, data, w, h);
	};

	RuleBuilderRenderer::Render(
		ctx,
		m_model,
		m_layouts,
		DropZoneRect(),
		m_selection,
		m_hover,
		m_dragHover,
		m_externalDrag
	);
}

void RuleBuilderPanel::OnLeftDown(wxMouseEvent& event) {
	SetFocus();
	const Hit hit = HitTest(event.GetX(), event.GetY());

	m_pressCell = {};
	if (hit.row >= 0 && (hit.type == HitType::Source || hit.type == HitType::Target)) {
		m_pressCell = { hit.row, hit.type == HitType::Source, hit.index };
		m_pressPoint = event.GetPosition();
	}

	switch (hit.type) {
		case HitType::Source:
			m_selection = { hit.row, hit.index, -1 };
			NotifyCellSelected();
			break;

		case HitType::RemoveSource:
			m_selection = { hit.row, hit.index, -1 };
			RemoveSelectedCell();
			break;

		case HitType::Target:
			m_selection = { hit.row, -1, hit.index };
			NotifyCellSelected();
			break;

		case HitType::RemoveTarget:
			m_selection = { hit.row, -1, hit.index };
			RemoveSelectedCell();
			break;

		case HitType::RuleAction:
			m_model.ToggleRuleAction(hit.row);
			NotifyRulesChanged();
			break;

		case HitType::Percent:
			OpenChanceEditor(hit.row);
			break;

		case HitType::AddSource:
			if (hit.row >= 0) {
				m_selection = { hit.row, -1, -1 };
				NotifyCellSelected();
			}
			break;

		case HitType::AddTarget:
		case HitType::TrashRule:
			m_model.ToggleRuleRemoval(hit.row);
			m_selection = {};
			NotifyRulesChanged();
			break;

		case HitType::DeleteRow:
			m_model.DeleteRow(hit.row);
			m_selection = {};
			NotifyRulesChanged();
			break;

		case HitType::NewRule:
		case HitType::None:
		default:
			break;
	}

	Refresh();
	event.Skip();
}

void RuleBuilderPanel::OnLeftUp(wxMouseEvent& event) {
	m_pressCell = {};
	m_internalDrag = false;
	event.Skip();
}

void RuleBuilderPanel::OnLeftDoubleClick(wxMouseEvent& event) {
	event.Skip();
}

void RuleBuilderPanel::OnMotion(wxMouseEvent& event) {
	if (m_pressCell.row >= 0 && !m_internalDrag && event.Dragging()) {
		if (std::abs(event.GetX() - m_pressPoint.x) > 6 || std::abs(event.GetY() - m_pressPoint.y) > 6) {
			m_internalDrag = true;
			const wxString payload = wxString::Format("%s%d:%c:%d", kRuleCellPrefix, m_pressCell.row, m_pressCell.source ? 's' : 't', m_pressCell.index);
			wxTextDataObject data(payload);
			wxDropSource dragSource(this);
			dragSource.SetData(data);
			dragSource.DoDragDrop(wxDrag_AllowMove);
			m_internalDrag = false;
			m_pressCell = {};
			return;
		}
	}

	const Hit hit = HitTest(event.GetX(), event.GetY());
	if (hit.type != m_hover.type || hit.row != m_hover.row || hit.index != m_hover.index) {
		m_hover = hit;
		Refresh();

		const RuleGroup* rule = m_model.GetRule(hit.row);
		if (rule && hit.type == HitType::Source && hit.index >= 0 && hit.index < static_cast<int>(rule->sources.size())) {
			SetToolTip(wxstr(ItemName(rule->sources[hit.index])));
		} else if (rule && hit.type == HitType::Target && hit.index >= 0 && hit.index < static_cast<int>(rule->targets.size())) {
			SetToolTip(wxstr(ItemName(rule->targets[hit.index].id)));
		} else if (rule && (hit.type == HitType::RemoveTarget || hit.type == HitType::RemoveSource)) {
			SetToolTip("Remove this item from the rule");
		} else if (rule && hit.type == HitType::Percent && hit.index >= 0 && hit.index < static_cast<int>(rule->targets.size())) {
			SetToolTip(wxstr(std::format("Chance {} - click to edit", FormatChance(rule->targets[hit.index].probability))));
		} else if (rule && hit.type == HitType::RuleAction) {
			SetToolTip(rule->remove
					? "This rule deletes the items - click for REPLACE"
					: (rule->action == RuleAction::Add ? "This rule adds the items - click for REMOVE" : "This rule replaces the items - click for ADD"));
		} else if (rule && (hit.type == HitType::TrashRule || hit.type == HitType::AddTarget)) {
			SetToolTip(rule->remove ? "Stop deleting the found items" : "Delete the found items instead of replacing them");
		} else {
			UnsetToolTip();
		}
	}

	switch (hit.type) {
		case HitType::Source:
		case HitType::RemoveSource:
		case HitType::Target:
		case HitType::RemoveTarget:
		case HitType::Percent:
		case HitType::RuleAction:
		case HitType::AddSource:
		case HitType::AddTarget:
		case HitType::DeleteRow:
		case HitType::TrashRule:
			SetCursor(wxCursor(wxCURSOR_HAND));
			break;
		default:
			SetCursor(wxNullCursor);
			break;
	}
	event.Skip();
}

void RuleBuilderPanel::OnLeave(wxMouseEvent& event) {
	m_hover = {};
	UnsetToolTip();
	Refresh();
	event.Skip();
}

void RuleBuilderPanel::OnContextMenu(wxContextMenuEvent& event) {
	const wxPoint client = ScreenToClient(event.GetPosition());
	const Hit hit = HitTest(client.x, client.y);

	wxMenu menu;
	if (hit.type == HitType::RuleAction) {
		const RuleGroup* rule = m_model.GetRule(hit.row);
		if (!rule) {
			return;
		}
		menu.AppendRadioItem(ID_ACTION_REPLACE, "Replace the item")->Check(!rule->remove && rule->action == RuleAction::Replace);
		menu.AppendRadioItem(ID_ACTION_ADD, "Add the item as well")->Check(!rule->remove && rule->action == RuleAction::Add);
		menu.AppendRadioItem(ID_ACTION_REMOVE, "Delete the found items")->Check(rule->remove);
	} else if (hit.type == HitType::Target || hit.type == HitType::RemoveTarget || hit.type == HitType::Percent) {
		if (hit.row < 0 || hit.row >= static_cast<int>(m_model.GetRuleCount())) {
			return;
		}
		menu.Append(ID_CHANCE, "Chance...");
		menu.AppendSeparator();
		menu.Append(ID_REMOVE_CELL, "Remove target");
	} else if (hit.type == HitType::Source) {
		menu.Append(ID_REMOVE_CELL, "Remove source");
	} else if (hit.type == HitType::TrashRule) {
		const RuleGroup* rule = m_model.GetRule(hit.row);
		menu.Append(ID_REMOVE_ROW, rule && rule->remove ? "Stop deleting and keep targets" : "Delete the found items");
	} else if (hit.type == HitType::None && hit.row >= 0) {
		menu.Append(ID_DUPLICATE_ROW, "Duplicate rule");
		menu.AppendSeparator();
		menu.Append(ID_REMOVE_ROW, "Remove rule");
	} else {
		return;
	}

	const int selected = GetPopupMenuSelectionFromUser(menu);
	switch (selected) {
		case ID_ACTION_REPLACE:
			if (RuleGroup* rule = m_model.GetRuleMut(hit.row)) {
				m_model.PushUndo();
				rule->remove = false;
				rule->action = RuleAction::Replace;
			}
			break;
		case ID_ACTION_ADD:
			if (RuleGroup* rule = m_model.GetRuleMut(hit.row)) {
				m_model.PushUndo();
				rule->remove = false;
				rule->action = RuleAction::Add;
			}
			break;
		case ID_ACTION_REMOVE:
			if (RuleGroup* rule = m_model.GetRuleMut(hit.row)) {
				m_model.PushUndo();
				rule->remove = true;
			}
			break;
		case ID_CHANCE:
			OpenChanceEditor(hit.row);
			return;
		case ID_REMOVE_CELL:
			m_selection = hit.type == HitType::Source ? SelectionState { hit.row, hit.index, -1 } : SelectionState { hit.row, -1, hit.index };
			RemoveSelectedCell();
			return;
		case ID_DUPLICATE_ROW:
			m_selection = { hit.row, -1, -1 };
			DuplicateSelectedRow();
			return;
		case ID_REMOVE_ROW:
			if (hit.type == HitType::TrashRule) {
				m_model.ToggleRuleRemoval(hit.row);
			} else {
				m_model.DeleteRow(hit.row);
				m_selection = {};
			}
			NotifyRulesChanged();
			return;
		default:
			return;
	}

	NotifyRulesChanged();
	NotifyCellSelected();
}

void RuleBuilderPanel::OnKeyDown(wxKeyEvent& event) {
	if (event.GetKeyCode() == WXK_DELETE) {
		RemoveSelectedCell();
		return;
	}
	if (event.GetKeyCode() == 'D' && event.ControlDown()) {
		DuplicateSelectedRow();
		return;
	}
	event.Skip();
}

void RuleBuilderPanel::OnSize(wxSizeEvent& event) {
	LayoutRows();
	Refresh();
	event.Skip();
}
