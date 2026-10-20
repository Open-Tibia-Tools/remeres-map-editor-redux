#include "ui/replace_tool/rule_list_control.h"
#include "ui/theme.h"
#include "util/image_manager.h"
#include <nanovg.h>
#include <memory>
#include <algorithm>

namespace {

	/** Mixes a tint into a base colour; wxColour has no blend in this version. */
	wxColour MixColour(const wxColour& base, const wxColour& tint, int weightPercent) {
		const double weight = weightPercent / 100.0;
		const auto mix = [weight](unsigned char from, unsigned char to) {
			return static_cast<unsigned char>(from * (1.0 - weight) + to * weight);
		};
		return wxColour(mix(base.Red(), tint.Red()), mix(base.Green(), tint.Green()), mix(base.Blue(), tint.Blue()));
	}

} // namespace

RuleListControl::RuleListControl(wxWindow* parent, Listener* listener) : NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxNO_BORDER | wxWANTS_CHARS),
																		 m_listener(listener) {
	// NanoVGCanvas handles background style
	m_itemHeight = FromDIP(56);
	// The base class owns scrolling; this control used to keep a second scroll
	// position, which made a click load a different item than the one drawn.
	SetScrollStep(m_itemHeight * 3);

	Bind(wxEVT_SIZE, &RuleListControl::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &RuleListControl::OnMouse, this);
	Bind(wxEVT_MOTION, &RuleListControl::OnMouse, this);
	Bind(wxEVT_LEAVE_WINDOW, &RuleListControl::OnMouse, this);
	Bind(wxEVT_CONTEXT_MENU, &RuleListControl::OnContextMenu, this);
}

void RuleListControl::SetRuleSets(const std::vector<std::string>& ruleSetNames) {
	m_ruleSetNames = ruleSetNames;
	m_selectedIndex = -1;

	// The active set is tracked by name, so it survives a refresh and reordering.
	if (!m_activeRuleSet.empty() && std::ranges::find(m_ruleSetNames, m_activeRuleSet) == m_ruleSetNames.end()) {
		m_activeRuleSet.clear();
	}

	RefreshVirtualSize();
	Refresh();
}

void RuleListControl::SetActiveRuleSet(const std::string& name, bool dirty, bool saved) {
	if (m_activeRuleSet == name && m_activeDirty == dirty && m_activeSaved == saved) {
		return;
	}
	m_activeRuleSet = name;
	m_activeDirty = dirty;
	m_activeSaved = saved;
	Refresh();
}
void RuleListControl::RefreshVirtualSize() {
	UpdateScrollbar(static_cast<int>(m_ruleSetNames.size()) * m_itemHeight);
}

wxSize RuleListControl::DoGetBestClientSize() const {
	return wxSize(FromDIP(150), FromDIP(200));
}

