#ifndef RME_RENDERING_INDICATORS_ZONE_FLAGS_H_
#define RME_RENDERING_INDICATORS_ZONE_FLAGS_H_

#include <cstdint>

namespace rme::rendering {

// Bitmask constants packed into uint32_t zone_flags (Location 7)
inline constexpr uint32_t ZONE_FLAG_BLOCKING       = 1u << 0;  // Bit 0: Pathing blocking (Red wash)
inline constexpr uint32_t ZONE_FLAG_SPAWN          = 1u << 1;  // Bit 1: Spawn radius (Magenta wash)
inline constexpr uint32_t ZONE_FLAG_PZ             = 1u << 2;  // Bit 2: Protection Zone (Yellow wash)
inline constexpr uint32_t ZONE_FLAG_NOPVP          = 1u << 3;  // Bit 3: No-PvP Zone (Green wash)
inline constexpr uint32_t ZONE_FLAG_NOLOGOUT       = 1u << 4;  // Bit 4: No-Logout Zone (Orange wash)
inline constexpr uint32_t ZONE_FLAG_PVPZONE        = 1u << 5;  // Bit 5: PvP Zone (Crimson wash)

// Outer connected borders for blocking (Bits 6-9)
inline constexpr uint32_t ZONE_FLAG_BLOCK_BORDER_N = 1u << 6;  // Bit 6: North blocking border
inline constexpr uint32_t ZONE_FLAG_BLOCK_BORDER_S = 1u << 7;  // Bit 7: South blocking border
inline constexpr uint32_t ZONE_FLAG_BLOCK_BORDER_W = 1u << 8;  // Bit 8: West blocking border
inline constexpr uint32_t ZONE_FLAG_BLOCK_BORDER_E = 1u << 9;  // Bit 9: East blocking border

// Outer connected borders for special zones (Bits 10-13)
inline constexpr uint32_t ZONE_FLAG_ZONE_BORDER_N  = 1u << 10; // Bit 10: North zone border
inline constexpr uint32_t ZONE_FLAG_ZONE_BORDER_S  = 1u << 11; // Bit 11: South zone border
inline constexpr uint32_t ZONE_FLAG_ZONE_BORDER_W  = 1u << 12; // Bit 12: West zone border
inline constexpr uint32_t ZONE_FLAG_ZONE_BORDER_E  = 1u << 13; // Bit 13: East zone border

// Outer borders for spawn rectangles (Bits 14-17)
inline constexpr uint32_t ZONE_FLAG_SPAWN_BORDER_N = 1u << 14; // Bit 14: North spawn border
inline constexpr uint32_t ZONE_FLAG_SPAWN_BORDER_S = 1u << 15; // Bit 15: South spawn border
inline constexpr uint32_t ZONE_FLAG_SPAWN_BORDER_W = 1u << 16; // Bit 16: West spawn border
inline constexpr uint32_t ZONE_FLAG_SPAWN_BORDER_E = 1u << 17; // Bit 17: East spawn border

// Dedicated cluster badge indicator quad (Bit 18)
inline constexpr uint32_t ZONE_FLAG_CLUSTER_BADGE  = 1u << 18; // Bit 18: Fixed World Center cluster badge quad

// Multiplicative blend mode flag for overlay quads (Bit 19)
inline constexpr uint32_t ZONE_FLAG_MULTIPLICATIVE = 1u << 19; // Bit 19: Multiplicative blending quad
} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_FLAGS_H_
