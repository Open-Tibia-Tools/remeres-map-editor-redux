#ifndef RME_RULE_BUILDER_PANEL_H_
#define RME_RULE_BUILDER_PANEL_H_

#include "app/main.h"
#include "util/nanovg_canvas.h"
#include "util/grid_cell_renderer.h"
#include "ui/replace_tool/rule_model.h"
#include "ui/replace_tool/rule_builder_layout.h"
#include "ui/replace_tool/rule_builder_renderer.h"

#include <wx/wx.h>
#include <wx/dnd.h>
#include <vector>

class Brush;

/**
 * Controller canvas for the replacement rules table.
 * Coordinates RuleModel, RuleBuilderLayout, and RuleBuilderRenderer.
 */
class RuleBuilderPanel : public NanoVGCanvas {
public:
	class Listener {
	public:
		virtual ~Listener() = default;
		virtual void OnRuleChanged() = 0;
		virtual void OnClearRules() = 0;
		virtual void OnSaveRule() = 0;
		virtual void OnRuleItemSelected(uint16_t itemId) = 0;
	};

	RuleBuilderPanel(wxWindow* parent, Listener* listener);
	~RuleBuilderPanel() override;

	class ItemDropTarget : public wxTextDropTarget {
	public:
		explicit ItemDropTarget(RuleBuilderPanel* panel);

		bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;
		wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def) override;
		void OnLeave() override;

	private:
		RuleBuilderPanel* m_panel;
	};

	void Clear();
	void SetRules(const std::vector<RuleGroup>& rules);
	std::vector<RuleGroup> GetRules() const;

	void AddRuleWithSource(uint16_t itemId);
	bool AddRuleIfNewSource(uint16_t itemId);
	void AddRulesWithSources(const std::vector<uint16_t>& itemIds);
	size_t AutoMatchUnmatchedRules(Brush* targetBrush);

	void SetShowLabels(bool show);
	bool GetShowLabels() const {
		return m_showLabels;
	}
	void SetIconSize(int sizePx);
	int GetIconSize() const {
		return m_iconSize;
	}

	bool CanUndo() const {
		return m_model.CanUndo();
	}
	bool CanRedo() const {
		return m_model.CanRedo();
	}
	void Undo();
	void Redo();

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	wxSize DoGetBestClientSize() const override;

private:
	friend class ItemDropTarget;

	void LayoutRows();
	wxRect DropZoneRect() const;
	rme::replace_tool::Hit HitTest(int x, int y) const;
	uint16_t SelectedItemId() const;

	void NotifyRulesChanged();
	void NotifyCellSelected();

	void ApplyDrop(int x, int y, const std::vector<uint16_t>& itemIds);
	bool ApplyRuleMove(int x, int y, const wxString& payload);
	void UpdateDragHover(int x, int y);
	void ClearDragHover();

	void OpenChanceEditor(int row);
	void RemoveSelectedCell();
	void DuplicateSelectedRow();

	// Event Handlers
	void OnLeftDown(wxMouseEvent& event);
	void OnLeftUp(wxMouseEvent& event);
	void OnLeftDoubleClick(wxMouseEvent& event);
	void OnMotion(wxMouseEvent& event);
	void OnLeave(wxMouseEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void OnSize(wxSizeEvent& event);

	rme::replace_tool::RuleModel m_model;
	std::vector<rme::replace_tool::RowLayout> m_layouts;
	Listener* m_listener = nullptr;

	rme::replace_tool::SelectionState m_selection;
	rme::replace_tool::Hit m_hover;
	rme::replace_tool::Hit m_dragHover;
	bool m_externalDrag = false;

	struct CellRef {
		int row = -1;
		bool source = false;
		int index = -1;
	};
	CellRef m_pressCell;
	wxPoint m_pressPoint;
	bool m_internalDrag = false;

	bool m_showLabels = false;
	int m_iconSize = rme::ui::grid::kIconSizeBase;
	rme::ui::grid::LabelCache m_labelCache;
	int m_rowsBottom = 0;
	int m_totalHeight = 0;
};

#endif
