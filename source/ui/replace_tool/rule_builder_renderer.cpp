#include "app/main.h"
#include "ui/replace_tool/rule_builder_renderer.h"
#include "ui/theme.h"
#include "item_definitions/core/item_definition_store.h"
#include "util/image_manager.h"

#include <algorithm>
#include <format>
#include <string_view>

namespace rme::replace_tool {

namespace {

constexpr uint64_t kTrashImageId = 0x1'0000'0100ULL;
constexpr uint64_t kTrashActiveImageId = 0x1'0000'0101ULL;

constexpr int kRowNumberFont = 13;
constexpr int kHeaderFont = 13;
constexpr int kPercentFont = 13;
constexpr int kDropHintFont = 14;

const char* ActionLabel(bool remove, RuleAction action) {
	if (remove) {
		return "REMOVE";
	}
	return action == RuleAction::Add ? "ADD" : "REPLACE";
}

NVGcolor ActionColour(bool remove, RuleAction action) {
	if (remove) {
		return rme::ui::grid::NvgColor(Theme::Role::Error);
	}
	return action == RuleAction::Add ? rme::ui::grid::NvgColor(Theme::Role::Success) : rme::ui::grid::NvgColor(Theme::Role::Accent);
}

NVGcolor ActionColour(RuleAction action) {
	return action == RuleAction::Add ? rme::ui::grid::NvgColor(Theme::Role::Success) : rme::ui::grid::NvgColor(Theme::Role::Accent);
}

wxString ItemLabel(uint16_t itemId) {
	const auto definition = g_item_definitions.get(itemId);
	if (definition && !definition.name().empty()) {
		return wxstr(std::string(definition.name()));
	}
	return wxstr(std::format("{}", itemId));
}

void DrawTextureInRect(NVGcontext* vg, const wxRect& rect, int texture) {
	if (texture <= 0) {
		return;
	}

	int textureWidth = 0;
	int textureHeight = 0;
	nvgImageSize(vg, texture, &textureWidth, &textureHeight);
	if (textureWidth <= 0 || textureHeight <= 0) {
		return;
	}

	const float scale = std::min(static_cast<float>(rect.width) / textureWidth, static_cast<float>(rect.height) / textureHeight);
	const float drawWidth = textureWidth * scale;
	const float drawHeight = textureHeight * scale;
	const float drawX = rect.x + (rect.width - drawWidth) / 2.0f;
	const float drawY = rect.y + (rect.height - drawHeight) / 2.0f;

	nvgBeginPath(vg);
	nvgRect(vg, drawX, drawY, drawWidth, drawHeight);
	nvgFillPaint(vg, nvgImagePattern(vg, drawX, drawY, drawWidth, drawHeight, 0.0f, texture, 1.0f));
	nvgFill(vg);
}

} // namespace

void RuleBuilderRenderer::Render(
	const RenderContext& ctx,
	const RuleModel& model,
	const std::vector<RowLayout>& layouts,
	const wxRect& dropZoneRect,
	const SelectionState& selection,
	const Hit& hover,
	const Hit& dragHover,
	bool externalDrag
) {
	NVGcontext* vg = ctx.vg;
	if (!vg) {
		return;
	}

	for (size_t i = 0; i < model.GetRuleCount() && i < layouts.size(); ++i) {
		DrawRow(ctx, model, static_cast<int>(i), layouts[i], selection, hover, dragHover, externalDrag);
	}

	DrawDropZone(vg, dropZoneRect, model.GetRuleCount() == 0, externalDrag && dragHover.type == HitType::NewRule);

	// The header band stays fixed while rows scroll under it.
	nvgSave(vg);
	nvgTranslate(vg, 0.0f, static_cast<float>(ctx.scrollY));
	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, 0.0f, static_cast<float>(ctx.width), static_cast<float>(LayoutMetrics::kHeaderH));
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Header));
	nvgFill(vg);
	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, static_cast<float>(LayoutMetrics::kHeaderH) - 0.5f, static_cast<float>(ctx.width), 1.0f);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Border, 160));
	nvgFill(vg);
	DrawColumnHeaders(ctx, layouts, externalDrag && dragHover.type == HitType::NewRule);
	nvgRestore(vg);
}

