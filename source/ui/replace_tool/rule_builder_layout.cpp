#include "app/main.h"
#include "ui/replace_tool/rule_builder_layout.h"

#include <algorithm>
#include <string_view>

namespace rme::replace_tool {

int LayoutMetrics::SlotWidth(int iconSize) {
	return rme::ui::grid::CellWidth(iconSize);
}

int LayoutMetrics::SourceSlotHeight(int iconSize, bool showLabels) {
	return rme::ui::grid::CellHeight(iconSize, showLabels);
}

int LayoutMetrics::TargetSlotHeight(int iconSize, bool showLabels) {
	return SourceSlotHeight(iconSize, showLabels) + kPercentBand;
}

int LayoutMetrics::CellPitch(int iconSize) {
	return SlotWidth(iconSize) + kGutter;
}

int LayoutMetrics::ActionButtonWidth(bool remove, RuleAction action) {
	const char* label = remove ? "REMOVE" : (action == RuleAction::Add ? "ADD" : "REPLACE");
	const size_t chars = std::string_view(label).size();
	return std::clamp(static_cast<int>(chars * 6.4f) + 26, kActionButtonMinW, kActionButtonMaxW);
}

std::vector<RowLayout> RuleBuilderLayout::ComputeLayout(
	const RuleModel& model,
	int clientWidth,
	int iconSize,
	bool showLabels,
	int& outRowsBottom,
	int& outTotalHeight,
	int& outRowPitch
) {
	const int width = std::max(clientWidth, 320);
	std::vector<RowLayout> layouts;
	layouts.reserve(model.GetRuleCount());

	const int slotW = LayoutMetrics::SlotWidth(iconSize);
	const int tileH = LayoutMetrics::SourceSlotHeight(iconSize, showLabels);
	const int contentH = LayoutMetrics::TargetSlotHeight(iconSize, showLabels);
	const int cellPitch = LayoutMetrics::CellPitch(iconSize);
	const int rowPitch = contentH + LayoutMetrics::kGutter;
	outRowPitch = rowPitch;

	int y = LayoutMetrics::kHeaderH + LayoutMetrics::kRowPad;
	for (size_t r = 0; r < model.GetRuleCount(); ++r) {
		const RuleGroup* rulePtr = model.GetRule(r);
		if (!rulePtr) {
			continue;
		}
		const RuleGroup& rule = *rulePtr;

		RowLayout layout;
		layout.y = y;
		layout.cellY = y + LayoutMetrics::kRowPad;

		const int contentW = std::max(2 * cellPitch, width - LayoutMetrics::kIndexW - LayoutMetrics::kActionW - LayoutMetrics::kRowActionW);
		const int srcCols = std::clamp(std::max(1, (contentW / 2 + LayoutMetrics::kGutter) / cellPitch), 1, 10);
		const int dstCols = std::max(1, ((contentW - srcCols * cellPitch) + LayoutMetrics::kGutter) / cellPitch);
		const int srcW = srcCols * cellPitch;
		layout.arrowX = LayoutMetrics::kIndexW + srcW + LayoutMetrics::kActionW / 2;
		layout.columnW = srcW;

		const int srcCount = static_cast<int>(rule.sources.size()) + 1; // trailing ghost slot
		const int dstCount = static_cast<int>(rule.targets.size()) + 1;
		const int srcRows = (srcCount + srcCols - 1) / srcCols;
		const int dstRows = (dstCount + dstCols - 1) / dstCols;
		const int gridRows = std::max(srcRows, dstRows);
		layout.height = LayoutMetrics::kRowPad * 2 + gridRows * rowPitch - LayoutMetrics::kGutter;

		const int centerY = layout.cellY + tileH / 2;

		for (int i = 0; i < srcCount; ++i) {
			const wxRect rect(LayoutMetrics::kIndexW + (i % srcCols) * cellPitch, layout.cellY + (i / srcCols) * rowPitch, slotW, tileH);
			if (i < static_cast<int>(rule.sources.size())) {
				layout.sources.push_back(rect);
			} else {
				layout.addSource = rect;
			}
		}
		for (int i = 0; i < dstCount; ++i) {
			const wxRect rect(LayoutMetrics::kIndexW + srcW + LayoutMetrics::kActionW + (i % dstCols) * cellPitch, layout.cellY + (i / dstCols) * rowPitch, slotW, contentH);
			if (i < static_cast<int>(rule.targets.size())) {
				layout.targets.push_back(rect);
				layout.percents.push_back(wxRect(rect.x, rect.y + contentH - LayoutMetrics::kPercentBand, slotW, LayoutMetrics::kPercentBand));
			} else {
				layout.addTarget = rect;
			}
		}

		const int buttonW = LayoutMetrics::ActionButtonWidth(rule.remove, rule.action);
		layout.actionButton = wxRect(layout.arrowX - buttonW / 2, centerY - LayoutMetrics::kActionButtonH / 2, buttonW, LayoutMetrics::kActionButtonH);

		const int trashX = width - 2 - LayoutMetrics::kRowActionCell;
		const int deleteX = trashX - LayoutMetrics::kRowActionCell;
		const int actionButtonY = centerY - LayoutMetrics::kRowActionButton / 2;
		layout.deleteRow = wxRect(deleteX + (LayoutMetrics::kRowActionCell - LayoutMetrics::kRowActionButton) / 2, actionButtonY, LayoutMetrics::kRowActionButton, LayoutMetrics::kRowActionButton);
		layout.trashRule = wxRect(trashX + (LayoutMetrics::kRowActionCell - LayoutMetrics::kRowActionButton) / 2, actionButtonY, LayoutMetrics::kRowActionButton, LayoutMetrics::kRowActionButton);

		y += layout.height;
		layouts.push_back(std::move(layout));
	}

	outRowsBottom = y;
	outTotalHeight = y + LayoutMetrics::kRowPad + LayoutMetrics::kDropZoneH + LayoutMetrics::kRowPad;
	return layouts;
}

wxRect RuleBuilderLayout::DropZoneRect(int rowsBottom, int clientWidth, int iconSize) {
	const int top = std::max(rowsBottom, LayoutMetrics::kHeaderH + LayoutMetrics::kRowPad) + LayoutMetrics::kRowPad;
	const int cellPitch = LayoutMetrics::CellPitch(iconSize);
	const int w = std::max(cellPitch, clientWidth - LayoutMetrics::kGutter * 2);
	return wxRect(LayoutMetrics::kGutter, top, w, LayoutMetrics::kDropZoneH);
}

Hit RuleBuilderLayout::HitTest(
	const std::vector<RowLayout>& layouts,
	const wxRect& dropZoneRect,
	const RuleModel& model,
	int x,
	int absY
) {
	for (size_t i = 0; i < layouts.size(); ++i) {
		const RowLayout& layout = layouts[i];
		if (absY < layout.y || absY >= layout.y + layout.height) {
			continue;
		}
		const int index = static_cast<int>(i);

		for (size_t s = 0; s < layout.sources.size(); ++s) {
			const wxRect& slot = layout.sources[s];
			if (!slot.Contains(x, absY)) {
				continue;
			}
			const wxRect badge(slot.GetRight() - LayoutMetrics::kBadgeSize - 3, slot.y + 3, LayoutMetrics::kBadgeSize, LayoutMetrics::kBadgeSize);
			if (badge.Contains(x, absY)) {
				return { HitType::RemoveSource, index, static_cast<int>(s) };
			}
			return { HitType::Source, index, static_cast<int>(s) };
		}

		for (size_t p = 0; p < layout.percents.size(); ++p) {
			if (layout.percents[p].Contains(x, absY)) {
				return { HitType::Percent, index, static_cast<int>(p) };
			}
		}

		for (size_t t = 0; t < layout.targets.size(); ++t) {
			const wxRect& slot = layout.targets[t];
			if (!slot.Contains(x, absY)) {
				continue;
			}
			if (model.IsRowRemoved(index)) {
				return { HitType::None, index, -1 };
			}
			const wxRect badge(slot.GetRight() - LayoutMetrics::kBadgeSize - 3, slot.y + 3, LayoutMetrics::kBadgeSize, LayoutMetrics::kBadgeSize);
			if (badge.Contains(x, absY)) {
				return { HitType::RemoveTarget, index, static_cast<int>(t) };
			}
			return { HitType::Target, index, static_cast<int>(t) };
		}

		if (layout.actionButton.Contains(x, absY)) {
			return { HitType::RuleAction, index, -1 };
		}
		if (layout.addSource.Contains(x, absY)) {
			return { HitType::AddSource, index, -1 };
		}
		if (layout.addTarget.Contains(x, absY)) {
			return { HitType::AddTarget, index, -1 };
		}
		if (layout.deleteRow.Contains(x, absY)) {
			return { HitType::DeleteRow, index, -1 };
		}
		if (layout.trashRule.Contains(x, absY)) {
			return { HitType::TrashRule, index, -1 };
		}
		return { HitType::None, index, -1 };
	}

	if (absY >= dropZoneRect.y - LayoutMetrics::kRowPad) {
		return { HitType::NewRule, -1, -1 };
	}

	return {};
}

} // namespace rme::replace_tool
