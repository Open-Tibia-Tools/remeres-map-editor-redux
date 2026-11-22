#ifndef RME_MAP_MISSING_GROUND_H_
#define RME_MAP_MISSING_GROUND_H_

// "Show missing ground": a tile with items but no ground is reported only when it is a hole in a
// grounded area, so tiles left without ground on purpose (floor edges, open space) stay quiet.
namespace MissingGround {

	// Orthogonal neighbours on the same floor (north, east, south, west) that must have ground.
	inline constexpr int kMinGroundedNeighbours = 3;

	[[nodiscard]] constexpr bool IsHole(bool has_items, bool has_ground, int grounded_neighbours) noexcept {
		return has_items && !has_ground && grounded_neighbours >= kMinGroundedNeighbours;
	}

	// has_ground(x, y): whether the tile at (x, y) on the same floor has ground.
	template <typename HasGround>
	[[nodiscard]] constexpr int CountGroundedNeighbours(int x, int y, HasGround&& has_ground) {
		return static_cast<int>(has_ground(x, y - 1)) + static_cast<int>(has_ground(x + 1, y)) + static_cast<int>(has_ground(x, y + 1)) + static_cast<int>(has_ground(x - 1, y));
	}

} // namespace MissingGround

#endif