void RuleBuilderRenderer::DrawColumnHeaders(const RenderContext& ctx, const std::vector<RowLayout>& layouts, bool isNewRuleHover) {
	if (layouts.empty()) {
		return;
	}
	NVGcontext* vg = ctx.vg;
	const RowLayout& layout = layouts.front();

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, static_cast<float>(kHeaderFont));
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Text, 235));

	const float labelY = LayoutMetrics::kHeaderH / 2.0f;
	nvgText(vg, static_cast<float>(LayoutMetrics::kIndexW), labelY, "IF FOUND", nullptr);
	nvgText(vg, static_cast<float>(LayoutMetrics::kIndexW + layout.columnW + LayoutMetrics::kActionW), labelY, "THEN", nullptr);

	if (isNewRuleHover) {
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Text, 255));
		nvgText(vg, static_cast<float>(ctx.width - LayoutMetrics::kGutter), labelY, "release to add a rule", nullptr);
	}
}

void RuleBuilderRenderer::DrawRow(
	const RenderContext& ctx,
	const RuleModel& model,
	int rowIndex,
	const RowLayout& layout,
	const SelectionState& selection,
	const Hit& hover,
	const Hit& dragHover,
	bool externalDrag
) {
	NVGcontext* vg = ctx.vg;
	const RuleGroup* rulePtr = model.GetRule(rowIndex);
	if (!rulePtr) {
		return;
	}
	const RuleGroup& rule = *rulePtr;
	const bool removed = rule.remove;
	const bool rowHovered = hover.row == rowIndex || (externalDrag && dragHover.row == rowIndex);

	if (rowIndex % 2 == 1) {
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, static_cast<float>(layout.y), static_cast<float>(ctx.width), static_cast<float>(layout.height - 1));
		nvgFillColor(vg, nvgRGBA(128, 128, 128, 20));
		nvgFill(vg);
	}
	if (rowHovered) {
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, static_cast<float>(layout.y), static_cast<float>(ctx.width), static_cast<float>(layout.height - 1));
		nvgFillColor(vg, nvgRGBA(128, 128, 128, 38));
		nvgFill(vg);
	}
	if (removed) {
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, static_cast<float>(layout.y), static_cast<float>(ctx.width), static_cast<float>(layout.height - 1));
		nvgFillColor(vg, nvgRGBA(190, 50, 50, 48));
		nvgFill(vg);
	}

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, static_cast<float>(kRowNumberFont));
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Text, model.IsRowTargetless(rowIndex) ? 130 : 220));
	const float tileCenterY = static_cast<float>(layout.cellY + LayoutMetrics::SourceSlotHeight(ctx.iconSize, ctx.showLabels) / 2.0f);
	nvgText(vg, static_cast<float>(LayoutMetrics::kGutter), tileCenterY, std::format("{}", rowIndex + 1).c_str(), nullptr);

	nvgBeginPath(vg);
	nvgRect(vg, static_cast<float>(LayoutMetrics::kGutter), layout.y + layout.height - 0.5f, static_cast<float>(ctx.width - LayoutMetrics::kGutter * 2), 1.0f);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Border, 110));
	nvgFill(vg);

	DrawActionButton(vg, layout.actionButton, rule.remove, rule.action, hover.type == HitType::RuleAction && hover.row == rowIndex);

	for (size_t s = 0; s < layout.sources.size(); ++s) {
		const bool onSource = ((hover.type == HitType::Source || hover.type == HitType::RemoveSource)
			&& hover.row == rowIndex && hover.index == static_cast<int>(s))
			|| (externalDrag && (dragHover.type == HitType::Source || dragHover.type == HitType::RemoveSource)
			&& dragHover.row == rowIndex && dragHover.index == static_cast<int>(s));
		const bool showBadge = onSource && hover.type == HitType::RemoveSource;
		DrawSlot(ctx, layout.sources[s], rule.sources[s], selection.IsSelected(rowIndex, static_cast<int>(s), -1), onSource, showBadge);
	}

	const bool addSourceHovered = (hover.type == HitType::AddSource && hover.row == rowIndex)
		|| (externalDrag && dragHover.type == HitType::AddSource && dragHover.row == rowIndex);
	DrawGhostSlot(vg, layout.addSource, addSourceHovered, externalDrag);

	if (removed) {
		for (size_t t = 0; t < layout.targets.size() && t < layout.percents.size(); ++t) {
			DrawTargetSlot(ctx, layout.targets[t], layout.percents[t], rule.targets[t], rule.action, false, false, true, true);
		}
		const bool trashHovered = (hover.type == HitType::AddTarget && hover.row == rowIndex)
			|| (externalDrag && dragHover.type == HitType::AddTarget && dragHover.row == rowIndex);
		DrawTrashGlyph(ctx, layout.addTarget, true, false, trashHovered);
	} else {
		for (size_t t = 0; t < layout.targets.size() && t < layout.percents.size(); ++t) {
			const ReplacementTarget& target = rule.targets[t];
			const bool selected = selection.IsSelected(rowIndex, -1, static_cast<int>(t));
			const bool onTarget = (hover.row == rowIndex && hover.index == static_cast<int>(t)
				&& (hover.type == HitType::Target || hover.type == HitType::RemoveTarget || hover.type == HitType::Percent))
				|| (externalDrag && dragHover.row == rowIndex && dragHover.index == static_cast<int>(t)
				&& (dragHover.type == HitType::Target || dragHover.type == HitType::RemoveTarget || dragHover.type == HitType::Percent));
			const bool showBadge = onTarget && hover.type == HitType::RemoveTarget;

			DrawTargetSlot(ctx, layout.targets[t], layout.percents[t], target, rule.action, selected, onTarget, false, showBadge);
		}
		const bool addTargetHovered = (hover.type == HitType::AddTarget && hover.row == rowIndex)
			|| (externalDrag && dragHover.type == HitType::AddTarget && dragHover.row == rowIndex);
		DrawGhostSlot(vg, layout.addTarget, addTargetHovered, externalDrag);
	}

	DrawRowDelete(vg, layout.deleteRow, hover.type == HitType::DeleteRow && hover.row == rowIndex);
	DrawRowTrash(ctx, layout.trashRule, removed, hover.type == HitType::TrashRule && hover.row == rowIndex);
}

