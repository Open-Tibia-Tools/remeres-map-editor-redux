#ifndef RME_UTIL_NVG_UTILS_H_
#define RME_UTIL_NVG_UTILS_H_

#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
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

		if (it && (it.isFluidContainer() || it.isSplash())) {
			const uint16_t fluid = subtype_opt.value_or(0);
			const uint8_t pattern_index = Item::ServerFluidToSpritePattern(fluid);
			if (gs.pattern_x > 0) {
				pattern_x = pattern_index % gs.pattern_x;
				if (gs.pattern_y > 0) {
					pattern_y = (pattern_index / gs.pattern_x) % gs.pattern_y;
				}
			}
		} else if (is_stackable) {
			int exactCount = 0;
			if (subtype_opt.has_value() && *subtype_opt > 0) {
				const uint16_t count = *subtype_opt;
				if (count <= 1) {
					exactCount = 0;
				} else if (count <= 2) {
					exactCount = 1;
				} else if (count <= 3) {
					exactCount = 2;
				} else if (count <= 4) {
					exactCount = 3;
				} else if (count < 10) {
					exactCount = 4;
				} else if (count < 25) {
					exactCount = 5;
				} else if (count < 50) {
					exactCount = 6;
				} else {
					exactCount = 7;
				}
			} else {
				// Representative preview (e.g. palette or catalog view)
				exactCount = (gs.pattern_x >= 3) ? 2 : 0;
			}
			if (gs.pattern_x > 0) {
				pattern_x = exactCount % gs.pattern_x;
				if (gs.pattern_y > 0) {
					pattern_y = (exactCount / gs.pattern_x) % gs.pattern_y;
				}
			}
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
	inline int CreateItemTexture(NVGcontext* vg, uint16_t id, uint16_t subtype = 0) {
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
