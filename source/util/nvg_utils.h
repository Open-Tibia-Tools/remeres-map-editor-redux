#ifndef RME_UTIL_NVG_UTILS_H_
#define RME_UTIL_NVG_UTILS_H_

#include "item_definitions/core/item_definition_store.h"
#include "ui/gui.h"
#include <nanovg.h>
#include <vector>
#include <algorithm>
#include <memory>

namespace NvgUtils {

	// Generates RGBA pixel data for a given item ID.
	// Returns a texture ID created in the given NanoVG context.
	// Returns 0 if generation fails.
	inline NVGcolor ToNvColor(const wxColour& c) {
		return nvgRGBA(c.Red(), c.Green(), c.Blue(), c.Alpha());
	}

	// Creates a composite RGBA buffer from a GameSprite.
	// Returns a unique_ptr to the buffer, or nullptr on failure.
	inline std::unique_ptr<uint8_t[]> CreateCompositeRGBA(GameSprite& gs, int& outW, int& outH) {
		const auto metrics = gs.getPlainLayoutMetrics(-1, (gs.pattern_x >= 3) ? 2 : 0, 0, 0, 0);
		outW = metrics.total_width;
		outH = metrics.total_height;

		if (outW <= 0 || outH <= 0) {
			return nullptr;
		}

		size_t bufferSize = static_cast<size_t>(outW) * outH * 4;
		auto composite = std::make_unique<uint8_t[]>(bufferSize);
		std::fill(composite.get(), composite.get() + bufferSize, 0);

		int pattern_x = (gs.pattern_x >= 3) ? 2 : 0;
		int pattern_y = 0;
		int pattern_z = 0;
		int frame = 0;

		bool hasVisiblePixels = false;

		for (int l = 0; l < gs.layers; ++l) {
			for (int w = 0; w < gs.width; ++w) {
				for (int h = 0; h < gs.height; ++h) {
					int spriteIdx = gs.getIndex(w, h, l, pattern_x, pattern_y, pattern_z, frame);
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
	}

	// Generates RGBA pixel data for a given item ID.
	// Returns a texture ID created in the given NanoVG context.
	// Returns 0 if generation fails.
	inline int CreateItemTexture(NVGcontext* vg, uint16_t id) {
		const auto it = g_item_definitions.get(id);
		GameSprite* gs = it ? dynamic_cast<GameSprite*>(g_gui.gfx.getSprite(it.clientId())) : nullptr;
		if (!gs) {
			return 0;
		}

		int w, h;
		auto composite = CreateCompositeRGBA(*gs, w, h);
		if (!composite) {
			return 0;
		}

		return nvgCreateImageRGBA(vg, w, h, 0, composite.get());
	}

} // namespace NvgUtils

#endif