void RuleBuilderRenderer::DrawSlot(const RenderContext& ctx, const wxRect& rect, uint16_t itemId, bool selected, bool hovered, bool showRemoveBadge) {
	NVGcontext* vg = ctx.vg;
	rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f);

	int texture = 0;
	if (itemId != 0 && ctx.getItemTexture) {
		texture = ctx.getItemTexture(itemId);
	}
	rme::ui::grid::DrawCellIcon(vg, rme::ui::grid::IconRect(rect, ctx.iconSize), texture, rme::rendering::TileIndicatorType::None);

	if (ctx.showLabels && ctx.labelCache) {
		rme::ui::grid::DrawCellLabel(vg, rect, ItemLabel(itemId), selected, *ctx.labelCache);
	}

	if (showRemoveBadge) {
		DrawRemoveBadge(vg, rect, true);
	}
}

void RuleBuilderRenderer::DrawTargetSlot(
	const RenderContext& ctx,
	const wxRect& rect,
	const wxRect& percentRect,
	const ReplacementTarget& target,
	RuleAction action,
	bool selected,
	bool hovered,
	bool ignored,
	bool showRemoveBadge
) {
	NVGcontext* vg = ctx.vg;
	const wxRect tile(rect.x, rect.y, rect.width, rect.height - LayoutMetrics::kPercentBand);

	if (selected && !ignored) {
		const NVGpaint glow = nvgBoxGradient(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height), 4.0f, 9.0f, nvgRGBA(100, 150, 255, 150), nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(vg);
		nvgRect(vg, static_cast<float>(rect.x) - 8.0f, static_cast<float>(rect.y) - 8.0f, static_cast<float>(rect.width) + 16.0f, static_cast<float>(rect.height) + 16.0f);
		nvgRoundedRect(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height), 4.0f);
		nvgPathWinding(vg, NVG_HOLE);
		nvgFillPaint(vg, glow);
		nvgFill(vg);
	}

	nvgSave(vg);
	if (ignored) {
		nvgGlobalAlpha(vg, 0.35f);
	}

	nvgBeginPath(vg);
	nvgRoundedRect(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height), 4.0f);
	nvgFillColor(vg, (hovered && !ignored) ? rme::ui::grid::NvgColor(Theme::Role::CardBaseHover) : rme::ui::grid::NvgColor(Theme::Role::CardBase));
	nvgFill(vg);

	int texture = 0;
	if (target.id != 0 && ctx.getItemTexture) {
		texture = ctx.getItemTexture(target.id);
	}
	rme::ui::grid::DrawCellIcon(vg, rme::ui::grid::IconRect(tile, ctx.iconSize), texture, rme::rendering::TileIndicatorType::None);
	if (ctx.showLabels && ctx.labelCache) {
		rme::ui::grid::DrawCellLabel(vg, tile, ItemLabel(target.id), false, *ctx.labelCache);
	}
	nvgRestore(vg);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, static_cast<float>(rect.x) + 1.0f, static_cast<float>(rect.y) + 1.0f, static_cast<float>(rect.width) - 2.0f, static_cast<float>(rect.height) - 2.0f, 4.0f);
	nvgStrokeColor(vg, ignored ? rme::ui::grid::NvgColor(Theme::Role::TextSubtle, 160) : ActionColour(action));
	nvgStrokeWidth(vg, 2.0f);
	nvgStroke(vg);

	if (showRemoveBadge) {
		DrawRemoveBadge(vg, rect, !ignored);
	}

	if (!ignored) {
		DrawPercentLabel(vg, percentRect, target.probability, hovered || selected);
	}
}

