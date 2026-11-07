#include "app/main.h"
#include "ui/replace_tool/brush_auto_matcher.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"

// Brush includes
#include "brushes/brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/auto_border.h"

#include <algorithm>

namespace rme::replace_tool {

std::vector<ReplacementTarget> BrushAutoMatcher::MatchItemToBrush(uint16_t sourceId, Brush* targetBrush) {
	if (sourceId == 0 || !targetBrush) {
		return {};
	}

	const auto sourceDef = g_item_definitions.get(sourceId);
	if (!sourceDef) {
		return {};
	}

	// CASE 1: WallBrush
	if (auto* targetWall = targetBrush->as<WallBrush>()) {
		const bool isWall = sourceDef.hasFlag(ItemFlag::IsWall) ||
		                    (sourceDef.editorData().brush && sourceDef.editorData().brush->is<WallBrush>());
		if (!isWall) {
			return {};
		}

		const auto alignment = static_cast<int>(sourceDef.attribute(ItemAttributeKey::BorderAlignment));
		if (alignment < 0 || alignment >= WallBrushItems::WALL_ALIGNMENT_COUNT) {
			return {};
		}

		// Check if source is a door
		DoorType doorType = WALL_UNDEFINED;
		if (sourceDef.editorData().brush && sourceDef.editorData().brush->is<WallBrush>()) {
			doorType = sourceDef.editorData().brush->as<WallBrush>()->getDoorTypeFromID(sourceId);
		}

		if (doorType != WALL_UNDEFINED) {
			const auto& targetDoors = targetWall->items.getDoorItems(alignment);
			for (const auto& door : targetDoors) {
				if (door.type == doorType && door.id != 0) {
					return { ReplacementTarget { .id = door.id, .probability = 100 } };
				}
			}
		}

		const auto& wallNode = targetWall->items.getWallNode(alignment);
		if (wallNode.items.empty()) {
			return {};
		}

		if (wallNode.items.size() == 1) {
			return { ReplacementTarget { .id = wallNode.items.front().id, .probability = 100 } };
		}

		std::vector<ReplacementTarget> targets;
		targets.reserve(wallNode.items.size());
		int assignedProb = 0;
		int prevChance = 0;
		for (size_t i = 0; i < wallNode.items.size(); ++i) {
			const auto& item = wallNode.items[i];
			const int weight = std::max(1, item.chance - prevChance);
			prevChance = item.chance;

			int prob = (i == wallNode.items.size() - 1)
				? (100 - assignedProb)
				: (wallNode.total_chance > 0 ? (weight * 100) / wallNode.total_chance : 100 / static_cast<int>(wallNode.items.size()));
			if (prob > 0) {
				targets.push_back(ReplacementTarget { .id = item.id, .probability = prob });
				assignedProb += prob;
			}
		}
		return targets;
	}

	// CASE 2: CarpetBrush
	if (auto* targetCarpet = targetBrush->as<CarpetBrush>()) {
		const bool isCarpet = sourceDef.hasFlag(ItemFlag::IsCarpet) ||
		                      (sourceDef.editorData().brush && sourceDef.editorData().brush->is<CarpetBrush>());
		if (!isCarpet) {
			return {};
		}

		const auto alignment = static_cast<int>(sourceDef.attribute(ItemAttributeKey::BorderAlignment));
		if (alignment < 0 || alignment >= static_cast<int>(targetCarpet->getItems().getGroups().size())) {
			return {};
		}

		const auto& group = targetCarpet->getItems().getGroups()[alignment];
		if (!group.items.empty()) {
			if (group.items.size() == 1) {
				return { ReplacementTarget { .id = group.items.front().id, .probability = 100 } };
			}
			std::vector<ReplacementTarget> targets;
			int assignedProb = 0;
			for (size_t i = 0; i < group.items.size(); ++i) {
				const auto& item = group.items[i];
				int prob = (i == group.items.size() - 1)
					? (100 - assignedProb)
					: (group.total_chance > 0 ? (item.chance * 100) / group.total_chance : 100 / static_cast<int>(group.items.size()));
				if (prob > 0) {
					targets.push_back(ReplacementTarget { .id = item.id, .probability = prob });
					assignedProb += prob;
				}
			}
			return targets;
		}
		return {};
	}

	// CASE 3: TableBrush
	if (auto* targetTable = targetBrush->as<TableBrush>()) {
		const bool isTable = sourceDef.hasFlag(ItemFlag::IsTable) ||
		                     (sourceDef.editorData().brush && sourceDef.editorData().brush->is<TableBrush>());
		if (!isTable) {
			return {};
		}

		int sourceAlignment = -1;
		if (sourceDef.editorData().brush && sourceDef.editorData().brush->is<TableBrush>()) {
			auto* sourceTable = sourceDef.editorData().brush->as<TableBrush>();
			for (int a = 0; a < TableBrushItems::TABLE_ALIGNMENT_COUNT; ++a) {
				if (sourceTable->getItems().hasItems(a)) {
					for (const auto& item : sourceTable->getItems().getItems(a).items) {
						if (item.item_id == sourceId) {
							sourceAlignment = a;
							break;
						}
					}
				}
				if (sourceAlignment != -1) break;
			}
		}

		if (sourceAlignment >= 0 && sourceAlignment < TableBrushItems::TABLE_ALIGNMENT_COUNT &&
		    targetTable->getItems().hasItems(sourceAlignment)) {
			const auto& node = targetTable->getItems().getItems(sourceAlignment);
			if (!node.items.empty()) {
				if (node.items.size() == 1) {
					return { ReplacementTarget { .id = node.items.front().item_id, .probability = 100 } };
				}
				std::vector<ReplacementTarget> targets;
				int assignedProb = 0;
				for (size_t i = 0; i < node.items.size(); ++i) {
					const auto& item = node.items[i];
					int prob = (i == node.items.size() - 1)
						? (100 - assignedProb)
						: (node.total_chance > 0 ? (item.chance * 100) / node.total_chance : 100 / static_cast<int>(node.items.size()));
					if (prob > 0) {
						targets.push_back(ReplacementTarget { .id = item.item_id, .probability = prob });
						assignedProb += prob;
					}
				}
				return targets;
			}
		}
		return {};
	}

	// CASE 4: GroundBrush
	if (auto* targetGround = targetBrush->as<GroundBrush>()) {
		const bool isGround = sourceDef.isGroundTile() ||
		                      (sourceDef.editorData().brush && sourceDef.editorData().brush->is<GroundBrush>());
		if (!isGround) {
			return {};
		}

		const auto alignment = static_cast<int>(sourceDef.attribute(ItemAttributeKey::BorderAlignment));
		if (alignment != BORDER_NONE) {
			for (const auto& bb : targetGround->getBorders()) {
				if (bb->autoborder && alignment >= 0 && alignment < static_cast<int>(bb->autoborder->tiles.size())) {
					uint32_t targetTileId = bb->autoborder->tiles[alignment];
					if (targetTileId != 0) {
						return { ReplacementTarget { .id = static_cast<uint16_t>(targetTileId), .probability = 100 } };
					}
				}
			}
		}

		if (!targetGround->getBorderItems().empty()) {
			if (targetGround->getBorderItems().size() == 1) {
				return { ReplacementTarget { .id = targetGround->getBorderItems().front().id, .probability = 100 } };
			}
			std::vector<ReplacementTarget> targets;
			int assignedProb = 0;
			int prevChance = 0;
			for (size_t i = 0; i < targetGround->getBorderItems().size(); ++i) {
				const auto& item = targetGround->getBorderItems()[i];
				const int delta = item.chance - prevChance;
				prevChance = item.chance;
				int prob = (i == targetGround->getBorderItems().size() - 1)
					? (100 - assignedProb)
					: (targetGround->getTotalChance() > 0 ? (delta * 100) / targetGround->getTotalChance() : 100 / static_cast<int>(targetGround->getBorderItems().size()));
				if (prob > 0) {
					targets.push_back(ReplacementTarget { .id = item.id, .probability = prob });
					assignedProb += prob;
				}
			}
			return targets;
		}
		return {};
	}

	return {};
}

size_t BrushAutoMatcher::AutoMatchUnmatchedRules(RuleModel& model, Brush* targetBrush) {
	if (!targetBrush || model.GetRuleCount() == 0) {
		return 0;
	}

	std::vector<std::pair<size_t, std::vector<ReplacementTarget>>> matches;
	for (size_t i = 0; i < model.GetRuleCount(); ++i) {
		const RuleGroup* rule = model.GetRule(i);
		if (!rule || rule->remove || !rule->targets.empty() || rule->sources.empty()) {
			continue;
		}

		const uint16_t sourceId = rule->sources.front();
		std::vector<ReplacementTarget> targets = MatchItemToBrush(sourceId, targetBrush);
		if (!targets.empty()) {
			matches.emplace_back(i, std::move(targets));
		}
	}

	if (matches.empty()) {
		return 0;
	}

	model.PushUndo();

	for (auto& [index, targets] : matches) {
		RuleGroup* rule = model.GetRuleMut(index);
		if (rule) {
			rule->targets = std::move(targets);
			rule->action = RuleAction::Replace;
		}
	}

	return matches.size();
}

} // namespace rme::replace_tool
