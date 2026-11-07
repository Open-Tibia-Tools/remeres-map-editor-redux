#ifndef RME_RULE_BUILDER_LAYOUT_H_
#define RME_RULE_BUILDER_LAYOUT_H_

#include "ui/replace_tool/rule_model.h"
#include "util/grid_cell_renderer.h"
#include <wx/gdicmn.h>
#include <vector>

namespace rme::replace_tool {

enum class HitType {
	None,
	Source,
	RemoveSource,
	Target,
	RemoveTarget,
	Percent,
	RuleAction,
	AddSource,
	AddTarget,
	DeleteRow,
	TrashRule,
	NewRule,
};

struct Hit {
	HitType type = HitType::None;
	int row = -1;
	int index = -1;
};

struct RowLayout {
	int y = 0;
	int height = 0;
	int arrowX = 0;
	int cellY = 0;
	int columnW = 0;
	std::vector<wxRect> sources;
	std::vector<wxRect> targets;
	std::vector<wxRect> percents;
	wxRect addSource;
	wxRect addTarget;
	wxRect deleteRow;
	wxRect trashRule;
	wxRect actionButton;
};

struct LayoutMetrics {
	static constexpr int kGutter = rme::ui::grid::kPadding;
	static constexpr int kRowPad = 6;
	static constexpr int kIndexW = 32;
	static constexpr int kActionW = 100;
	static constexpr int kActionButtonH = 24;
	static constexpr int kActionButtonMinW = 52;
	static constexpr int kActionButtonMaxW = 88;
	static constexpr int kActionFont = 11;
	static constexpr int kRowActionButton = 26;
	static constexpr int kRowActionCell = 30;
	static constexpr int kRowActionW = kRowActionCell * 2;
	static constexpr int kHeaderH = 28;
	static constexpr int kDropZoneH = 48;
	static constexpr int kPercentBand = 19;
	static constexpr int kBadgeSize = 18;
	static constexpr int kTrashIconPx = 20;

	static int SlotWidth(int iconSize);
	static int SourceSlotHeight(int iconSize, bool showLabels);
	static int TargetSlotHeight(int iconSize, bool showLabels);
	static int CellPitch(int iconSize);
	static int ActionButtonWidth(bool remove, RuleAction action);
};

class RuleBuilderLayout {
public:
	static std::vector<RowLayout> ComputeLayout(
		const RuleModel& model,
		int clientWidth,
		int iconSize,
		bool showLabels,
		int& outRowsBottom,
		int& outTotalHeight,
		int& outRowPitch
	);

	static wxRect DropZoneRect(int rowsBottom, int clientWidth, int iconSize);

	static Hit HitTest(
		const std::vector<RowLayout>& layouts,
		const wxRect& dropZoneRect,
		const RuleModel& model,
		int x,
		int absY
	);
};

} // namespace rme::replace_tool

#endif
