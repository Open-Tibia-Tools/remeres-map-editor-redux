//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_ITEM_DEFINITIONS_FORMATS_PROTOBUF_PROTOBUF_SHIFT_BOUNDS_H_
#define RME_ITEM_DEFINITIONS_FORMATS_PROTOBUF_PROTOBUF_SHIFT_BOUNDS_H_

#include <cstdint>
#include <limits>

struct ProtobufShiftClampResult {
    uint16_t value = 0;
    bool clamped = false;
};

[[nodiscard]] constexpr ProtobufShiftClampResult ClampProtobufShiftToUint16(uint32_t value) noexcept
{
    constexpr uint32_t kMaxShift = std::numeric_limits<uint16_t>::max();
    if (value > kMaxShift) {
        return {static_cast<uint16_t>(kMaxShift), true};
    }
    return {static_cast<uint16_t>(value), false};
}

#endif
