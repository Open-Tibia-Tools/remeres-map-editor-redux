#include "ui/find_item_window_views.h"

#include "brushes/creature/creature_brush.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/gui.h"
#include "ui/theme.h"

#include <glad/glad.h>
#include <nanovg.h>

#include <algorithm>
#include <string_view>

namespace {
	NVGcolor toColor(const wxColour& colour) {
		return nvgRGBA(colour.Red(), colour.Green(), colour.Blue(), colour.Alpha());
	}

	[[nodiscard]] std::string hoverSummary(const AdvancedFinderCatalogRow& row) {
		if (row.isCreature()) {
			return "SID: -   CID: -   " + row.label;
		}
		return "SID: " + std::to_string(row.server_id) + "   CID: " + std::to_string(row.client_id) + "   " + row.label;
	}
}

void AdvancedFinderResultsView::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	if (cached_width_ != width || cached_height_ != height) {
		updateLayoutMetrics(width);
	}

	if (empty_state_ != EmptyState::Rows || rows_.empty()) {
		drawEmptyState(vg, width, height);
		return;
	}

	const int scroll_pos = GetScrollPosition();
	const wxPoint live_mouse = ScreenToClient(wxGetMousePosition());
	const bool mouse_inside = GetClientRect().Contains(live_mouse);
	const int live_hover_index = mouse_inside ? hitTest(live_mouse.x, live_mouse.y + scroll_pos) : -1;
	hover_position_ = mouse_inside ? live_mouse : wxDefaultPosition;
	hover_index_ = live_hover_index;

	if (mode_ == AdvancedFinderResultViewMode::List) {
		const int header_height = std::max(1, listHeaderHeight());
		const wxColour header_fill = Theme::Get(Theme::Role::RaisedSurface);
		const wxColour border = Theme::Get(Theme::Role::CardBorder);
		const wxColour text = Theme::Get(Theme::Role::TextSubtle);
		const float column_image = static_cast<float>(FromDIP(56));
		const float column_sid = static_cast<float>(FromDIP(110));
		const float column_cid = static_cast<float>(FromDIP(110));
		const float y_center = header_height * 0.5f;

		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(header_height));
		nvgFillColor(vg, toColor(header_fill));
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0.0f, static_cast<float>(header_height));
		nvgLineTo(vg, static_cast<float>(width), static_cast<float>(header_height));
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, toColor(border));
		nvgStroke(vg);

		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, 12.0f);
		nvgFillColor(vg, toColor(text));
		nvgText(vg, 14.0f, y_center, "Image", nullptr);
		nvgText(vg, column_image + 12.0f, y_center, "SID", nullptr);
		nvgText(vg, column_image + column_sid + 12.0f, y_center, "CID", nullptr);
		nvgText(vg, column_image + column_sid + column_cid + 12.0f, y_center, "Name", nullptr);

		const int item_height = std::max(1, listItemHeight());
		const int content_scroll = std::max(0, scroll_pos - header_height);
		const int start_index = std::max(0, content_scroll / item_height);
		const int end_index = std::min(static_cast<int>(rows_.size()), (content_scroll + height + item_height - 1) / item_height);

		for (int index = start_index; index < end_index; ++index) {
			const wxRect rect = itemRect(static_cast<size_t>(index));
			drawListRow(vg, rect, *rows_[index], index == selected_index_, index == live_hover_index);
		}
		return;
	}

	const int row_height = std::max(1, gridCardHeight() + gridGap());
	const int first_row = std::max(0, (scroll_pos - padding_) / row_height);
	const int last_row = std::min((static_cast<int>(rows_.size()) + columns_ - 1) / columns_, (scroll_pos + height - padding_ + row_height - 1) / row_height + 1);

	for (int row = first_row; row < last_row; ++row) {
		for (int column = 0; column < columns_; ++column) {
			const int index = row * columns_ + column;
			if (index < 0 || index >= static_cast<int>(rows_.size())) {
				continue;
			}

			const wxRect rect = itemRect(static_cast<size_t>(index));
			if (rect.y > scroll_pos + height || rect.y + rect.height < scroll_pos) {
				continue;
			}

			drawGridCard(vg, rect, *rows_[index], index == selected_index_, index == live_hover_index);
		}
	}

	if (mouse_inside && live_hover_index >= 0 && live_hover_index < static_cast<int>(rows_.size())) {
		drawGridHoverInfo(vg, width, height, scroll_pos);
	}
}

int AdvancedFinderResultsView::listHeaderHeight() const {
	return FromDIP(28);
}

int AdvancedFinderResultsView::listItemHeight() const {
	return FromDIP(48);
}

