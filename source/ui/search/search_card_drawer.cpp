#include "ui/search/search_card_drawer.h"
#include "ui/search/search_card_layout.h"

#include "ui/theme.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"
#include "util/grid_cell_renderer.h"
#include "brushes/creature/creature_brush.h"

#include <nanovg.h>
#include <algorithm>
#include <array>
#include <format>
#include <string_view>

namespace rme::ui::search {

void SearchCardDrawer::DrawEmptyState(
	NVGcontext* vg,
	int width,
	int height,
	int scroll_pos,
	bool is_editor_open
) {
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

	const float center_y = static_cast<float>(scroll_pos + height / 2);

	nvgFontSize(vg, 14.0f);
	nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
	if (!is_editor_open) {
		nvgText(vg, width * 0.5f, center_y - 10.0f, "No map open", nullptr);
		nvgFontSize(vg, 12.0f);
		nvgText(vg, width * 0.5f, center_y + 12.0f, "Open a map to view search results", nullptr);
	} else {
		nvgText(vg, width * 0.5f, center_y - 10.0f, "No matching items found", nullptr);
		nvgFontSize(vg, 12.0f);
		nvgText(vg, width * 0.5f, center_y + 12.0f, "Try adjusting your search criteria or category filter", nullptr);
	}
}

int SearchCardDrawer::GetTextureForGroup(NVGcontext* vg, NanoVGCanvas* canvas, const SearchResultGroup& group) {
	if (!canvas) {
		return 0;
	}
	if (group.is_creature) {
		if (group.brush) {
			if (Sprite* sprite = group.brush->getSprite()) {
				return canvas->GetOrCreateSpriteTexture(vg, sprite);
			}
		}
		return 0;
	}
	return canvas->GetOrCreateItemImage(static_cast<uint16_t>(group.id));
}

void SearchCardDrawer::DrawGroupHeader(
	NVGcontext* vg,
	NanoVGCanvas* canvas,
	const wxRect& rect,
	const SearchResultGroup& group,
	size_t group_idx,
	bool selected,
	bool hovered
) {
	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);
	const float center_y = static_cast<float>(drawn.y + drawn.height / 2);

	const int dip_6 = canvas ? canvas->FromDIP(6) : 6;
	const int dip_7 = canvas ? canvas->FromDIP(7) : 7;
	const int dip_8 = canvas ? canvas->FromDIP(8) : 8;
	const int dip_26 = canvas ? canvas->FromDIP(26) : 26;
	const int dip_80 = canvas ? canvas->FromDIP(80) : 80;

	// 1. Expander Chevron Arrow
	const float arrow_x = static_cast<float>(drawn.x + dip_8);
	const float arrow_sz = static_cast<float>(dip_7);
	const NVGcolor arrow_col = selected
		? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent))
		: NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle));

	nvgBeginPath(vg);
	if (group.expanded) {
		nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
		nvgLineTo(vg, arrow_x + arrow_sz, center_y - arrow_sz * 0.5f);
		nvgLineTo(vg, arrow_x + arrow_sz * 0.5f, center_y + arrow_sz * 0.5f);
	} else {
		nvgMoveTo(vg, arrow_x, center_y - arrow_sz * 0.5f);
		nvgLineTo(vg, arrow_x + arrow_sz, center_y);
		nvgLineTo(vg, arrow_x, center_y + arrow_sz * 0.5f);
	}
	nvgClosePath(vg);
	nvgFillColor(vg, arrow_col);
	nvgFill(vg);

	// 2. Index number
	std::array<char, 32> index_buf;
	auto index_res = std::format_to_n(index_buf.data(), index_buf.size() - 1, "#{}", group_idx + 1);
	*index_res.out = '\0';
	const float index_x = arrow_x + arrow_sz + dip_6;

	nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
	nvgText(vg, index_x, center_y, index_buf.data(), nullptr);

	// Measure index text width
	float index_bounds[4] = {0};
	nvgTextBounds(vg, index_x, center_y, index_buf.data(), nullptr, index_bounds);
	const float index_width = std::max(static_cast<float>(dip_26), index_bounds[2] - index_bounds[0]);

	// 3. 32x32 Icon
	const int icon_sz = rme::ui::grid::kIconSizeBase; // 32
	const int icon_x = static_cast<int>(index_x + index_width + dip_6);
	const int icon_y = drawn.y + (drawn.height - icon_sz) / 2;
	const wxRect icon_rect(icon_x, icon_y, icon_sz, icon_sz);

	const int tex = GetTextureForGroup(vg, canvas, group);
	rme::ui::grid::DrawCellIcon(vg, icon_rect, tex, rme::rendering::TileIndicatorType::None);

	// 4. Group Name
	const float text_x = static_cast<float>(icon_x + icon_sz + dip_8);
	const float badge_w = static_cast<float>(dip_80);
	const float avail_w = std::max(20.0f, static_cast<float>(drawn.x + drawn.width) - text_x - badge_w);

	nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));

	nvgSave(vg);
	nvgScissor(vg, text_x, static_cast<float>(drawn.y), avail_w, static_cast<float>(drawn.height));
	nvgText(vg, text_x, center_y, group.name.c_str(), nullptr);
	nvgRestore(vg);

	// 5. Match count badge on the right
	std::array<char, 64> count_buf;
	auto count_res = std::format_to_n(count_buf.data(), count_buf.size() - 1, "({} {})",
		group.children.size(), group.children.size() == 1 ? "match" : "matches");
	*count_res.out = '\0';

	nvgFontSize(vg, static_cast<float>(rme::ui::grid::kListLabelFontSize)); // 14px
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
	nvgText(vg, static_cast<float>(drawn.x + drawn.width - dip_8), center_y, count_buf.data(), nullptr);
}

