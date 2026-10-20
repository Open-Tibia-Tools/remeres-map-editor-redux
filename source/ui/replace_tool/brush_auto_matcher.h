#ifndef RME_BRUSH_AUTO_MATCHER_H_
#define RME_BRUSH_AUTO_MATCHER_H_

#include "ui/replace_tool/rule_model.h"
#include <cstdint>
#include <vector>

class Brush;

namespace rme::replace_tool {

/**
 * Domain service providing semantic matching between item definitions and brushes
 * (WallBrush, CarpetBrush, TableBrush, GroundBrush).
 */
class BrushAutoMatcher {
public:
	/** Matches a single source item ID to replacement targets within targetBrush. */
	static std::vector<ReplacementTarget> MatchItemToBrush(uint16_t sourceId, Brush* targetBrush);

	/**
	 * Inspects all unmatched rules in model and attempts semantic matching against targetBrush.
	 * Returns the number of rules matched and filled.
	 */
	static size_t AutoMatchUnmatchedRules(RuleModel& model, Brush* targetBrush);
};

} // namespace rme::replace_tool

#endif
