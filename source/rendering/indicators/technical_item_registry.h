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
	[[nodiscard]] static TechnicalItemLists CreateDefaultTechnicalLists();
	static void SetLists(TechnicalItemLists lists);
	[[nodiscard]] static const TechnicalItemLists& GetLists() noexcept;

	[[nodiscard]] static TileIndicatorType Classify(uint32_t server_id, uint32_t client_id) noexcept;

	[[nodiscard]] static bool IsTechnical(uint32_t server_id, uint32_t client_id) noexcept {
		return Classify(server_id, client_id) != TileIndicatorType::None;
	}

	[[nodiscard]] static bool IsInvisibleWall(uint32_t server_id, uint32_t client_id) noexcept {
		return Classify(server_id, client_id) == TileIndicatorType::TechInvisibleWall;
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
			return { "entry", "e", 0, 255, 0, 0, 255, 0, 128, 166, 255, 166, 0, 77, 0 };
		case TileIndicatorType::Spawn:
			return { "spawn", "s", 255, 0, 255, 255, 0, 255, 128, 255, 166, 255, 77, 0, 77 };
		case TileIndicatorType::TownTemple:
			return { "town", "t", 255, 217, 0, 255, 217, 0, 128, 255, 242, 166, 77, 65, 0 };
		case TileIndicatorType::Waypoint:
			return { "waypt", "w", 0, 166, 255, 0, 166, 255, 128, 166, 224, 255, 0, 50, 77 };
		case TileIndicatorType::TechInvisibleStair:
			return { "stair", "s", 255, 255, 0, 255, 255, 0, 128, 255, 255, 166, 77, 77, 0 };
		case TileIndicatorType::TechInvisibleWalkable:
			return { "walk", "w", 0, 255, 255, 0, 255, 255, 128, 166, 255, 255, 0, 77, 77 };
		case TileIndicatorType::TechInvisibleWall:
			return { "block", "b", 255, 0, 0, 255, 0, 0, 128, 255, 166, 166, 77, 0, 0 };
		case TileIndicatorType::TechPrimalLight:
			return { "light", "l", 0, 217, 255, 0, 217, 255, 128, 166, 242, 255, 0, 65, 77 };
		case TileIndicatorType::InvalidGround:
			return { "invalid", "inv", 255, 0, 0, 255, 0, 0, 128, 255, 166, 166, 77, 0, 0 };
		case TileIndicatorType::InvalidItem:
			return { "invalid", "inv", 255, 128, 0, 255, 128, 0, 128, 255, 217, 166, 77, 38, 0 };
		case TileIndicatorType::InvalidZone:
			return { "invalid", "inv", 255, 0, 255, 255, 0, 255, 128, 255, 166, 255, 77, 0, 77 };
		case TileIndicatorType::ZonePZ:
			return { "pz", "pz", 20, 117, 255, 20, 117, 255, 128, 173, 212, 255, 6, 35, 77 };
		case TileIndicatorType::ZoneNoPvP:
			return { "nopvp", "np", 0, 219, 92, 0, 219, 92, 128, 166, 255, 206, 0, 66, 28 };
		case TileIndicatorType::ZoneNoLogout:
			return { "nolog", "nl", 255, 122, 0, 255, 122, 0, 128, 255, 215, 166, 77, 37, 0 };
		case TileIndicatorType::ZonePvP:
			return { "pvp", "pvp", 245, 26, 51, 245, 26, 51, 128, 255, 174, 183, 74, 8, 15 };
		case TileIndicatorType::House:
			return { "house", "h", 89, 191, 13, 89, 191, 13, 128, 199, 240, 169, 27, 57, 4 };
		default:
			return { "?", "?", 200, 200, 200, 100, 100, 100, 128, 240, 240, 240, 50, 50, 50 };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_TECHNICAL_ITEM_REGISTRY_H_
