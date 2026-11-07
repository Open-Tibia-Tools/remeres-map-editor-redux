#ifndef RME_RULE_MODEL_H_
#define RME_RULE_MODEL_H_

#include "ui/replace_tool/rule_manager.h"
#include <cstdint>
#include <vector>
#include <functional>

namespace rme::replace_tool {

/**
 * Manages in-memory Replacement rules, state mutations, chance budget calculations,
 * and Undo/Redo history. Decoupled from all UI rendering and window controls.
 */
class RuleModel {
public:
	RuleModel() = default;

	void Clear();
	void SetRules(const std::vector<RuleGroup>& rules);
	const std::vector<RuleGroup>& GetRules() const {
		return m_rules;
	}
	size_t GetRuleCount() const {
		return m_rules.size();
	}
	const RuleGroup* GetRule(size_t index) const {
		return index < m_rules.size() ? &m_rules[index] : nullptr;
	}
	RuleGroup* GetRuleMut(size_t index) {
		return index < m_rules.size() ? &m_rules[index] : nullptr;
	}

	void AddRuleWithSource(uint16_t itemId);
	bool AddRuleIfNewSource(uint16_t itemId);
	size_t AddRulesWithSources(const std::vector<uint16_t>& itemIds);

	bool CanUndo() const {
		return !m_undoStack.empty();
	}
	bool CanRedo() const {
		return !m_redoStack.empty();
	}
	bool Undo();
	bool Redo();
	void PushUndo();

	void DistributeEvenly(size_t row);
	void DistributeForNewTarget(size_t row);
	void PlaceNewTargets(size_t row, size_t newCount);

	void ToggleRuleAction(size_t row);
	void ToggleRuleRemoval(size_t row);

	void RemoveCell(size_t row, int sourceIndex, int targetIndex);
	void DuplicateRow(size_t row);
	void DeleteRow(size_t row);
	void SetCustomTargets(size_t row, std::vector<ReplacementTarget> targets);

	int CountSource(size_t row, uint16_t itemId) const;
	int FindSourceRow(uint16_t itemId) const;

	bool MoveCell(int fromRow, bool fromSource, int fromIndex, int toRow, bool toSource, int toIndex, int& outInsertedIndex);

	bool ApplyDrop(int targetRow, bool isSource, int slotIndex, bool isNewRule, const std::vector<uint16_t>& itemIds, int& outSelectedRow, int& outSelectedSource, int& outSelectedTarget);

	bool IsRowTargetless(size_t row) const;
	bool IsRowRemoved(size_t row) const;

private:
	void RestoreRules(std::vector<RuleGroup>&& rules);

	std::vector<RuleGroup> m_rules;
	std::vector<std::vector<RuleGroup>> m_undoStack;
	std::vector<std::vector<RuleGroup>> m_redoStack;
};

} // namespace rme::replace_tool

#endif
