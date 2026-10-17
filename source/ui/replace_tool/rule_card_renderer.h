#ifndef RME_RULE_CARD_RENDERER_H_
#define RME_RULE_CARD_RENDERER_H_

#include <nanovg.h>
#include <cstdint>

class NanoVGCanvas;

class RuleBuilderPanel;
struct ReplacementRule;

class RuleCardRenderer {
public:
	// Layout Constants (Shared: the panel lays out with these, the renderer
	// draws with them, so hit-testing and drawing cannot diverge).
	static constexpr int CARD_PADDING = 12;
	static constexpr int CARD_MARGIN_X = 10;
	static constexpr int CARD_MARGIN_Y = 8;
	static constexpr int HEADER_HEIGHT = 32;
	static constexpr int ITEM_SIZE = 56;
	static constexpr int ITEM_H = 110;
	static constexpr int ITEM_SPACING = 8;
	static constexpr int ARROW_WIDTH = 48;
	static constexpr int SECTION_GAP = 16;
	static constexpr int CARD_W = ITEM_SIZE + 20;
	/** Height of the "drop a new rule" area. */
	static constexpr int NEW_RULE_AREA_H = 52;

	static void DrawRuleItemCard(NanoVGCanvas* canvas, NVGcontext* vg, float x, float y, float w, float h, uint16_t id, bool highlight, bool isTrash, bool showDeleteOverlay, int probability = -1);
	static void DrawTrashIcon(NVGcontext* vg, float x, float y, float size, bool highlight);

	// New Static Helper Methods for RuleBuilderPanel
	static void DrawHeader(NVGcontext* vg, float width);
	static void DrawClearButton(NVGcontext* vg, float width, bool isHovered);
	static void DrawSaveButton(NVGcontext* vg, float width, bool isHovered);
	static void DrawRuleCard(RuleBuilderPanel* panel, NVGcontext* vg, int ruleIndex, int y, int width, bool hoverDelete, int dragHoverTargetIdx, int dragHoverType, bool isExternalDrag);
	static void DrawRuleArrow(NVGcontext* vg, float x, float y, float h);
	static void DrawNewRuleArea(NVGcontext* vg, float width, float y, bool isHovered);
};

#endif