void RuleBuilderRenderer::DrawPercentLabel(NVGcontext* vg, const wxRect& rect, int probability, bool hovered) {
	nvgBeginPath(vg);
	nvgRect(vg, static_cast<float>(rect.x) + 2.0f, static_cast<float>(rect.y), static_cast<float>(rect.width) - 4.0f, static_cast<float>(rect.height));
	nvgFillColor(vg, nvgRGBA(128, 128, 128, hovered ? 48 : 24));
	nvgFill(vg);

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, static_cast<float>(kPercentFont));
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Text, hovered ? 255 : 225));

	const std::string text = FormatChance(probability);
	nvgText(vg, rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f + 0.5f, text.c_str(), nullptr);
}

void RuleBuilderRenderer::DrawTrashGlyph(const RenderContext& ctx, const wxRect& rect, bool active, bool selected, bool hovered) {
	NVGcontext* vg = ctx.vg;
	const float x = static_cast<float>(rect.x);
	const float y = static_cast<float>(rect.y);
	const float w = static_cast<float>(rect.width);
	const float h = static_cast<float>(rect.height);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 4.0f);
	nvgFillColor(vg, nvgRGBA(190, 50, 50, active ? 60 : 28));
	nvgFill(vg);
	nvgStrokeColor(vg, (selected || hovered) ? rme::ui::grid::NvgColor(Theme::Role::Error) : rme::ui::grid::NvgColor(Theme::Role::Error, active ? 210 : 130));
	nvgStrokeWidth(vg, 2.0f);
	nvgStroke(vg);

	const int offset = ctx.showLabels ? LayoutMetrics::kPercentBand / 2 : 0;
	const wxRect icon(rect.x + (rect.width - LayoutMetrics::kTrashIconPx) / 2, rect.y + (rect.height - LayoutMetrics::kTrashIconPx) / 2 - offset, LayoutMetrics::kTrashIconPx, LayoutMetrics::kTrashIconPx);
	DrawTextureInRect(vg, icon, GetTrashTexture(ctx, true));

	if (ctx.showLabels && ctx.labelCache) {
		rme::ui::grid::DrawCellLabel(vg, rect, "delete", selected, *ctx.labelCache);
	}
}

