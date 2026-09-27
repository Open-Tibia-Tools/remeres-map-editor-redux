#ifndef RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
#define RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_

#include <array>
#include <cstdint>

class Brush;

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
	TechPrimalLight = 8,     // "LIGHT" - Sky blue frame + wash
	InvalidGround = 9,       // "INVALID" - Red frame + wash (Missing ground tile)
	InvalidItem = 10,        // "INVALID" - Orange/Yellow frame + wash (Missing top item)
	InvalidZone = 11         // "INVALID" - Magenta frame + wash (Invalid zone flags)
};

// Base float IDs encoded in vertex attribute `house_id` / `marker_id`
inline constexpr float INDICATOR_HOUSE_ENTRY_BASE     = 1000000.0f;
inline constexpr float INDICATOR_SPAWN_BASE           = 2000000.0f;
inline constexpr float INDICATOR_TOWN_BASE            = 3000000.0f;
inline constexpr float INDICATOR_WAYPOINT_BASE        = 4000000.0f;
inline constexpr float INDICATOR_TECH_STAIR_BASE      = 5000000.0f;
inline constexpr float INDICATOR_TECH_WALK_BASE       = 6000000.0f;
inline constexpr float INDICATOR_TECH_BLOCK_BASE      = 7000000.0f;
inline constexpr float INDICATOR_TECH_LIGHT_BASE      = 8000000.0f;
inline constexpr float INDICATOR_INVALID_GROUND_BASE  = 9000000.0f;
inline constexpr float INDICATOR_INVALID_ITEM_BASE    = 10000000.0f;
inline constexpr float INDICATOR_INVALID_ZONE_BASE    = 11000000.0f;

/**
 * @brief Known item IDs for technical and utility items across client versions.
 */
namespace ProtocolItems {
	inline constexpr std::array<uint16_t, 2> INVISIBLE_STAIRS  = { 459, 469 };
	inline constexpr std::array<uint16_t, 5> INVISIBLE_WALKABLE = { 460, 470, 17970, 20028, 34168 };
	inline constexpr std::array<uint16_t, 2> INVISIBLE_WALLS    = { 1548, 2187 };

	inline constexpr uint16_t PRIMAL_LIGHT_MIN = 39092;
	inline constexpr uint16_t PRIMAL_LIGHT_MAX = 39100;
	inline constexpr std::array<uint16_t, 3> PRIMAL_LIGHT_EXTRA = { 39236, 39367, 39368 };
} // namespace ProtocolItems

/**
 * @brief Centralized registry of technical and utility items.
 *
 * Maps server/client item IDs to high-performance shader indicator types,
 * eliminating hardcoded magic numbers across the rendering subsystem.
 */
class TechnicalItemRegistry {
public:
	template <size_t N>
	[[nodiscard]] static constexpr bool MatchesAny(uint16_t server_id, uint16_t client_id, const std::array<uint16_t, N>& list) noexcept {
		for (uint16_t id : list) {
			if (server_id == id || client_id == id) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] static constexpr TileIndicatorType Classify(uint16_t server_id, uint16_t client_id) noexcept {
		// Invisible stairs (yellow: Server 459 / Client 469)
		if (MatchesAny(server_id, client_id, ProtocolItems::INVISIBLE_STAIRS)) {
			return TileIndicatorType::TechInvisibleStair;
		}

		// Invisible walkable (cyan: Server 460 / Client 470, 17970, 20028, 34168)
		if (MatchesAny(server_id, client_id, ProtocolItems::INVISIBLE_WALKABLE)) {
			return TileIndicatorType::TechInvisibleWalkable;
		}

		// Invisible wall / magic blocker (red: Server 1548 / Client 2187)
		if (MatchesAny(server_id, client_id, ProtocolItems::INVISIBLE_WALLS)) {
			return TileIndicatorType::TechInvisibleWall;
		}

		// Primal light / light sources (sky blue: Client 39092-39100, 39236, 39367, 39368)
		if ((client_id >= ProtocolItems::PRIMAL_LIGHT_MIN && client_id <= ProtocolItems::PRIMAL_LIGHT_MAX) ||
		    (server_id >= ProtocolItems::PRIMAL_LIGHT_MIN && server_id <= ProtocolItems::PRIMAL_LIGHT_MAX) ||
		    MatchesAny(server_id, client_id, ProtocolItems::PRIMAL_LIGHT_EXTRA)) {
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
			case TileIndicatorType::InvalidGround:         return INDICATOR_INVALID_GROUND_BASE;
			case TileIndicatorType::InvalidItem:           return INDICATOR_INVALID_ITEM_BASE;
			case TileIndicatorType::InvalidZone:           return INDICATOR_INVALID_ZONE_BASE;
			default: return 0.0f;
		}
	}

	/**
	 * @brief Maps a palette brush to its corresponding tile indicator type.
	 */
	[[nodiscard]] static TileIndicatorType GetBrushIndicatorType(const Brush* brush);
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
		case TileIndicatorType::InvalidGround:
			return { "", 255, 0, 0, 255, 0, 0, 171, 0, 0, 0 };
		case TileIndicatorType::InvalidItem:
			return { "", 255, 165, 0, 255, 165, 0, 171, 0, 0, 0 };
		case TileIndicatorType::InvalidZone:
			return { "", 255, 0, 255, 255, 0, 255, 171, 0, 0, 0 };
		default:
			return { "?", 200, 200, 200, 100, 100, 100, 100, 0, 0, 0 };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
