#ifndef RME_REPLACEMENT_ENGINE_H_
#define RME_REPLACEMENT_ENGINE_H_

#include "app/main.h"
#include "rule_manager.h"
#include "map/position.h"
#include <random>
#include <vector>

enum class ReplaceScope {
	Selection,
	Viewport,
	AllMap
};

class ReplacementEngine {
public:
	ReplacementEngine();

	/**
	 * Roll once against the rule's chances: the targets form a cumulative range
	 * out of 100%, and the roll picks whichever slice it lands in. A roll past
	 * the total means nothing happens, which is what a total below 100% means.
	 * Returns nullptr in that case.
	 */
	const ReplacementTarget* ResolveReplacement(const ReplacementRule& rule);

	// Execute the replacement on the map as a single undoable action.
	// If scope is Viewport, posVec must contain the tiles to process.
	void ExecuteReplacement(class Editor* editor, const std::vector<ReplacementRule>& rules, ReplaceScope scope, const std::vector<Position>* posVec = nullptr);

	// Count the items each rule would replace, without modifying the map.
	// The returned vector is parallel to `rules`.
	std::vector<uint32_t> CountMatchesByRule(class Editor* editor, const std::vector<ReplacementRule>& rules, ReplaceScope scope, const std::vector<Position>* posVec = nullptr) const;

private:
	std::mt19937 rng;
};

#endif