void SearchCardDrawer::DrawAttributeTokens(
	NVGcontext* vg,
	float start_x,
	float y,
	const SearchResultChildEntry& child,
	float gap,
	bool selected
) {
	struct AttributeToken {
		std::string_view text;
		wxColour color;
	};

	std::array<AttributeToken, 6> tokens;
	std::array<std::array<char, 64>, 6> buf_storage;
	size_t token_count = 0;

	const wxColour white_wx = Theme::Get(Theme::Role::TextOnAccent);
	const wxColour uid_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipUniqueId);
	const wxColour aid_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipActionId);
	const wxColour tele_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipTeleport);
	const wxColour text_wx = selected ? white_wx : Theme::Get(Theme::Role::TooltipTextValue);
	const wxColour accent_wx = selected ? white_wx : Theme::Get(Theme::Role::Accent);
	const wxColour subtle_wx = selected ? white_wx : Theme::Get(Theme::Role::TextSubtle);

	if (child.kind == SearchResultKind::Teleport) {
		auto res = std::format_to_n(buf_storage[token_count].data(), buf_storage[token_count].size() - 1,
			"Dest: [{}, {}, {}]", child.teleport_destination.x, child.teleport_destination.y, child.teleport_destination.z);
		*res.out = '\0';
		tokens[token_count] = { std::string_view(buf_storage[token_count].data(), res.out), tele_wx };
		++token_count;
	} else if (child.kind == SearchResultKind::Creature) {
		tokens[token_count++] = {
			child.subtitle.empty() ? std::string_view("Creature") : std::string_view(child.subtitle),
			accent_wx
		};
	} else if (child.kind == SearchResultKind::Pickupable) {
		tokens[token_count++] = {
			"Pickupable",
			accent_wx
		};
	} else if (child.kind == SearchResultKind::ServerNumber) {
		tokens[token_count++] = {
			child.subtitle,
			aid_wx
		};
	}

	if (child.unique_id > 0) {
		auto res = std::format_to_n(buf_storage[token_count].data(), buf_storage[token_count].size() - 1,
			"UID: {}", child.unique_id);
		*res.out = '\0';
		tokens[token_count] = { std::string_view(buf_storage[token_count].data(), res.out), uid_wx };
		++token_count;
	}

	if (child.action_id > 0) {
		auto res = std::format_to_n(buf_storage[token_count].data(), buf_storage[token_count].size() - 1,
			"AID: {}", child.action_id);
		*res.out = '\0';
		tokens[token_count] = { std::string_view(buf_storage[token_count].data(), res.out), aid_wx };
		++token_count;
	}

	if (!child.text.empty()) {
		auto res = std::format_to_n(buf_storage[token_count].data(), buf_storage[token_count].size() - 1,
			"Text: \"{}\"", child.text);
		*res.out = '\0';
		tokens[token_count] = { std::string_view(buf_storage[token_count].data(), res.out), text_wx };
		++token_count;
	}

	if (token_count == 0) {
		tokens[token_count++] = {
			(child.kind == SearchResultKind::Container) ? "Container Instance" : "Item Instance",
			subtle_wx
		};
	}

	nvgFontFace(vg, "sans");
	nvgFontSize(vg, 13.0f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

	float cur_x = start_x;
	for (size_t i = 0; i < token_count; ++i) {
		const auto& token = tokens[i];
		nvgFillColor(vg, NvgUtils::ToNvColor(token.color));
		nvgText(vg, cur_x, y, token.text.data(), token.text.data() + token.text.size());

		float bounds[4] = {0};
		nvgTextBounds(vg, cur_x, y, token.text.data(), token.text.data() + token.text.size(), bounds);
		cur_x = bounds[2] + gap;
	}
}

