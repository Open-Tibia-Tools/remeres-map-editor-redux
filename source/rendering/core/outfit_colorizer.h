//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_OUTFIT_COLORIZER_H_
#define RME_RENDERING_CORE_OUTFIT_COLORIZER_H_

#include <cstdint>
#include <cstddef>

class OutfitColorizer {
public:
	static void ColorizePixel(uint8_t color, uint8_t& red, uint8_t& green, uint8_t& blue);
	static void ColorizeTemplatePixels(uint8_t* dest, const uint8_t* mask, size_t pixelCount, int lookHead, int lookBody, int lookLegs, int lookFeet, bool destHasAlpha);
};

#endif
