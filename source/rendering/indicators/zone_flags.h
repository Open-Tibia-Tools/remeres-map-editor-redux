#ifndef RME_RENDERING_INDICATORS_ZONE_FLAGS_H_
#define RME_RENDERING_INDICATORS_ZONE_FLAGS_H_

namespace rme::rendering {

// Bitmask constants packed into float zone_flags (Location 7)
// Up to 24 bits representable with bit-exact precision in IEEE-754 float
inline constexpr float ZONE_FLAG_BLOCKING       = 1.0f;     // Bit 0: Pathing blocking (Red wash)
inline constexpr float ZONE_FLAG_SPAWN          = 2.0f;     // Bit 1: Spawn radius (Magenta wash)
inline constexpr float ZONE_FLAG_PZ             = 4.0f;     // Bit 2: Protection Zone (Green wash)
inline constexpr float ZONE_FLAG_NOPVP          = 8.0f;     // Bit 3: No-PvP Zone (Yellow wash)
inline constexpr float ZONE_FLAG_NOLOGOUT       = 16.0f;    // Bit 4: No-Logout Zone (Orange wash)
inline constexpr float ZONE_FLAG_PVPZONE        = 32.0f;    // Bit 5: PvP Zone (Crimson wash)
inline constexpr float ZONE_FLAG_REFRESH        = 262144.0f;// Bit 18: Refresh Zone (Chartreuse wash)

// Outer connected borders for blocking (Bits 6-9)
inline constexpr float ZONE_FLAG_BLOCK_BORDER_N = 64.0f;    // Bit 6: North blocking border
inline constexpr float ZONE_FLAG_BLOCK_BORDER_S = 128.0f;   // Bit 7: South blocking border
inline constexpr float ZONE_FLAG_BLOCK_BORDER_W = 256.0f;   // Bit 8: West blocking border
inline constexpr float ZONE_FLAG_BLOCK_BORDER_E = 512.0f;   // Bit 9: East blocking border

// Outer connected borders for special zones (Bits 10-13)
inline constexpr float ZONE_FLAG_ZONE_BORDER_N  = 1024.0f;  // Bit 10: North zone border
inline constexpr float ZONE_FLAG_ZONE_BORDER_S  = 2048.0f;  // Bit 11: South zone border
inline constexpr float ZONE_FLAG_ZONE_BORDER_W  = 4096.0f;  // Bit 12: West zone border
inline constexpr float ZONE_FLAG_ZONE_BORDER_E  = 8192.0f;  // Bit 13: East zone border

// Outer borders for spawn rectangles (Bits 14-17)
inline constexpr float ZONE_FLAG_SPAWN_BORDER_N = 16384.0f; // Bit 14: North spawn border
inline constexpr float ZONE_FLAG_SPAWN_BORDER_S = 32768.0f; // Bit 15: South spawn border
inline constexpr float ZONE_FLAG_SPAWN_BORDER_W = 65536.0f; // Bit 16: West spawn border
inline constexpr float ZONE_FLAG_SPAWN_BORDER_E = 131072.0f;// Bit 17: East spawn border
} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_FLAGS_H_
