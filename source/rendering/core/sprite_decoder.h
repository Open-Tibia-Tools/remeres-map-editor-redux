//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_SPRITE_DECODER_H_
#define RME_RENDERING_CORE_SPRITE_DECODER_H_

#include <cstdint>
#include <memory>
#include <span>

class SpriteDecoder {
public:
	[[nodiscard]] static std::unique_ptr<uint8_t[]> DecodeRle(std::span<const uint8_t> dump, bool use_alpha, int id = 0);
};

#endif
