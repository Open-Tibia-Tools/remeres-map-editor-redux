#ifndef RME_RULE_LIST_CONTROL_H_
#define RME_RULE_LIST_CONTROL_H_

#include "ui/replace_tool/rule_manager.h"
#include "util/nanovg_canvas.h"
#include <vector>
#include <string>

class RuleListControl : public NanoVGCanvas {
public:
	class Listener {
	public:
		virtual ~Listener() { }
		virtual void OnRuleSelected(const RuleSet& ruleSet) = 0;
		virtual void OnRuleDeleted(const std::string& name) = 0;
		virtual void OnRuleRenamed(const std::string& oldName, const std::string& newName) = 0;
	};

	RuleListControl(wxWindow* parent, Listener* listener);

	void SetRuleSets(const std::vector<std::string>& ruleSetNames);
	/**
	 * Marks the set whose rules are in the table. Its colour follows the
	 * workflow: blue while it is freshly loaded, orange once edited, green
	 * after it was written to disk.
	 */
	void SetActiveRuleSet(const std::string& name, bool dirty, bool saved);
	wxSize DoGetBestClientSize() const override;

	int GetSelectedIndex() const {
		return m_selectedIndex;
	}
	const std::vector<std::string>& GetRuleSetNames() const {
		return m_ruleSetNames;
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	void OnMouse(wxMouseEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnSize(wxSizeEvent& event);

	void RefreshVirtualSize();
	wxRect GetItemRect(int index) const;

	std::vector<std::string> m_ruleSetNames;
	std::string m_activeRuleSet;
	bool m_activeDirty = false;
	bool m_activeSaved = false;
	int m_selectedIndex = -1;
	int m_hoveredIndex = -1;
	int m_itemHeight;
	Listener* m_listener;
};

#endif
