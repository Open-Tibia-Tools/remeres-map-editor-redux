#ifndef RME_UTIL_NVG_UTILS_H_
#define RME_UTIL_NVG_UTILS_H_

#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
#include "game/subtype_pattern.h"
#include "ui/gui.h"
#include <nanovg.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <optional>

namespace NvgUtils {

	// Generates RGBA pixel data for a given item ID.
	// Returns a texture ID created in the given NanoVG context.
	// Returns 0 if generation fails.
	inline NVGcolor ToNvColor(const wxColour& c) {
		return nvgRGBA(c.Red(), c.Green(), c.Blue(), c.Alpha());
	}

	// How a subtype selects a sprite pattern.
	using PatternSlot = SubtypePattern::Slot;

	// Slot in the item's pattern grid for a given definition and optional subtype.
	inline PatternSlot GetPatternSlot(const ItemDefinitionView& it, bool is_stackable, std::optional<uint16_t> subtype) {
		if (it && (it.isFluidContainer() || it.isSplash())) {
			return PatternSlot::At(Item::ServerFluidToSpritePattern(subtype.value_or(LIQUID_WATER)));
		}
		if (is_stackable) {
			return subtype.has_value()
				? PatternSlot::At(SubtypePattern::FromCount(*subtype))
				: PatternSlot::Representative();
		}
		return PatternSlot::None();
	}

	// Slot used for the composite image of an item id and optional subtype.
	inline PatternSlot GetCompositeSlot(uint16_t id, std::optional<uint16_t> subtype) {
		const auto it = g_item_definitions.get(id);
		if (!it) {
			return PatternSlot::None();
		}
		return GetPatternSlot(it, it.hasFlag(ItemFlag::Stackable), subtype);
	}

