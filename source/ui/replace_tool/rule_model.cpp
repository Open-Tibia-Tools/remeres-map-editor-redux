#include "app/main.h"
#include "ui/replace_tool/rule_model.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace rme::replace_tool {

void RuleModel::Clear() {
	PushUndo();
	m_rules.clear();
}

void RuleModel::SetRules(const std::vector<RuleGroup>& rules) {
	m_rules = rules;
	m_undoStack.clear();
	m_redoStack.clear();
}

void RuleModel::AddRuleWithSource(uint16_t itemId) {
	PushUndo();
	RuleGroup rule;
	if (itemId != 0) {
		rule.sources.push_back(itemId);
	}
	m_rules.push_back(std::move(rule));
}

bool RuleModel::AddRuleIfNewSource(uint16_t itemId) {
	if (itemId == 0) {
		return false;
	}
	for (const RuleGroup& rule : m_rules) {
		if (std::ranges::find(rule.sources, itemId) != rule.sources.end()) {
			return false;
		}
	}
	AddRuleWithSource(itemId);
	return true;
}

size_t RuleModel::AddRulesWithSources(const std::vector<uint16_t>& itemIds) {
	if (itemIds.empty()) {
		return 0;
	}

	size_t addedCount = 0;
	for (uint16_t id : itemIds) {
		if (id == 0) {
			continue;
		}
		const bool known = std::ranges::any_of(m_rules, [id](const RuleGroup& rule) {
			return std::ranges::find(rule.sources, id) != rule.sources.end();
		});
		if (known) {
			continue;
		}

		if (addedCount == 0) {
			PushUndo();
		}
		RuleGroup group;
		group.sources.push_back(id);
		m_rules.push_back(std::move(group));
		++addedCount;
	}

	return addedCount;
}

void RuleModel::PushUndo() {
	if (!m_undoStack.empty() && m_undoStack.back() == m_rules) {
		return;
	}

	constexpr size_t kMaxUndoDepth = 64;
	m_undoStack.push_back(m_rules);
	if (m_undoStack.size() > kMaxUndoDepth) {
		m_undoStack.erase(m_undoStack.begin());
	}
	m_redoStack.clear();
}

void RuleModel::RestoreRules(std::vector<RuleGroup>&& rules) {
	m_rules = std::move(rules);
}

bool RuleModel::Undo() {
	while (!m_undoStack.empty() && m_undoStack.back() == m_rules) {
		m_undoStack.pop_back();
	}
	if (m_undoStack.empty()) {
		return false;
	}

	m_redoStack.push_back(m_rules);
	std::vector<RuleGroup> previous = std::move(m_undoStack.back());
	m_undoStack.pop_back();
	RestoreRules(std::move(previous));
	return true;
}

bool RuleModel::Redo() {
	while (!m_redoStack.empty() && m_redoStack.back() == m_rules) {
		m_redoStack.pop_back();
	}
	if (m_redoStack.empty()) {
		return false;
	}

	m_undoStack.push_back(m_rules);
	std::vector<RuleGroup> next = std::move(m_redoStack.back());
	m_redoStack.pop_back();
	RestoreRules(std::move(next));
	return true;
}

void RuleModel::DistributeEvenly(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}
	auto& targets = m_rules[row].targets;
	if (targets.empty()) {
		return;
	}

	const double step = static_cast<double>(kChanceMax) / static_cast<double>(targets.size());
	double accumulated = 0.0;
	for (auto& target : targets) {
		const double next = accumulated + step;
		target.probability = static_cast<int>(std::lround(next)) - static_cast<int>(std::lround(accumulated));
		accumulated = next;
	}
}

void RuleModel::DistributeForNewTarget(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}
	auto& targets = m_rules[row].targets;
	if (targets.empty()) {
		return;
	}

	if (targets.size() == 1) {
		targets.front().probability = kChanceMax;
		return;
	}

	int existing = 0;
	for (size_t i = 0; i + 1 < targets.size(); ++i) {
		existing += targets[i].probability;
	}

	if (existing <= 0) {
		targets.back().probability = kChanceMax;
		return;
	}
	if (const int remainder = kChanceMax - existing; remainder > 0) {
		targets.back().probability = remainder;
		return;
	}

	DistributeEvenly(row);
}