void RuleListControl::OnSize(wxSizeEvent& event) {
	RefreshVirtualSize();
	Refresh();
	event.Skip();
}
void RuleListControl::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	const int scrollPos = GetScrollPosition();
	// Assumption: NanoVGCanvas applies nvgTranslate(0, -scrollPos) automatically.
	// So we draw items at their absolute Y position.

	wxSize clientSize = GetClientSize();

	int startIdx = scrollPos / m_itemHeight;
	int endIdx = std::min((int)m_ruleSetNames.size() - 1, (scrollPos + clientSize.y) / m_itemHeight + 1);

	static const float padding = 4.0f;
	static const float radius = 4.0f;
	static const float FONT_SIZE = 16.0f; // 10pt equivalent, adjusted for better visual weight

	const wxColour cardBase = Theme::Get(Theme::Role::CardBase);
	const wxColour cardBaseHover = Theme::Get(Theme::Role::CardBaseHover);
	const wxColour borderNormal = Theme::Get(Theme::Role::CardBorder);
	const wxColour textNormal = Theme::Get(Theme::Role::Text);

	nvgFontSize(vg, FONT_SIZE);
	if (nvgFindFont(vg, "sans") != -1) {
		nvgFontFace(vg, "sans");
	}
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

	// Rule sets are colour coded by state:
	//   selected        blue    - the set whose rules are in the table
	//   selected, edited orange - loaded, but the table has unsaved changes
	//   saved           green   - loaded and written to disk
	for (int i = startIdx; i <= endIdx; ++i) {
		float y = i * m_itemHeight + padding;
		float x = padding;
		float w = width - 2 * padding;
		float h = m_itemHeight - 2 * padding;

		const bool isSelected = (i == m_selectedIndex);
		const bool isHovered = (i == m_hoveredIndex);

		// Only the set being edited is colour coded, and its colour is the state
		// of its edits:
		//   blue   - loaded, unchanged since
		//   orange - edited, not written to disk
		//   green  - written to disk
		// Everything else is a set sitting on disk and stays neutral, otherwise
		// the list would be one colour forever and say nothing.
		const bool isActive = (!m_activeRuleSet.empty() && m_ruleSetNames[i] == m_activeRuleSet);

		wxColour stateColour = Theme::Get(Theme::Role::Text);
		if (isActive) {
			stateColour = Theme::Get(m_activeDirty ? Theme::Role::Warning : (m_activeSaved ? Theme::Role::Success : Theme::Role::Accent));
		}

		wxColour fillColor = cardBase;
		if (isActive) {
			fillColor = MixColour(cardBase, stateColour, 40);
		} else if (isSelected || isHovered) {
			fillColor = cardBaseHover;
		}

		// Draw Shadow (Simple offset)
		if (!isSelected) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 2, y + 2, w, h, radius);
			nvgFillColor(vg, nvgRGBA(0, 0, 0, 50));
			nvgFill(vg);
		}

		// Draw Card Body
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, radius);
		nvgFillColor(vg, nvgRGBA(fillColor.Red(), fillColor.Green(), fillColor.Blue(), fillColor.Alpha()));
		nvgFill(vg);

		// Border
		wxColour borderColor = isActive ? stateColour : borderNormal;
		nvgStrokeColor(vg, nvgRGBA(borderColor.Red(), borderColor.Green(), borderColor.Blue(), borderColor.Alpha()));
		nvgStrokeWidth(vg, isActive ? 2.0f : 1.0f);
		nvgStroke(vg);

		// Text
		wxColour textColor = isActive ? stateColour : textNormal;
		nvgFillColor(vg, nvgRGBA(textColor.Red(), textColor.Green(), textColor.Blue(), textColor.Alpha()));

		nvgText(vg, x + 14, y + h / 2.0f, m_ruleSetNames[i].c_str(), nullptr);

		// A solid state bar on the left, on the set being edited.
		if (isActive) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, y, 5.0f, h, 2.0f);
			nvgFillColor(vg, nvgRGBA(stateColour.Red(), stateColour.Green(), stateColour.Blue(), 255));
			nvgFill(vg);
		}
	}
}

wxRect RuleListControl::GetItemRect(int index) const {
	if (index < 0 || index >= (int)m_ruleSetNames.size()) {
		return wxRect();
	}
	wxSize clientSize = GetClientSize();
	const int scrollPos = GetScrollPosition();
	return wxRect(0, index * m_itemHeight - scrollPos, clientSize.x, m_itemHeight);
}

void RuleListControl::OnMouse(wxMouseEvent& event) {
	int oldHover = m_hoveredIndex;

	if (event.GetEventType() == wxEVT_LEAVE_WINDOW) {
		m_hoveredIndex = -1;
	} else {
		const int scrollPos = GetScrollPosition();
		int y = event.GetY() + scrollPos;
		int idx = y / m_itemHeight;

		if (idx >= 0 && idx < (int)m_ruleSetNames.size()) {
			m_hoveredIndex = idx;
		} else {
			m_hoveredIndex = -1;
		}

		if (event.LeftDown() && m_hoveredIndex != -1) {
			m_selectedIndex = m_hoveredIndex;
			if (m_listener) {
				m_listener->OnRuleSelected(RuleManager::Get().LoadRuleSet(m_ruleSetNames[m_selectedIndex]));
			}
		}
	}

	if (oldHover != m_hoveredIndex || event.LeftDown()) {
		Refresh();
	}
}

void RuleListControl::OnContextMenu(wxContextMenuEvent& event) {
	int menuIdx = m_hoveredIndex;
	if (menuIdx == -1) {
		return;
	}

	wxMenu menu;
	menu.Append(wxID_EDIT, "Edit Name")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN_TO_SQUARE, wxSize(16, 16)));
	menu.Append(wxID_DELETE, "Delete")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, wxSize(16, 16)));

	menu.Bind(wxEVT_MENU, [this, menuIdx](wxCommandEvent& e) {
		if (menuIdx < 0 || menuIdx >= (int)m_ruleSetNames.size()) {
			return;
		}

		std::string name = m_ruleSetNames[menuIdx];

		if (e.GetId() == wxID_EDIT) {
			wxString newName = wxGetTextFromUser("Enter new name for rule set:", "Edit Name", name);
			if (!newName.IsEmpty() && newName.ToStdString() != name) {
				if (m_listener) {
					m_listener->OnRuleRenamed(name, newName.ToStdString());
				}
			}
		} else if (e.GetId() == wxID_DELETE) {
			wxMessageDialog dlg(this, "Are you sure you want to delete rule set '" + name + "'?", "Delete Rule Set", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
			if (dlg.ShowModal() == wxID_YES) {
				if (m_listener) {
					m_listener->OnRuleDeleted(name);
				}
			}
		}
	});

	PopupMenu(&menu);
}