void RuleBuilderRenderer::DrawGhostSlot(NVGcontext* vg, const wxRect& rect, bool hovered, bool externalDrag) {
	if (rect.width <= 0) {
		return;
	}
	const float x = static_cast<float>(rect.x);
	const float y = static_cast<float>(rect.y);
	const float w = static_cast<float>(rect.width);
	const float h = static_cast<float>(rect.height);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
	if (hovered) {
		nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::CardBaseHover));
		nvgFill(vg);
		nvgStrokeColor(vg, rme::ui::grid::NvgColor(Theme::Role::Accent));
		nvgStrokeWidth(vg, 2.0f);
	} else if (externalDrag) {
		nvgStrokeColor(vg, rme::ui::grid::NvgColor(Theme::Role::CardBorder, 230));
		nvgStrokeWidth(vg, 1.0f);
	} else {
		nvgStrokeColor(vg, rme::ui::grid::NvgColor(Theme::Role::CardBorder, 170));
		nvgStrokeWidth(vg, 1.0f);
	}
	nvgStroke(vg);

	const float cx = x + w / 2.0f;
	const float cy = y + h / 2.0f;
	const float arm = std::min(10.0f, w * 0.26f);
	nvgBeginPath(vg);
	nvgMoveTo(vg, cx - arm, cy);
	nvgLineTo(vg, cx + arm, cy);
	nvgMoveTo(vg, cx, cy - arm);
	nvgLineTo(vg, cx, cy + arm);
	nvgStrokeColor(vg, hovered ? rme::ui::grid::NvgColor(Theme::Role::Accent) : rme::ui::grid::NvgColor(Theme::Role::TextSubtle, externalDrag ? 255 : 245));
	nvgStrokeWidth(vg, 2.0f);
	nvgStroke(vg);
}

void RuleBuilderRenderer::DrawRemoveBadge(NVGcontext* vg, const wxRect& rect, bool hovered) {
	const float size = static_cast<float>(LayoutMetrics::kBadgeSize);
	const float x = static_cast<float>(rect.GetRight()) - size - 3.0f;
	const float y = static_cast<float>(rect.y) + 3.0f;

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, size, size, 3.0f);
	nvgFillColor(vg, hovered ? rme::ui::grid::NvgColor(Theme::Role::Error) : rme::ui::grid::NvgColor(Theme::Role::RaisedSurface));
	nvgFill(vg);
	nvgStrokeColor(vg, hovered ? rme::ui::grid::NvgColor(Theme::Role::Error) : rme::ui::grid::NvgColor(Theme::Role::Border, 200));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	const float arm = size * 0.24f;
	const float cx = x + size / 2.0f;
	const float cy = y + size / 2.0f;
	nvgBeginPath(vg);
	nvgMoveTo(vg, cx - arm, cy - arm);
	nvgLineTo(vg, cx + arm, cy + arm);
	nvgMoveTo(vg, cx + arm, cy - arm);
	nvgLineTo(vg, cx - arm, cy + arm);
	nvgStrokeColor(vg, hovered ? rme::ui::grid::NvgColor(Theme::Role::TextOnAccent) : rme::ui::grid::NvgColor(Theme::Role::Text, 220));
	nvgStrokeWidth(vg, 2.0f);
	nvgStroke(vg);
}

void RuleBuilderRenderer::DrawRowDelete(NVGcontext* vg, const wxRect& rect, bool hovered) {
	if (hovered) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height), 4.0f);
		nvgFillColor(vg, nvgRGBA(128, 128, 128, 40));
		nvgFill(vg);
	}

	const float cx = rect.x + rect.width / 2.0f;
	const float cy = rect.y + rect.height / 2.0f;
	const float arm = 7.0f;

	nvgBeginPath(vg);
	nvgMoveTo(vg, cx - arm, cy - arm);
	nvgLineTo(vg, cx + arm, cy + arm);
	nvgMoveTo(vg, cx + arm, cy - arm);
	nvgLineTo(vg, cx - arm, cy + arm);
	nvgStrokeColor(vg, hovered ? rme::ui::grid::NvgColor(Theme::Role::Error) : rme::ui::grid::NvgColor(Theme::Role::Text, 190));
	nvgStrokeWidth(vg, 2.2f);
	nvgStroke(vg);
}