void RuleModel::PlaceNewTargets(size_t row, size_t newCount) {
	if (row >= m_rules.size() || newCount == 0) {
		return;
	}
	auto& targets = m_rules[row].targets;
	if (targets.empty()) {
		return;
	}

	if (targets.size() == newCount) {
		DistributeEvenly(row);
		return;
	}

	int existing = 0;
	for (size_t i = 0; i + newCount < targets.size(); ++i) {
		existing += targets[i].probability;
	}

	const int free = kChanceMax - existing;
	if (free <= 0) {
		DistributeEvenly(row);
		return;
	}

	const int share = std::max(1, free / static_cast<int>(newCount));
	for (size_t i = targets.size() - newCount; i < targets.size(); ++i) {
		targets[i].probability = share;
	}
}

void RuleModel::ToggleRuleAction(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}

	PushUndo();
	RuleGroup& rule = m_rules[row];
	if (rule.remove) {
		rule.remove = false;
		rule.action = RuleAction::Replace;
	} else if (rule.action == RuleAction::Replace) {
		rule.action = RuleAction::Add;
	} else {
		rule.remove = true;
	}
}

void RuleModel::ToggleRuleRemoval(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}

	PushUndo();
	m_rules[row].remove = !m_rules[row].remove;
}

void RuleModel::RemoveCell(size_t row, int sourceIndex, int targetIndex) {
	if (row >= m_rules.size()) {
		return;
	}

	PushUndo();
	RuleGroup& rule = m_rules[row];
	if (sourceIndex >= 0 && sourceIndex < static_cast<int>(rule.sources.size())) {
		rule.sources.erase(rule.sources.begin() + sourceIndex);
	} else if (targetIndex >= 0 && targetIndex < static_cast<int>(rule.targets.size())) {
		rule.targets.erase(rule.targets.begin() + targetIndex);
	} else {
		return;
	}

	if (rule.sources.empty() && rule.targets.empty()) {
		m_rules.erase(m_rules.begin() + row);
	}
}

void RuleModel::DuplicateRow(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}
	PushUndo();
	m_rules.insert(m_rules.begin() + row + 1, m_rules[row]);
}

void RuleModel::DeleteRow(size_t row) {
	if (row >= m_rules.size()) {
		return;
	}
	PushUndo();
	m_rules.erase(m_rules.begin() + row);
}

void RuleModel::SetCustomTargets(size_t row, std::vector<ReplacementTarget> targets) {
	if (row >= m_rules.size()) {
		return;
	}
	PushUndo();
	m_rules[row].targets = std::move(targets);
}

int RuleModel::CountSource(size_t row, uint16_t itemId) const {
	if (row >= m_rules.size()) {
		return 0;
	}
	return static_cast<int>(std::ranges::count(m_rules[row].sources, itemId));
}

int RuleModel::FindSourceRow(uint16_t itemId) const {
	for (size_t row = 0; row < m_rules.size(); ++row) {
		if (std::ranges::find(m_rules[row].sources, itemId) != m_rules[row].sources.end()) {
			return static_cast<int>(row);
		}
	}
	return -1;
}

bool RuleModel::MoveCell(int fromRow, bool fromSource, int fromIndex, int toRow, bool toSource, int toIndex, int& outInsertedIndex) {
	outInsertedIndex = -1;
	if (fromRow < 0 || fromRow >= static_cast<int>(m_rules.size()) ||
	    toRow < 0 || toRow >= static_cast<int>(m_rules.size())) {
		return false;
	}
	if (fromSource != toSource) {
		return false;
	}
	if (m_rules[toRow].remove) {
		return false;
	}

	PushUndo();
	if (fromSource) {
		if (fromIndex < 0 || fromIndex >= static_cast<int>(m_rules[fromRow].sources.size())) {
			return false;
		}
		const uint16_t id = m_rules[fromRow].sources[fromIndex];
		if (fromRow != toRow && CountSource(toRow, id) > 0) {
			return false;
		}

		m_rules[fromRow].sources.erase(m_rules[fromRow].sources.begin() + fromIndex);
		size_t insertAt = m_rules[toRow].sources.size();
		if (toIndex >= 0) {
			insertAt = static_cast<size_t>(toIndex);
			if (fromRow == toRow && fromIndex < toIndex) {
				--insertAt;
			}
		}
		insertAt = std::min(insertAt, m_rules[toRow].sources.size());
		m_rules[toRow].sources.insert(m_rules[toRow].sources.begin() + insertAt, id);
		outInsertedIndex = static_cast<int>(insertAt);
	} else {
		if (fromIndex < 0 || fromIndex >= static_cast<int>(m_rules[fromRow].targets.size())) {
			return false;
		}
		ReplacementTarget target = m_rules[fromRow].targets[fromIndex];
		m_rules[fromRow].targets.erase(m_rules[fromRow].targets.begin() + fromIndex);

		size_t insertAt = m_rules[toRow].targets.size();
		if (toIndex >= 0) {
			insertAt = static_cast<size_t>(toIndex);
			if (fromRow == toRow && fromIndex < toIndex) {
				--insertAt;
			}
		}
		insertAt = std::min(insertAt, m_rules[toRow].targets.size());

		int others = 0;
		for (const ReplacementTarget& other : m_rules[toRow].targets) {
			others += other.probability;
		}
		target.probability = std::clamp(target.probability, 0, std::max(0, kChanceMax - others));

		m_rules[toRow].targets.insert(m_rules[toRow].targets.begin() + insertAt, target);
		outInsertedIndex = static_cast<int>(insertAt);
	}
	return true;
}

