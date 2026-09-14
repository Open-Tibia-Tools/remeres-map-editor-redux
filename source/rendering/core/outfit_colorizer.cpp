//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/outfit_colorizer.h"
#include "rendering/core/outfit_colors.h"
#include <ranges>
#include <span>

void OutfitColorizer::ColorizePixel(uint8_t color, uint8_t& red, uint8_t& green, uint8_t& blue) {
	// Thanks! Khaos, or was it mips? Hmmm... =)
	uint8_t ro = (TemplateOutfitLookupTable[color] & 0xFF0000) >> 16; // rgb outfit
	uint8_t go = (TemplateOutfitLookupTable[color] & 0xFF00) >> 8;
	uint8_t bo = (TemplateOutfitLookupTable[color] & 0xFF);
	red = (uint8_t)(red * (ro / 255.f));
	green = (uint8_t)(green * (go / 255.f));
	blue = (uint8_t)(blue * (bo / 255.f));
}

void OutfitColorizer::ColorizeTemplatePixels(uint8_t* dest, const uint8_t* mask, size_t pixelCount, int lookHead, int lookBody, int lookLegs, int lookFeet, bool destHasAlpha) {
	constexpr int RGB_COMPONENTS = 3;
	constexpr int RGBA_COMPONENTS = 4;
	const int dest_step = destHasAlpha ? RGBA_COMPONENTS : RGB_COMPONENTS;
	const int mask_step = RGB_COMPONENTS;

	std::span<uint8_t> destSpan(dest, pixelCount * dest_step);
	std::span<const uint8_t> maskSpan(mask, pixelCount * mask_step);

	for (size_t i : std::views::iota(0u, pixelCount)) {
		uint8_t& red = destSpan[i * dest_step + 0];
		uint8_t& green = destSpan[i * dest_step + 1];
		uint8_t& blue = destSpan[i * dest_step + 2];

		const uint8_t& tred = maskSpan[i * mask_step + 0];
		const uint8_t& tgreen = maskSpan[i * mask_step + 1];
		const uint8_t& tblue = maskSpan[i * mask_step + 2];

		if (tred && tgreen && !tblue) { // yellow => head
			OutfitColorizer::ColorizePixel(lookHead, red, green, blue);
		} else if (tred && !tgreen && !tblue) { // red => body
			OutfitColorizer::ColorizePixel(lookBody, red, green, blue);
		} else if (!tred && tgreen && !tblue) { // green => legs
			OutfitColorizer::ColorizePixel(lookLegs, red, green, blue);
		} else if (!tred && !tgreen && tblue) { // blue => feet
			OutfitColorizer::ColorizePixel(lookFeet, red, green, blue);
		}
	}
}