int AdvancedFinderResultsView::gridCardWidth() const {
	return FromDIP(40);
}

int AdvancedFinderResultsView::gridCardHeight() const {
	return FromDIP(40);
}

int AdvancedFinderResultsView::gridPadding() const {
	return FromDIP(8);
}

int AdvancedFinderResultsView::gridGap() const {
	return FromDIP(4);
}

void AdvancedFinderResultsView::drawEmptyState(NVGcontext* vg, int width, int height) const {
	const wxColour surface = Theme::Get(Theme::Role::Background);
	const wxColour title = Theme::Get(Theme::Role::TextSubtle);
	const wxColour body = Theme::Get(Theme::Role::TextSubtle);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
	nvgFillColor(vg, toColor(surface));
	nvgFill(vg);

	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

	const char* primary = primary_message_.empty() ? "No matching items" : primary_message_.c_str();
	const char* secondary = secondary_message_.empty() ? "Try a different query or filters." : secondary_message_.c_str();

	nvgFontSize(vg, 15.0f);
	nvgFillColor(vg, toColor(title));
	nvgText(vg, 18.0f, height * 0.42f, primary, nullptr);

	nvgFontSize(vg, 12.0f);
	nvgFillColor(vg, toColor(body));
	nvgText(vg, 18.0f, height * 0.52f, secondary, nullptr);
}

void AdvancedFinderResultsView::drawSpriteBadge(NVGcontext* vg, const wxRect& rect, Sprite* sprite) const {
	if (sprite == nullptr) {
		return;
	}

	// Mutates only the internal texture cache; drawing semantics stay const.
	if (const int texture = const_cast<AdvancedFinderResultsView*>(this)->GetOrCreateSpriteTexture(vg, sprite); texture > 0) {
		const float radius = 8.0f;
		NVGpaint paint = nvgImagePattern(vg, rect.x, rect.y, rect.width, rect.height, 0.0f, texture, 1.0f);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, rect.x, rect.y, rect.width, rect.height, radius);
		nvgFillPaint(vg, paint);
		nvgFill(vg);
	}
}

void AdvancedFinderResultsView::drawRowBadge(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row) const {
	auto tech = rme::rendering::TileIndicatorType::None;
	if (row.isItem()) {
		tech = rme::rendering::TechnicalItemRegistry::Classify(
			row.server_id,
			row.client_id
		);
	}
	const Brush* b = row.brush ? row.brush : static_cast<Brush*>(row.raw_brush);
	if (tech == rme::rendering::TileIndicatorType::None && b) {
		tech = rme::rendering::TechnicalItemRegistry::GetBrushIndicatorType(b);
	}
	if (tech != rme::rendering::TileIndicatorType::None) {
		rme::rendering::DrawNanoVGIndicatorBadge(
			vg,
			tech,
			static_cast<float>(rect.x),
			static_cast<float>(rect.y),
			static_cast<float>(rect.width)
		);
		return;
	}
	drawSpriteBadge(vg, rect, spriteForRow(row));
}

void AdvancedFinderResultsView::drawListRow(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row, bool selected, bool hovered) const {
	const wxColour selected_fill = Theme::Get(Theme::Role::Accent);
	const wxColour hover_fill = Theme::Get(Theme::Role::CardBaseHover);
	const wxColour row_fill = Theme::Get(Theme::Role::CardBase);
	const wxColour border = Theme::Get(Theme::Role::CardBorder);
	const wxColour icon_fill = Theme::Get(Theme::Role::RaisedSurface);
	const wxColour icon_border = selected ? Theme::Get(Theme::Role::AccentHover) : border;
	const wxColour text = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);
	const wxColour fill_colour = selected ? selected_fill : (hovered ? hover_fill : row_fill);
	const float column_image = static_cast<float>(FromDIP(56));
	const float column_sid = static_cast<float>(FromDIP(110));
	const float column_cid = static_cast<float>(FromDIP(110));

	nvgBeginPath(vg);
	nvgRect(vg, rect.x, rect.y, rect.width, rect.height);
	nvgFillColor(vg, toColor(fill_colour));
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, toColor(border));
	nvgStroke(vg);

	const int icon_well = FromDIP(36);
	const int icon_size = FromDIP(32);
	const wxRect icon_background(rect.x + 8, rect.y + 6, FromDIP(40), rect.height - 12);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, icon_background.x, icon_background.y, icon_background.width, icon_background.height, 6.0f);
	nvgFillColor(vg, toColor(icon_fill));
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, toColor(icon_border));
	nvgStroke(vg);

	const wxRect icon_rect(
		icon_background.x + (icon_background.width - icon_well) / 2,
		rect.y + (rect.height - icon_well) / 2,
		icon_well,
		icon_well
	);
	const wxRect sprite_rect(icon_rect.x + (icon_rect.width - icon_size) / 2, icon_rect.y + (icon_rect.height - icon_size) / 2, icon_size, icon_size);
	drawRowBadge(vg, sprite_rect, row);

	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFontSize(vg, 13.0f);
	nvgFillColor(vg, toColor(text));

	const float text_y = static_cast<float>(rect.y + rect.height * 0.5f);
	nvgSave(vg);
	nvgScissor(vg, column_image + 6.0f, rect.y, rect.width - column_image - 12.0f, rect.height);
	nvgText(vg, column_image + 12.0f, text_y, std::to_string(row.server_id).c_str(), nullptr);
	nvgText(vg, column_image + column_sid + 12.0f, text_y, std::to_string(row.client_id).c_str(), nullptr);
	nvgText(vg, column_image + column_sid + column_cid + 12.0f, text_y, row.label.c_str(), nullptr);
	nvgRestore(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, column_image, rect.y);
	nvgLineTo(vg, column_image, rect.y + rect.height);
	nvgMoveTo(vg, column_image + column_sid, rect.y);
	nvgLineTo(vg, column_image + column_sid, rect.y + rect.height);
	nvgMoveTo(vg, column_image + column_sid + column_cid, rect.y);
	nvgLineTo(vg, column_image + column_sid + column_cid, rect.y + rect.height);
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, toColor(border));
	nvgStroke(vg);
}

