#include "app/main.h"
#include "replacement_engine.h"

#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "game/item.h"
#include "ui/gui.h"
#include <algorithm>
#include <map>
#include <utility>

ReplacementEngine::ReplacementEngine() : rng(std::random_device {}()) { }

bool ReplacementEngine::ResolveReplacement(uint16_t& resultId, const ReplacementRule& rule) {
	if (rule.targets.empty()) {
		return false;
	}

	// Calculate total probability weight
	int totalProb = 0;
	for (const auto& target : rule.targets) {
		totalProb += target.probability;
	}

	if (totalProb <= 0) {
		return false;
	}

	// Use a single roll from 1 to max(100, totalProb)
	// This ensures that if the UI says 100% total, exactly one target is picked.
	// If total is < 100, the remainder is "no replacement".
	std::uniform_int_distribution<int> dist(1, std::max(100, totalProb));
	int roll = dist(rng);

	int currentSum = 0;
	for (const auto& target : rule.targets) {
		currentSum += target.probability;
		if (roll <= currentSum) {
			resultId = target.id;
			return true;
		}
	}

	return false;
}

namespace {

// Rule lookup: the last rule for a given source id wins.
std::map<uint16_t, const ReplacementRule*> buildRuleMap(const std::vector<ReplacementRule>& rules) {
	std::map<uint16_t, const ReplacementRule*> ruleMap;
	for (const auto& rule : rules) {
		if (rule.fromId != 0) {
			ruleMap[rule.fromId] = &rule;
		}
	}
	return ruleMap;
}

// Visits the ground item, every top-level item and all items nested in
// containers of the given tile (iterative, no deep recursion).
// The visitor returns true when the item was affected.
template <typename VisitFn>
size_t forEachReplaceableItem(Tile* tile, VisitFn&& visit)
requires std::invocable<VisitFn&, Item*> {
	if (!tile) {
		return 0;
	}

	size_t visited = 0;

	if (tile->ground) {
		visit(tile->ground.get());
		++visited;
	}

	std::vector<Container*> pendingContainers;
	for (const auto& item : tile->items) {
		visit(item.get());
		++visited;

		if (Container* container = item->asContainer()) {
			pendingContainers.push_back(container);
		}
	}

	size_t index = 0;
	while (index < pendingContainers.size()) {
		Container* container = pendingContainers[index++];
		for (const auto& item : container->getVector()) {
			visit(item.get());
			++visited;

			if (Container* nested = item->asContainer()) {
				pendingContainers.push_back(nested);
			}
		}
	}

	return visited;
}

template <typename RuleMapT>
uint32_t countMatchesOnTile(Tile* tile, const RuleMapT& ruleMap) {
	if (!tile) {
		return 0;
	}

	uint32_t matches = 0;
	forEachReplaceableItem(tile, [&](Item* item) {
		if (ruleMap.contains(item->getID())) {
			++matches;
		}
	});
	return matches;
}

std::vector<Tile*> collectScopeTiles(Editor* editor, ReplaceScope scope, const std::vector<Position>* posVec) {
	std::vector<Tile*> tiles;
	switch (scope) {
		case ReplaceScope::Viewport:
			if (posVec) {
				for (const auto& pos : *posVec) {
					if (Tile* tile = editor->map.getTile(pos)) {
						tiles.push_back(tile);
					}
				}
			}
			break;

		case ReplaceScope::Selection:
			tiles = editor->selection.getTiles();
			break;

		case ReplaceScope::AllMap: {
			auto visitor = [&tiles](Map&, Tile* tile, long long) {
				tiles.push_back(tile);
			};
			foreach_TileOnMap(editor->map, visitor);
			break;
		}
	}
	return tiles;
}

} // namespace

void ReplacementEngine::ExecuteReplacement(Editor* editor, const std::vector<ReplacementRule>& rules, ReplaceScope scope, const std::vector<Position>* posVec) {
	if (rules.empty()) {
		return;
	}

	const auto ruleMap = buildRuleMap(rules);
	if (ruleMap.empty()) {
		return;
	}

	// One undoable action for the whole execution. Every affected tile is
	// modified on a deep copy and swapped in whole, which pairs cleanly with
	// the tile-swap the Action machinery performs on commit and undo.
	std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_REPLACE_ITEMS);

	auto tileProcessor = [&](Tile* tile) {
		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor->map);

		size_t replaced = 0;
		forEachReplaceableItem(new_tile.get(), [&](Item* item) -> bool {
			const auto it = ruleMap.find(item->getID());
			if (it == ruleMap.end()) {
				return false;
			}

			uint16_t newId;
			if (!ResolveReplacement(newId, *it->second)) {
				return false;
			}

			item->setID(newId == TRASH_ITEM_ID ? 0 : newId);
			++replaced;
			return true;
		});

		if (replaced > 0) {
			TileOperations::update(new_tile.get());
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
		}
	};

	for (Tile* tile : collectScopeTiles(editor, scope, posVec)) {
		tileProcessor(tile);
	}

	if (action->size() > 0) {
		editor->actionQueue->addAction(std::move(action));
	}
	g_gui.RefreshView();
}

std::vector<uint32_t> ReplacementEngine::CountMatchesByRule(Editor* editor, const std::vector<ReplacementRule>& rules, ReplaceScope scope, const std::vector<Position>* posVec) const {
	std::vector<uint32_t> perRule(rules.size(), 0);
	if (rules.empty()) {
		return perRule;
	}

	// Attribute every match to the effective rule for its source id, matching
	// ExecuteReplacement precedence: the last rule for each source id wins.
	// If that effective rule has no targets or non-positive total probability,
	// it replaces nothing and is excluded from counts.
	std::map<uint16_t, size_t> ruleIndices;
	for (size_t index = 0; index < rules.size(); ++index) {
		const ReplacementRule& rule = rules[index];
		if (rule.fromId != 0) {
			ruleIndices[rule.fromId] = index;
		}
	}

	for (auto it = ruleIndices.begin(); it != ruleIndices.end(); ) {
		const ReplacementRule& rule = rules[it->second];
		if (rule.targets.empty()) {
			it = ruleIndices.erase(it);
			continue;
		}

		int totalProbability = 0;
		for (const auto& target : rule.targets) {
			totalProbability += target.probability;
		}
		if (totalProbability <= 0) {
			it = ruleIndices.erase(it);
			continue;
		}
		++it;
	}

	if (ruleIndices.empty()) {
		return perRule;
	}

	for (Tile* tile : collectScopeTiles(editor, scope, posVec)) {
		forEachReplaceableItem(tile, [&](Item* item) {
			if (const auto it = ruleIndices.find(item->getID()); it != ruleIndices.end()) {
				++perRule[it->second];
			}
		});
	}

	return perRule;
}