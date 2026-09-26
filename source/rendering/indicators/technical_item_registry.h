#ifndef RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
#define RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_

#include <cstdint>

namespace rme::rendering {

/**
 * @brief Enum of all shader-rendered tile indicators.
 */
enum class TileIndicatorType : uint8_t {
	None = 0,
	HouseEntry = 1,          // "ENTRY" - Blue frame + wash
	Spawn = 2,               // "SPAWN" - Purple frame + wash
	TownTemple = 3,          // "TOWN"  - Gold frame + wash
	Waypoint = 4,            // "WAYPT" - Cyan frame + wash
	TechInvisibleStair = 5,  // "STAIR" - Yellow frame + wash
	TechInvisibleWalkable = 6,// "WALK"  - Cyan frame + wash
	TechInvisibleWall = 7,   // "BLOCK" - Red frame + wash
	TechPrimalLight = 8      // "LIGHT" - Sky blue frame + wash
};

// Base float IDs encoded in vertex attribute `house_id` / `marker_id`
inline constexpr float INDICATOR_HOUSE_ENTRY_BASE = 1000000.0f;
inline constexpr float INDICATOR_SPAWN_BASE       = 2000000.0f;
inline constexpr float INDICATOR_TOWN_BASE        = 3000000.0f;
inline constexpr float INDICATOR_WAYPOINT_BASE    = 4000000.0f;
inline constexpr float INDICATOR_TECH_STAIR_BASE  = 5000000.0f;
inline constexpr float INDICATOR_TECH_WALK_BASE   = 6000000.0f;
inline constexpr float INDICATOR_TECH_BLOCK_BASE  = 7000000.0f;
inline constexpr float INDICATOR_TECH_LIGHT_BASE  = 8000000.0f;

/**
 * @brief Centralized registry of technical and utility items.
 *
 * Maps server/client item IDs to high-performance shader indicator types,
 * eliminating hardcoded magic numbers across the rendering subsystem.
 */
class TechnicalItemRegistry {
public:
	[[nodiscard]] static constexpr TileIndicatorType Classify(uint16_t server_id, uint16_t client_id) noexcept {
		// Invisible stairs (yellow: Server 459 / Client 469)
		if (server_id == 459 || client_id == 459 || server_id == 469 || client_id == 469) {
			return TileIndicatorType::TechInvisibleStair;
		}

		// Invisible walkable (cyan: Server 460 / Client 470, 17970, 20028, 34168)
		if (server_id == 460 || client_id == 460 || server_id == 470 || client_id == 470 ||
		    server_id == 17970 || client_id == 17970 ||
		    server_id == 20028 || client_id == 20028 ||
		    server_id == 34168 || client_id == 34168) {
			return TileIndicatorType::TechInvisibleWalkable;
		}

		// Invisible wall / magic blocker (red: Server 1548 / Client 2187)
		if (server_id == 1548 || client_id == 1548 || server_id == 2187 || client_id == 2187) {
			return TileIndicatorType::TechInvisibleWall;
		}

		// Primal light / light sources (sky blue: Client 39092-39100, 39236, 39367, 39368)
		if ((client_id >= 39092 && client_id <= 39100) || (server_id >= 39092 && server_id <= 39100) ||
		    client_id == 39236 || server_id == 39236 ||
		    client_id == 39367 || server_id == 39367 ||
		    client_id == 39368 || server_id == 39368) {
			return TileIndicatorType::TechPrimalLight;
		}

		return TileIndicatorType::None;
	}

	[[nodiscard]] static constexpr bool IsTechnical(uint16_t server_id, uint16_t client_id) noexcept {
		return Classify(server_id, client_id) != TileIndicatorType::None;
	}

	[[nodiscard]] static constexpr float GetMarkerId(TileIndicatorType type) noexcept {
		switch (type) {
			case TileIndicatorType::Spawn:                 return INDICATOR_SPAWN_BASE;
			case TileIndicatorType::TownTemple:            return INDICATOR_TOWN_BASE;
			case TileIndicatorType::Waypoint:              return INDICATOR_WAYPOINT_BASE;
			case TileIndicatorType::TechInvisibleStair:    return INDICATOR_TECH_STAIR_BASE;
			case TileIndicatorType::TechInvisibleWalkable: return INDICATOR_TECH_WALK_BASE;
			case TileIndicatorType::TechInvisibleWall:     return INDICATOR_TECH_BLOCK_BASE;
			case TileIndicatorType::TechPrimalLight:       return INDICATOR_TECH_LIGHT_BASE;
			default: return 0.0f;
		}
	}
};

struct IndicatorBadgeStyle {
	const char* text;
	uint8_t border_r, border_g, border_b;
	uint8_t bg_r, bg_g, bg_b, bg_a;
	uint8_t outline_r, outline_g, outline_b;
};

inline constexpr IndicatorBadgeStyle GetIndicatorBadgeStyle(TileIndicatorType type) noexcept {
	switch (type) {
		case TileIndicatorType::HouseEntry:
			return { "ENTRY", 38, 128, 255, 20, 80, 200, 110, 5, 25, 80 };
		case TileIndicatorType::Spawn:
			return { "SPAWN", 255, 51, 255, 210, 35, 210, 110, 60, 5, 60 };
		case TileIndicatorType::TownTemple:
			return { "TOWN", 255, 215, 0, 255, 180, 20, 110, 80, 40, 0 };
		case TileIndicatorType::Waypoint:
			return { "WAYPT", 0, 255, 255, 15, 200, 220, 110, 0, 50, 60 };
		case TileIndicatorType::TechInvisibleStair:
			return { "STAIR", 255, 240, 30, 255, 220, 30, 110, 80, 60, 0 };
		case TileIndicatorType::TechInvisibleWalkable:
			return { "WALK", 0, 240, 240, 0, 200, 210, 110, 0, 50, 60 };
		case TileIndicatorType::TechInvisibleWall:
			return { "BLOCK", 255, 40, 40, 230, 40, 40, 110, 80, 5, 5 };
		case TileIndicatorType::TechPrimalLight:
			return { "LIGHT", 90, 220, 255, 70, 180, 240, 110, 10, 40, 90 };
		default:
			return { "?", 200, 200, 200, 100, 100, 100, 100, 0, 0, 0 };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