void AdvancedFinderResultsView::drawGridCard(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row, bool selected, bool hovered) const {
	const wxColour border = Theme::Get(Theme::Role::CardBorder);
	const wxColour hover_fill = Theme::Get(Theme::Role::CardBaseHover);
	const wxColour selected_fill = Theme::Get(Theme::Role::Accent);
	const wxColour surface = Theme::Get(Theme::Role::RaisedSurface);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, rect.x, rect.y, rect.width, rect.height, 4.0f);
	nvgFillColor(vg, toColor(selected ? selected_fill : (hovered ? hover_fill : surface)));
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, toColor(selected ? Theme::Get(Theme::Role::AccentHover) : border));
	nvgStroke(vg);

	const wxRect sprite_rect(
		rect.x + (rect.width - FromDIP(32)) / 2,
		rect.y + (rect.height - FromDIP(32)) / 2,
		FromDIP(32),
		FromDIP(32)
	);
	drawRowBadge(vg, sprite_rect, row);
}

void AdvancedFinderResultsView::drawGridHoverInfo(NVGcontext* vg, int width, int height, int scroll_pos) const {
	if (hover_index_ < 0 || hover_index_ >= static_cast<int>(rows_.size()) || hover_position_ == wxDefaultPosition) {
		return;
	}

	const auto& row = *rows_[hover_index_];
	const std::string summary = hoverSummary(row);
	const wxColour panel = Theme::Get(Theme::Role::RaisedSurface);
	const wxColour border = Theme::Get(Theme::Role::CardBorder);
	const wxColour text = Theme::Get(Theme::Role::Text);

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, 12.0f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

	float bounds[4] {};
	nvgTextBounds(vg, 0.0f, 0.0f, summary.c_str(), nullptr, bounds);
	const float panel_width = std::min(static_cast<float>(width - FromDIP(24)), (bounds[2] - bounds[0]) + static_cast<float>(FromDIP(18)));
	const float panel_height = static_cast<float>(FromDIP(28));
	float panel_x = static_cast<float>(hover_position_.x + FromDIP(14));
	float panel_y = static_cast<float>(hover_position_.y + scroll_pos + FromDIP(18));
	panel_x = std::clamp(panel_x, static_cast<float>(FromDIP(8)), static_cast<float>(width) - panel_width - static_cast<float>(FromDIP(8)));
	panel_y = std::clamp(
		panel_y,
		static_cast<float>(scroll_pos + FromDIP(8)),
		static_cast<float>(scroll_pos + height) - panel_height - static_cast<float>(FromDIP(8))
	);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, panel_x, panel_y, panel_width, panel_height, 6.0f);
	nvgFillColor(vg, toColor(panel));
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, toColor(border));
	nvgStroke(vg);

	nvgSave(vg);
	nvgScissor(vg, panel_x + FromDIP(9), panel_y, panel_width - FromDIP(18), panel_height);
	nvgFillColor(vg, toColor(text));
	nvgText(vg, panel_x + FromDIP(9), panel_y + panel_height * 0.5f, summary.c_str(), nullptr);
	nvgRestore(vg);
}
