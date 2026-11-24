//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_CHUNK_TILE_MASK_H_
#define RME_RENDERING_CORE_CHUNK_TILE_MASK_H_

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

// One bit per tile of a 16x16 render chunk. Built while the chunk is baked, so a per-frame
// pass can visit only the tiles it needs and skip an empty chunk with one test.
struct ChunkTileMask {
    static constexpr int kSide = 16;

    std::array<uint64_t, 4> words = {0, 0, 0, 0};

    [[nodiscard]] static constexpr bool inBounds(int tx, int ty) noexcept
    {
        return static_cast<unsigned>(tx) < static_cast<unsigned>(kSide) && static_cast<unsigned>(ty) < static_cast<unsigned>(kSide);
    }

    [[nodiscard]] bool any() const noexcept
    {
        return (words[0] | words[1] | words[2] | words[3]) != 0;
    }

    void clear() noexcept
    {
        words.fill(0);
    }

    void mark(int tx, int ty) noexcept
    {
        if (!inBounds(tx, ty)) {
            return;
        }
        const int idx = ty * kSide + tx;
        words[idx >> 6] |= (uint64_t {1} << (idx & 63));
    }

    [[nodiscard]] bool test(int tx, int ty) const noexcept
    {
        if (!inBounds(tx, ty)) {
            return false;
        }
        const int idx = ty * kSide + tx;
        return ((words[idx >> 6] >> (idx & 63)) & uint64_t {1}) != 0;
    }

    // Calls fn(tx, ty) for every marked tile, in row-major order.
    template <typename Fn>
    void forEach(Fn&& fn) const
    {
        for (size_t w = 0; w < words.size(); ++w) {
            uint64_t word = words[w];
            while (word != 0) {
                const int idx = static_cast<int>(w * 64) + std::countr_zero(word);
                fn(idx % kSide, idx / kSide);
                word &= word - 1;
            }
        }
    }
};

#endif