void SearchCardDrawer::DrawContainerCard(
	NVGcontext* vg,
	NanoVGCanvas* canvas,
	const wxRect& rect,
	const SearchResultChildEntry& child,
	size_t child_idx,
	size_t total_children,
	bool selected,
	bool hovered
) {
	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);
	const auto geo = CardGeometry::ForContainer(drawn, total_children, canvas);

	const wxColour meta_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
	const wxColour accent_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Accent);
	const int dip_8 = canvas ? canvas->FromDIP(8) : 8;
	const int dip_10 = canvas ? canvas->FromDIP(10) : 10;
	const int dip_44 = canvas ? canvas->FromDIP(44) : 44;

	// 1. Child Index number (#1, #2, ...) on the left
	std::array<char, 32> index_buf;
	auto index_res = std::format_to_n(index_buf.data(), index_buf.size() - 1, "#{}", child_idx + 1);
	*index_res.out = '\0';

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
	nvgText(vg, static_cast<float>(geo.index_x + geo.index_w), geo.center_y, index_buf.data(), nullptr);

	// 2. Main Container 32x32 icon on the left (vertically centered)
	const wxRect main_slot = geo.GetMainIconRect();
	rme::ui::grid::DrawCardChrome(vg, main_slot, false, false, 0.0f, false);

	const int main_tex = canvas ? canvas->GetOrCreateItemImage(child.item_id) : 0;
	rme::ui::grid::DrawCellIcon(vg, main_slot, main_tex, rme::rendering::TileIndicatorType::None);

	// 3. Row 1: Horizontal strip of 32x32 contained items (or empty 32x32 box)
	if (child.container_items.empty()) {
		const wxRect empty_slot = geo.GetContainedSlotRect(0);
		rme::ui::grid::DrawCardChrome(vg, empty_slot, false, false, 0.0f, false);

		nvgFontFace(vg, "sans");
		nvgFontSize(vg, 12.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
		nvgText(vg, static_cast<float>(geo.content_x + geo.icon_sz + dip_8), static_cast<float>(geo.container_icons_y + geo.icon_sz / 2), "(empty)", nullptr);
	} else {
		const int max_x = drawn.x + drawn.width - dip_44;
		size_t drawn_count = 0;

		for (size_t i = 0; i < child.container_items.size(); ++i) {
			const wxRect slot_rect = geo.GetContainedSlotRect(i);
			if (slot_rect.x + slot_rect.width > max_x) {
				// Overflow badge slot (+N)
				rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);

				std::array<char, 32> more_buf;
				auto more_res = std::format_to_n(more_buf.data(), more_buf.size() - 1, "+{}", child.container_items.size() - drawn_count);
				*more_res.out = '\0';

				nvgFontFace(vg, "sans");
				nvgFontSize(vg, 13.0f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, NvgUtils::ToNvColor(accent_wx));
				nvgText(vg, static_cast<float>(slot_rect.x + slot_rect.width / 2), static_cast<float>(slot_rect.y + slot_rect.height / 2), more_buf.data(), nullptr);
				break;
			}

			// 32x32 sunken slot
			rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);

			const auto& contained = child.container_items[i];
			const int ctex = canvas ? canvas->GetOrCreateItemImage(contained.item_id) : 0;
			rme::ui::grid::DrawCellIcon(vg, slot_rect, ctex, rme::rendering::TileIndicatorType::None);

			// Count badge if count > 1
			if (contained.count > 1) {
				std::array<char, 16> count_buf;
				auto count_res = std::format_to_n(count_buf.data(), count_buf.size() - 1, "{}", contained.count);
				*count_res.out = '\0';

				nvgFontSize(vg, 11.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);

				// Shadow
				nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
				nvgText(vg, static_cast<float>(slot_rect.x + slot_rect.width), static_cast<float>(slot_rect.y + slot_rect.height), count_buf.data(), nullptr);

				// Text
				const wxColour count_col = Theme::Get(Theme::Role::TooltipCountText);
				nvgFillColor(vg, nvgRGBA(count_col.Red(), count_col.Green(), count_col.Blue(), 255));
				nvgText(vg, static_cast<float>(slot_rect.x + slot_rect.width - 1), static_cast<float>(slot_rect.y + slot_rect.height - 1), count_buf.data(), nullptr);
			}

			++drawn_count;
		}
	}

	// 4. Row 2: Action ID, Unique ID on top, Position on bottom
	nvgSave(vg);
	nvgScissor(vg, static_cast<float>(geo.content_x), static_cast<float>(drawn.y),
		static_cast<float>(drawn.x + drawn.width - geo.content_x - dip_8), static_cast<float>(drawn.height));

	DrawAttributeTokens(vg, static_cast<float>(geo.content_x), geo.top_y, child, static_cast<float>(dip_10), selected);

	// Bottom line: Precomputed Position
	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
	nvgText(vg, static_cast<float>(geo.content_x), geo.bottom_y, child.formatted_pos.c_str(), nullptr);

	nvgRestore(vg);
}

