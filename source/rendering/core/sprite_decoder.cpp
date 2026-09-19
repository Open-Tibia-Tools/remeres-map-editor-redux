//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_decoder.h"
#include "app/definitions.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

	constexpr int RGBA_COMPONENTS = 4;

	struct DecompressionContext {
		int id;
		uint8_t bpp;
		bool use_alpha;
		bool& non_zero_alpha_found;
		bool& non_black_pixel_found;
	};

	bool ProcessTransparencyRun(std::span<const uint8_t> dump, size_t& read, std::span<uint8_t> data, size_t& write, DecompressionContext ctx) {
		if (read + 1 >= dump.size()) {
			return false;
		}
		int transparent = dump[read] | dump[read + 1] << 8;

		// Integrity check for transparency run
		if (write + (transparent * RGBA_COMPONENTS) > data.size()) {
			spdlog::warn("Sprite {}: Transparency run overrun (transparent={}, write={}, max={})", ctx.id, transparent, write, data.size());
			transparent = static_cast<int>((data.size() - write) / RGBA_COMPONENTS);
		}

		read += 2;
		std::ranges::fill(data.subspan(write, transparent * RGBA_COMPONENTS), 0);
		write += transparent * RGBA_COMPONENTS;
		return true;
	}

	bool ProcessColoredRun(std::span<const uint8_t> dump, size_t& read, std::span<uint8_t> data, size_t& write, DecompressionContext ctx) {
		if (read + 1 >= dump.size()) {
			return false;
		}
		int colored = dump[read] | dump[read + 1] << 8;
		read += 2;

		// Integrity check for colored run
		if (write + (colored * RGBA_COMPONENTS) > data.size()) {
			spdlog::warn("Sprite {}: Colored run overrun (colored={}, write={}, max={})", ctx.id, colored, write, data.size());
			colored = static_cast<int>((data.size() - write) / RGBA_COMPONENTS);
		}

		// Integrity check for read buffer
		if (read + (colored * ctx.bpp) > dump.size()) {
			spdlog::warn("Sprite {}: Read buffer overrun (colored={}, bpp={}, read={}, size={})", ctx.id, colored, ctx.bpp, read, dump.size());
			return false;
		}

		for (int cnt = 0; cnt < colored; ++cnt) {
			uint8_t r = dump[read + 0];
			uint8_t g = dump[read + 1];
			uint8_t b = dump[read + 2];
			uint8_t a = ctx.use_alpha ? dump[read + 3] : 0xFF;

			data[write + 0] = r;
			data[write + 1] = g;
			data[write + 2] = b;
			data[write + 3] = a;

			if (a > 0) {
				ctx.non_zero_alpha_found = true;
			}
			if (r > 0 || g > 0 || b > 0) {
				ctx.non_black_pixel_found = true;
			}

			write += RGBA_COMPONENTS;
			read += ctx.bpp;
		}
		return true;
	}

} // namespace

std::unique_ptr<uint8_t[]> SpriteDecoder::DecodeRle(std::span<const uint8_t> dump, bool use_alpha, int id) {
	const int pixels_data_size = SPRITE_PIXELS_SIZE * RGBA_COMPONENTS;
	auto data_buffer = std::make_unique<uint8_t[]>(pixels_data_size);

	std::span<uint8_t> data(data_buffer.get(), pixels_data_size);

	uint8_t bpp = use_alpha ? 4 : 3;
	size_t write = 0;
	size_t read = 0;
	bool non_zero_alpha_found = false;
	bool non_black_pixel_found = false;

	DecompressionContext ctx {
		.id = id,
		.bpp = bpp,
		.use_alpha = use_alpha,
		.non_zero_alpha_found = non_zero_alpha_found,
		.non_black_pixel_found = non_black_pixel_found
	};

	// decompress pixels
	while (read < dump.size() && write < data.size()) {
		if (!ProcessTransparencyRun(dump, read, data, write, ctx)) {
			break;
		}

		if (read >= dump.size() || write >= data.size()) {
			break;
		}

		if (!ProcessColoredRun(dump, read, data, write, ctx)) {
			break;
		}
	}

	// fill remaining pixels
	while (write < data.size()) {
		data[write + 0] = 0x00; // red
		data[write + 1] = 0x00; // green
		data[write + 2] = 0x00; // blue
		data[write + 3] = 0x00; // alpha
		write += RGBA_COMPONENTS;
	}

	// Diagnostic logging
	if (!non_zero_alpha_found && id > 100) {
		static int empty_log_count = 0;
		if (empty_log_count++ < 10) {
			spdlog::info("Sprite {}: Decoded fully transparent sprite. bpp used: {}, dump size: {}", id, bpp, dump.size());
		}
	} else if (!non_black_pixel_found && non_zero_alpha_found && id > 100) {
		static int black_log_count = 0;
		if (black_log_count++ < 10) {
			spdlog::warn("Sprite {}: Decoded PURE BLACK sprite (Alpha > 0, RGB = 0). bpp used: {}, dump size: {}. Check hasTransparency() config!", id, bpp, dump.size());
		}
	}

	return data_buffer;
}
