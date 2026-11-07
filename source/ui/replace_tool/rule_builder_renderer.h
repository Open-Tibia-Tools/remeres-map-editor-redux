#ifndef RME_RULE_BUILDER_RENDERER_H_
#define RME_RULE_BUILDER_RENDERER_H_

#include "ui/replace_tool/rule_model.h"
#include "ui/replace_tool/rule_builder_layout.h"
#include "util/grid_cell_renderer.h"

#include <nanovg.h>
#include <functional>

namespace rme::replace_tool {

struct SelectionState {
	int row = -1;
	int sourceIndex = -1;
	int targetIndex = -1;

	bool IsSelected(int r, int s, int t) const {
		return row == r && sourceIndex == s && targetIndex == t;
	}
};

struct RenderContext {
	NVGcontext* vg = nullptr;
	int width = 0;
	int height = 0;
	int scrollY = 0;
	int iconSize = 32;
	bool showLabels = false;
	rme::ui::grid::LabelCache* labelCache = nullptr;

	std::function<int(uint16_t)> getItemTexture;
	std::function<int(uint64_t)> getCachedTexture;
	std::function<int(uint64_t, const unsigned char*, int, int)> createTexture;
};

class RuleBuilderRenderer {
public:
	static void Render(
		const RenderContext& ctx,
		const RuleModel& model,
		const std::vector<RowLayout>& layouts,
		const wxRect& dropZoneRect,
		const SelectionState& selection,
		const Hit& hover,
		const Hit& dragHover,
		bool externalDrag
	);

private:
	static void DrawColumnHeaders(const RenderContext& ctx, const std::vector<RowLayout>& layouts, bool isNewRuleHover);
	static void DrawRow(
		const RenderContext& ctx,
		const RuleModel& model,
		int rowIndex,
		const RowLayout& layout,
		const SelectionState& selection,
		const Hit& hover,
		const Hit& dragHover,
		bool externalDrag
	);
	static void DrawSlot(const RenderContext& ctx, const wxRect& rect, uint16_t itemId, bool selected, bool hovered, bool showRemoveBadge);
	static void DrawTargetSlot(
		const RenderContext& ctx,
		const wxRect& rect,
		const wxRect& percentRect,
		const ReplacementTarget& target,
		RuleAction action,
		bool selected,
		bool hovered,
		bool ignored,
		bool showRemoveBadge
	);
	static void DrawPercentLabel(NVGcontext* vg, const wxRect& rect, int probability, bool hovered);
	static void DrawRemoveBadge(NVGcontext* vg, const wxRect& rect, bool hovered);
	static void DrawGhostSlot(NVGcontext* vg, const wxRect& rect, bool hovered, bool externalDrag);
	static void DrawTrashGlyph(const RenderContext& ctx, const wxRect& rect, bool active, bool selected, bool hovered);
	static void DrawRowDelete(NVGcontext* vg, const wxRect& rect, bool hovered);
	static void DrawRowTrash(const RenderContext& ctx, const wxRect& rect, bool active, bool hovered);
	static void DrawDropZone(NVGcontext* vg, const wxRect& rect, bool emptyRules, bool active);
	static void DrawActionButton(NVGcontext* vg, const wxRect& rect, bool remove, RuleAction action, bool hovered);
	static int GetTrashTexture(const RenderContext& ctx, bool active);
};

} // namespace rme::replace_tool

#endif