void SearchCardDrawer::DrawChildCard(
	NVGcontext* vg,
	NanoVGCanvas* canvas,
	const wxRect& rect,
	const SearchResultChildEntry& child,
	size_t child_idx,
	size_t total_children,
	bool selected,
	bool hovered
) {
	const wxRect drawn = rme::ui::grid::DrawCardChrome(vg, rect, selected, hovered, 0.0f, false);
	const auto geo = CardGeometry::ForChild(drawn, total_children, canvas);

	const wxColour meta_wx = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);
	const int dip_8 = canvas ? canvas->FromDIP(8) : 8;
	const int dip_10 = canvas ? canvas->FromDIP(10) : 10;

	// 1. Child Index number (#1, #2, ...) on the left
	std::array<char, 32> index_buf;
	auto index_res = std::format_to_n(index_buf.data(), index_buf.size() - 1, "#{}", child_idx + 1);
	*index_res.out = '\0';

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
	nvgText(vg, static_cast<float>(geo.index_x + geo.index_w), geo.center_y, index_buf.data(), nullptr);

	// 2. 32x32 item/creature icon next to index
	const wxRect slot_rect = geo.GetMainIconRect();
	rme::ui::grid::DrawCardChrome(vg, slot_rect, false, false, 0.0f, false);

	int tex = 0;
	if (canvas) {
		if (child.is_creature) {
			if (child.brush) {
				if (Sprite* sprite = child.brush->getSprite()) {
					tex = canvas->GetOrCreateSpriteTexture(vg, sprite);
				}
			}
		} else {
			tex = canvas->GetOrCreateItemImage(child.item_id);
		}
	}
	rme::ui::grid::DrawCellIcon(vg, slot_rect, tex, rme::rendering::TileIndicatorType::None);

	// 3. Text section next to 32x32 icon
	nvgSave(vg);
	nvgScissor(vg, static_cast<float>(geo.content_x), static_cast<float>(drawn.y),
		static_cast<float>(drawn.x + drawn.width - geo.content_x - dip_8), static_cast<float>(drawn.height));

	DrawAttributeTokens(vg, static_cast<float>(geo.content_x), geo.top_y, child, static_cast<float>(dip_10), selected);

	// Bottom line: Precomputed Coordinates
	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, NvgUtils::ToNvColor(meta_wx));
	nvgText(vg, static_cast<float>(geo.content_x), geo.bottom_y, child.formatted_pos.c_str(), nullptr);

	nvgRestore(vg);
}

} // namespace rme::ui::search
