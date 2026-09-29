#ifndef RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
#define RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_

#include <cstdint>
#include <vector>

namespace toml {
	inline namespace v3 {
		class table;
	}
	using table = v3::table;
}

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
	InvalidGround = 9,       // Flat red wash (Missing ground tile)
	InvalidItem = 10,        // Flat orange wash (Missing top item)
	InvalidZone = 11         // Flat magenta wash (Invalid zone flags)
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
 * @brief Separate Server and Client ID filters for technical items.
 */
struct TechnicalIdFilter {
	std::vector<uint32_t> server_ids;
	std::vector<uint32_t> client_ids;

	[[nodiscard]] bool matches(uint32_t sid, uint32_t cid) const noexcept;
	void sort_and_dedup();
};

/**
 * @brief Runtime lists of technical item IDs populated dynamically from config.toml.
 */
struct TechnicalItemLists {
	TechnicalIdFilter invisible_stairs;
	TechnicalIdFilter invisible_walkable;
	TechnicalIdFilter invisible_walls;
	TechnicalIdFilter primal_lights;
};

/**
 * @brief Centralized registry of technical and utility items.
 *
 * Maps server/client item IDs to high-performance shader indicator types.
 * All item IDs are loaded dynamically from config.toml with zero hardcoded IDs in C++.
 */
class TechnicalItemRegistry {
public:
	static void Initialize(const toml::table& config_table);
	static void SetLists(TechnicalItemLists lists);
	[[nodiscard]] static const TechnicalItemLists& GetLists() noexcept;

	[[nodiscard]] static TileIndicatorType Classify(uint32_t server_id, uint32_t client_id) noexcept;

	[[nodiscard]] static bool IsTechnical(uint32_t server_id, uint32_t client_id) noexcept {
		return Classify(server_id, client_id) != TileIndicatorType::None;
	}

	[[nodiscard]] static constexpr float GetMarkerId(TileIndicatorType type) noexcept {
		switch (type) {
			case TileIndicatorType::HouseEntry:            return INDICATOR_HOUSE_ENTRY_BASE;
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
	const char* short_text;
	uint8_t border_r, border_g, border_b;
	uint8_t bg_r, bg_g, bg_b, bg_a;
	uint8_t outline_r, outline_g, outline_b;
};

inline constexpr IndicatorBadgeStyle GetIndicatorBadgeStyle(TileIndicatorType type) noexcept {
	switch (type) {
		case TileIndicatorType::HouseEntry:
			return { "ENTRY", "E", 38, 128, 255, 20, 80, 200, 110, 5, 25, 80 };
		case TileIndicatorType::Spawn:
			return { "SPAWN", "S", 255, 51, 255, 210, 35, 210, 110, 60, 5, 60 };
		case TileIndicatorType::TownTemple:
			return { "TOWN", "T", 255, 215, 0, 255, 180, 20, 110, 80, 40, 0 };
		case TileIndicatorType::Waypoint:
			return { "WAYPT", "W", 0, 255, 255, 15, 200, 220, 110, 0, 50, 60 };
		case TileIndicatorType::TechInvisibleStair:
			return { "STAIR", "S", 255, 240, 30, 255, 220, 30, 110, 80, 60, 0 };
		case TileIndicatorType::TechInvisibleWalkable:
			return { "WALK", "W", 0, 240, 240, 0, 200, 210, 110, 0, 50, 60 };
		case TileIndicatorType::TechInvisibleWall:
			return { "BLOCK", "B", 255, 40, 40, 230, 40, 40, 110, 80, 5, 5 };
		case TileIndicatorType::TechPrimalLight:
			return { "LIGHT", "L", 90, 220, 255, 70, 180, 240, 110, 10, 40, 90 };
		case TileIndicatorType::InvalidGround:
			return { "", "", 255, 0, 0, 255, 0, 0, 171, 0, 0, 0 };
		case TileIndicatorType::InvalidItem:
			return { "", "", 255, 165, 0, 255, 165, 0, 171, 0, 0, 0 };
		case TileIndicatorType::InvalidZone:
			return { "", "", 255, 0, 255, 255, 0, 255, 171, 0, 0, 0 };
		default:
			return { "?", "?", 200, 200, 200, 100, 100, 100, 100, 0, 0, 0 };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