	// Creates a composite RGBA buffer from a GameSprite.
	// Returns a unique_ptr to the buffer, or nullptr on failure.
	inline std::unique_ptr<uint8_t[]> CreateCompositeRGBA(
		GameSprite& gs,
		int& outW,
		int& outH,
		std::optional<bool> is_stackable_opt = std::nullopt,
		std::optional<uint16_t> subtype_opt = std::nullopt,
		std::optional<ItemDefinitionView> item_def_opt = std::nullopt
	) {
		ItemDefinitionView it;
		if (item_def_opt.has_value()) {
			it = *item_def_opt;
		} else if (gs.getId() != 0) {
			if (const auto sid = g_item_definitions.findByClientId(gs.getId())) {
				it = g_item_definitions.get(*sid);
			}
		}

		bool is_stackable = false;
		if (is_stackable_opt.has_value()) {
			is_stackable = *is_stackable_opt;
		} else if (it) {
			is_stackable = it.hasFlag(ItemFlag::Stackable);
		}

		int pattern_x = 0;
		int pattern_y = 0;

		const auto slot = GetPatternSlot(it, is_stackable, subtype_opt);
		if (slot.kind == PatternSlot::Kind::Representative) {
			// Palette preview: show a pile only when the sprite has a third pattern column.
			const auto offset = SubtypePattern::Resolve(SubtypePattern::RepresentativeIndex(gs.pattern_x), gs.pattern_x, gs.pattern_y);
			pattern_x = offset.x;
			pattern_y = offset.y;
		} else if (slot.kind == PatternSlot::Kind::Index) {
			const auto offset = SubtypePattern::Resolve(slot.index, gs.pattern_x, gs.pattern_y);
			pattern_x = offset.x;
			pattern_y = offset.y;
		} else if (it && it.hasFlag(ItemFlag::IsHangable) && gs.pattern_x > 1) {
			pattern_x = 1;
		}

		auto buildComposite = [&](int pat_x, int pat_y) -> std::unique_ptr<uint8_t[]> {
			const auto metrics = gs.getPlainLayoutMetrics(-1, pat_x, pat_y, 0, 0);
			outW = metrics.total_width;
			outH = metrics.total_height;

			if (outW <= 0 || outH <= 0) {
				return nullptr;
			}

			size_t bufferSize = static_cast<size_t>(outW) * outH * 4;
			auto composite = std::make_unique<uint8_t[]>(bufferSize);
			std::fill(composite.get(), composite.get() + bufferSize, 0);
			int pattern_z = 0;
			int frame = 0;

			bool hasVisiblePixels = false;

			for (int l = 0; l < gs.layers; ++l) {
				for (int w = 0; w < gs.width; ++w) {
					for (int h = 0; h < gs.height; ++h) {
						int spriteIdx = gs.getIndex(w, h, l, pat_x, pat_y, pattern_z, frame);
						if (spriteIdx < 0 || (size_t)spriteIdx >= gs.spriteList.size()) {
							continue;
						}

						auto spriteData = gs.spriteList[spriteIdx]->getRGBAData();
						if (!spriteData) {
							continue;
						}
						const auto dimensions = gs.spriteList[spriteIdx]->getDimensions();

						int part_x = 0;
						for (int column = w + 1; column < gs.width; ++column) {
							part_x += metrics.column_widths[column];
						}
						int part_y = 0;
						for (int row = h + 1; row < gs.height; ++row) {
							part_y += metrics.row_heights[row];
						}

						for (int sy = 0; sy < dimensions.height; ++sy) {
							for (int sx = 0; sx < dimensions.width; ++sx) {
								int dy = part_y + sy;
								int dx = part_x + sx;

								if (dx < 0 || dx >= outW || dy < 0 || dy >= outH) {
									continue;
								}

								int src_idx = (sy * dimensions.width + sx) * 4;
								int dst_idx = (dy * outW + dx) * 4;

								uint8_t sa = spriteData[src_idx + 3];
								if (sa == 0) {
									continue;
								}

								hasVisiblePixels = true;

								if (sa == 255) {
									composite[dst_idx + 0] = spriteData[src_idx + 0];
									composite[dst_idx + 1] = spriteData[src_idx + 1];
									composite[dst_idx + 2] = spriteData[src_idx + 2];
									composite[dst_idx + 3] = 255;
								} else {
									float a = sa / 255.0f;
									float inv_a = 1.0f - a;
									composite[dst_idx + 0] = (uint8_t)(spriteData[src_idx + 0] * a + composite[dst_idx + 0] * inv_a);
									composite[dst_idx + 1] = (uint8_t)(spriteData[src_idx + 1] * a + composite[dst_idx + 1] * inv_a);
									composite[dst_idx + 2] = (uint8_t)(spriteData[src_idx + 2] * a + composite[dst_idx + 2] * inv_a);
									composite[dst_idx + 3] = std::max(composite[dst_idx + 3], sa);
								}
							}
						}
					}
				}
			}
			if (!hasVisiblePixels) {
				return nullptr;
			}
			return composite;
		};

		auto result = buildComposite(pattern_x, pattern_y);
		if (!result && (pattern_x != 0 || pattern_y != 0)) {
			result = buildComposite(0, 0);
		}
		return result;
	}

	// Generates RGBA pixel data for a given item ID and optional subtype (fluid / charges / count).
	// Returns a texture ID created in the given NanoVG context.
	// Returns 0 if generation fails.
	inline int CreateItemTexture(NVGcontext* vg, uint16_t id, std::optional<uint16_t> subtype = std::nullopt) {
		const auto it = g_item_definitions.get(id);
		GameSprite* gs = it ? dynamic_cast<GameSprite*>(g_gui.gfx.getSprite(it.clientId())) : nullptr;
		if (!gs) {
			return 0;
		}

		int w, h;
		auto composite = CreateCompositeRGBA(*gs, w, h, it.hasFlag(ItemFlag::Stackable), subtype, it);
		if (!composite) {
			return 0;
		}

		return nvgCreateImageRGBA(vg, w, h, 0, composite.get());
	}

} // namespace NvgUtils

#endif