bool RuleModel::ApplyDrop(int targetRow, bool isSource, int slotIndex, bool isNewRule, const std::vector<uint16_t>& itemIds, int& outSelectedRow, int& outSelectedSource, int& outSelectedTarget) {
	outSelectedRow = -1;
	outSelectedSource = -1;
	outSelectedTarget = -1;

	if (itemIds.empty()) {
		return false;
	}

	if (isNewRule) {
		const int existing = FindSourceRow(itemIds.front());
		if (existing >= 0) {
			outSelectedRow = existing;
			outSelectedSource = 0;
			return false; // not an error, but didn't create new rule
		}

		PushUndo();
		for (const uint16_t id : itemIds) {
			if (FindSourceRow(id) < 0) {
				RuleGroup rule;
				rule.sources.push_back(id);
				m_rules.push_back(std::move(rule));
			}
		}
		outSelectedRow = static_cast<int>(m_rules.size()) - 1;
		outSelectedSource = 0;
		return true;
	}

	if (targetRow < 0 || targetRow >= static_cast<int>(m_rules.size())) {
		return false;
	}

	RuleGroup& rule = m_rules[targetRow];
	if (rule.remove) {
		return false;
	}

	PushUndo();
	if (isSource) {
		size_t target = slotIndex >= 0 && slotIndex < static_cast<int>(rule.sources.size())
			? static_cast<size_t>(slotIndex)
			: rule.sources.size();

		for (const uint16_t id : itemIds) {
			if (CountSource(targetRow, id) > 0 || FindSourceRow(id) >= 0) {
				continue;
			}
			if (target <= rule.sources.size()) {
				rule.sources.insert(rule.sources.begin() + target, id);
				++target;
			} else {
				rule.sources.push_back(id);
			}
		}
		if (!rule.sources.empty()) {
			outSelectedRow = targetRow;
			outSelectedSource = static_cast<int>(std::min(target, rule.sources.size())) - 1;
		}
		return true;
	} else {
		const bool replaceSlot = slotIndex >= 0 && slotIndex < static_cast<int>(rule.targets.size());
		size_t index = rule.targets.size();
		size_t added = 0;
		size_t appended = 0;

		for (const uint16_t id : itemIds) {
			if (replaceSlot && added == 0) {
				rule.targets[slotIndex].id = id;
				index = static_cast<size_t>(slotIndex);
			} else {
				rule.targets.push_back({ id, 0 });
				index = rule.targets.size() - 1;
				++appended;
			}
			++added;
		}

		if (added > 0) {
			if (appended > 0) {
				PlaceNewTargets(targetRow, appended);
			}
			outSelectedRow = targetRow;
			outSelectedTarget = static_cast<int>(index);
		}
		return true;
	}
}

bool RuleModel::IsRowTargetless(size_t row) const {
	if (row >= m_rules.size()) {
		return false;
	}
	const RuleGroup& rule = m_rules[row];
	return !rule.remove && rule.targets.empty();
}

bool RuleModel::IsRowRemoved(size_t row) const {
	if (row >= m_rules.size()) {
		return false;
	}
	return m_rules[row].remove;
}

} // namespace rme::replace_tool
