#ifndef RME_RULE_MANAGER_H_
#define RME_RULE_MANAGER_H_

#include <cstdint>
#include <format>
#include <string>
#include <vector>

static const uint16_t TRASH_ITEM_ID = 0xFFFF;

/**
 * Chances are whole percents. An even split of three targets is 33/33/34: the
 * remainder goes to the first targets so the total is always exact.
 */
inline constexpr int kChanceMax = 100;
/** Unit chances are stored in; rule sets record it so older files convert. */
inline constexpr int kChanceScale = 1;

/** A chance rendered for the UI. */
inline std::string FormatChance(int probability) {
	return std::format("{}%", probability);
}

/** What a matched item does: take the target's place, or join it on the tile. */
enum class RuleAction : uint8_t {
	Replace,
	Add,
};

struct ReplacementTarget {
	uint16_t id = 0;
	int probability = 0; // 0 .. kChanceMax percent

	bool operator==(const ReplacementTarget& other) const = default;
};

/** The engine's rule form: one source, a weighted set of targets. */
struct ReplacementRule {
	uint16_t fromId = 0;
	std::vector<ReplacementTarget> targets;
	/** Replace the matched item, or add the target next to it. */
	RuleAction action = RuleAction::Replace;
	/** When set the matched item is removed from the tile instead. */
	bool remove = false;
};

/**
 * The editor's rule form: a set of sources mapped to one weighted set of
 * targets, so "these six grass variants become this one tile" is a single rule.
 * Expanded into ReplacementRule entries when it is executed or counted.
 */
struct RuleGroup {
	std::vector<uint16_t> sources;
	std::vector<ReplacementTarget> targets;
	/** Replace the matched items, or add the targets next to them. */
	RuleAction action = RuleAction::Replace;
	/** When set the rule deletes the matched items; targets are kept but unused. */
	bool remove = false;

	bool operator==(const RuleGroup& other) const = default;
};

struct RuleSet {
	std::string name;
	std::vector<RuleGroup> rules;
};

/**
 * Expands editor groups into the engine's one-source-per-rule form.
 * ruleRows[i] receives the group index each expanded rule came from, so the
 * caller can report counts per rule row.
 */
std::vector<ReplacementRule> ExpandRuleGroups(const std::vector<RuleGroup>& groups, std::vector<size_t>& ruleRows);

class RuleManager {
public:
	static RuleManager& Get();

	bool SaveRuleSet(const RuleSet& ruleSet);
	RuleSet LoadRuleSet(const std::string& name);
	bool DeleteRuleSet(const std::string& name);
	std::vector<std::string> GetAvailableRuleSets();

private:
	RuleManager();
	std::string GetRulesDir();
};

#endif