int RuleBuilderRenderer::GetTrashTexture(const RenderContext& ctx, bool active) {
	const uint64_t key = active ? kTrashActiveImageId : kTrashImageId;
	if (ctx.getCachedTexture) {
		if (const int cached = ctx.getCachedTexture(key); cached > 0) {
			return cached;
		}
	}

	const wxBitmap bitmap = IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, wxSize(LayoutMetrics::kTrashIconPx, LayoutMetrics::kTrashIconPx), Theme::Get(active ? Theme::Role::Error : Theme::Role::TextSubtle));
	if (!bitmap.IsOk()) {
		return 0;
	}

	const wxImage image = bitmap.ConvertToImage();
	if (!image.IsOk() || image.GetWidth() <= 0 || image.GetHeight() <= 0) {
		return 0;
	}

	const int width = image.GetWidth();
	const int height = image.GetHeight();
	const unsigned char* rgb = image.GetData();
	const unsigned char* alpha = image.HasAlpha() ? image.GetAlpha() : nullptr;

	std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
	for (int i = 0; i < width * height; ++i) {
		rgba[i * 4 + 0] = rgb[i * 3 + 0];
		rgba[i * 4 + 1] = rgb[i * 3 + 1];
		rgba[i * 4 + 2] = rgb[i * 3 + 2];
		rgba[i * 4 + 3] = alpha != nullptr ? alpha[i] : 255;
	}

	if (ctx.createTexture) {
		return ctx.createTexture(key, rgba.data(), width, height);
	}
	return 0;
}

void RuleBuilderRenderer::DrawRowTrash(const RenderContext& ctx, const wxRect& rect, bool active, bool hovered) {
	NVGcontext* vg = ctx.vg;
	if (active || hovered) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height), 4.0f);
		nvgFillColor(vg, active ? nvgRGBA(190, 50, 50, 70) : nvgRGBA(128, 128, 128, 40));
		nvgFill(vg);
	}

	const int texture = GetTrashTexture(ctx, active || hovered);
	if (texture <= 0) {
		return;
	}

	const wxRect icon(rect.x + (rect.width - LayoutMetrics::kTrashIconPx) / 2, rect.y + (rect.height - LayoutMetrics::kTrashIconPx) / 2, LayoutMetrics::kTrashIconPx, LayoutMetrics::kTrashIconPx);
	DrawTextureInRect(vg, icon, texture);
}

void RuleBuilderRenderer::DrawDropZone(NVGcontext* vg, const wxRect& rect, bool emptyRules, bool active) {
	const float x = static_cast<float>(rect.x);
	const float y = static_cast<float>(rect.y);
	const float w = static_cast<float>(rect.width);
	const float h = static_cast<float>(rect.height);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
	if (active) {
		nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::Accent, 40));
		nvgFill(vg);
	}
	nvgStrokeColor(vg, active ? rme::ui::grid::NvgColor(Theme::Role::Accent) : rme::ui::grid::NvgColor(Theme::Role::CardBorder, 200));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, static_cast<float>(kDropHintFont));
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, active ? rme::ui::grid::NvgColor(Theme::Role::Text) : rme::ui::grid::NvgColor(Theme::Role::TextSubtle, 255));
	const char* hint = emptyRules ? "Drop a tile here to create your first rule" : "Drop a tile here to add another rule";
	nvgText(vg, x + w / 2.0f, y + h / 2.0f, hint, nullptr);
}

void RuleBuilderRenderer::DrawActionButton(NVGcontext* vg, const wxRect& rect, bool remove, RuleAction action, bool hovered) {
	const float x = static_cast<float>(rect.x);
	const float y = static_cast<float>(rect.y);
	const float w = static_cast<float>(rect.width);
	const float h = static_cast<float>(rect.height);

	NVGcolor colour = ActionColour(remove, action);
	if (action == RuleAction::Add && !remove) {
		colour = nvgRGBAf(colour.r * 0.78f, colour.g * 0.78f, colour.b * 0.78f, 1.0f);
	}
	if (hovered) {
		colour = nvgRGBAf(std::min(1.0f, colour.r * 1.25f), std::min(1.0f, colour.g * 1.25f), std::min(1.0f, colour.b * 1.25f), 1.0f);
	}

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, 4.0f);
	nvgFillColor(vg, colour);
	nvgFill(vg);

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, static_cast<float>(LayoutMetrics::kActionFont));
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, rme::ui::grid::NvgColor(Theme::Role::TextOnAccent));
	nvgText(vg, x + w / 2.0f, y + h / 2.0f + 0.5f, ActionLabel(remove, action), nullptr);
}

} // namespace rme::replace_tool
