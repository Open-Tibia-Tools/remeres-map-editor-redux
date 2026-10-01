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
	InvalidZone = 11,        // Flat magenta wash (Invalid zone flags)
	ZonePZ = 12,             // "PZ"    - Protection Zone (Gold/Yellow)
	ZoneNoPvP = 13,          // "NOPVP" - No-PvP Zone (Green)
	ZoneNoLogout = 14,       // "NOLOG" - No-Logout Zone (Orange)
	ZonePvP = 15,            // "PVP"   - PvP Zone (Red)
	House = 16               // "HOUSE" - House Tile (Amber)
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
	uint8_t light_r, light_g, light_b; // 3D highlight bevel
	uint8_t dark_r, dark_g, dark_b;   // 3D shadow bevel
};

inline constexpr IndicatorBadgeStyle GetIndicatorBadgeStyle(TileIndicatorType type) noexcept {
	switch (type) {
		case TileIndicatorType::HouseEntry:
			return { "ENTRY", "E", 180, 235, 31, 180, 235, 31, 71, 227, 255, 143, 79, 107, 0 };
		case TileIndicatorType::Spawn:
			return { "SPAWN", "S", 242, 38, 242, 242, 38, 242, 71, 255, 166, 255, 122, 0, 122 };
		case TileIndicatorType::TownTemple:
			return { "TOWN", "T", 255, 217, 0, 255, 217, 0, 71, 255, 245, 153, 140, 90, 0 };
		case TileIndicatorType::Waypoint:
			return { "WAYPT", "W", 0, 229, 255, 0, 229, 255, 71, 178, 246, 255, 0, 107, 128 };
		case TileIndicatorType::TechInvisibleStair:
			return { "STAIR", "S", 232, 217, 107, 232, 217, 107, 71, 247, 240, 184, 122, 110, 31 };
		case TileIndicatorType::TechInvisibleWalkable:
			return { "WALK", "W", 92, 184, 196, 92, 184, 196, 71, 194, 230, 236, 31, 90, 102 };
		case TileIndicatorType::TechInvisibleWall:
			return { "BLOCK", "B", 192, 80, 77, 192, 80, 77, 71, 232, 176, 174, 94, 31, 29 };
		case TileIndicatorType::TechPrimalLight:
			return { "LIGHT", "L", 191, 233, 255, 191, 233, 255, 71, 240, 250, 255, 79, 143, 176 };
		case TileIndicatorType::InvalidGround:
			return { "INVALID", "INV", 235, 31, 61, 235, 31, 61, 71, 255, 153, 173, 107, 10, 40 };
		case TileIndicatorType::InvalidItem:
			return { "INVALID", "INV", 255, 133, 15, 255, 133, 15, 71, 255, 199, 128, 138, 46, 16 };
		case TileIndicatorType::InvalidZone:
			return { "INVALID", "INV", 242, 38, 242, 242, 38, 242, 71, 255, 166, 255, 122, 0, 122 };
		case TileIndicatorType::ZonePZ:
			return { "PZ", "PZ", 47, 139, 255, 47, 139, 255, 71, 166, 210, 255, 16, 42, 140 };
		case TileIndicatorType::ZoneNoPvP:
			return { "NOPVP", "NP", 31, 217, 122, 31, 217, 122, 66, 184, 255, 217, 0, 97, 72 };
		case TileIndicatorType::ZoneNoLogout:
			return { "NOLOG", "NL", 255, 133, 15, 255, 133, 15, 71, 255, 199, 128, 138, 46, 16 };
		case TileIndicatorType::ZonePvP:
			return { "PVP", "PVP", 235, 31, 61, 235, 31, 61, 71, 255, 153, 173, 107, 10, 40 };
		case TileIndicatorType::House:
			return { "HOUSE", "H", 143, 127, 196, 143, 127, 196, 71, 210, 201, 240, 63, 52, 112 };
		default:
			return { "?", "?", 200, 200, 200, 100, 100, 100, 100, 240, 240, 240, 50, 50, 50 };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
