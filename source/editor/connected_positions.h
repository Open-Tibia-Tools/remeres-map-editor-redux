//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_CONNECTED_POSITIONS_H
#define RME_EDITOR_CONNECTED_POSITIONS_H

#include "map/position.h"
#include "map/position_rect.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_set>

// Pure position geometry used by the magic wand. No map, tile or UI access:
// callers decide membership through the predicate.
namespace ConnectedPositions {

	[[nodiscard]] constexpr uint64_t planarKey(const Position& position) noexcept {
		return (static_cast<uint64_t>(static_cast<uint32_t>(position.y)) << 32) | static_cast<uint32_t>(position.x);
	}

	// Eight-connected flood fill on the origin's floor: diagonally touching tiles count as
	// neighbours, so a checkerboard of the same ground is one connected patch. `belongs(position)`
	// decides membership; non-members stop the flood, so a void or foreign tile is a wall.
	// `keep_going(found_count)` is called after every accepted position; returning false cancels
	// and yields std::nullopt. When `bounds` is set the flood never leaves it, which keeps a wand
	// search inside the viewport.
	template <typename Belongs, typename KeepGoing>
	[[nodiscard]] std::optional<PositionVector> collect(
		const Position& origin,
		Belongs&& belongs,
		KeepGoing&& keep_going,
		const PositionRect* bounds = nullptr
	) {
		PositionVector found;
		if (!origin.isValid()) {
			return found;
		}
		if (bounds && !bounds->contains(origin)) {
			return found;
		}

		const std::array<Position, 8> steps {
			Position(1, 0, 0),
			Position(1, 1, 0),
			Position(0, 1, 0),
			Position(-1, 1, 0),
			Position(-1, 0, 0),
			Position(-1, -1, 0),
			Position(0, -1, 0),
			Position(1, -1, 0),
		};
		std::unordered_set<uint64_t> visited { planarKey(origin) };
		PositionVector pending { origin };
		while (!pending.empty()) {
			const Position position = pending.back();
			pending.pop_back();
			if (!belongs(position)) {
				continue;
			}
			found.push_back(position);
			if (!keep_going(found.size())) {
				return std::nullopt;
			}
			for (const Position& step : steps) {
				const Position next = position + step;
				if (!next.isValid() || (bounds && !bounds->contains(next))) {
					continue;
				}
				if (visited.insert(planarKey(next)).second) {
					pending.push_back(next);
				}
			}
		}
		return found;
	}

	// Positions eight-adjacent to the patch that are not part of it, each listed once.
	// All patch positions are expected on one floor.
	[[nodiscard]] inline PositionVector borderRing(std::span<const Position> patch, const PositionRect* bounds = nullptr) {
		std::unordered_set<uint64_t> in_patch;
		in_patch.reserve(patch.size());
		for (const Position& position : patch) {
			in_patch.insert(planarKey(position));
		}

		PositionVector ring;
		std::unordered_set<uint64_t> seen;
		for (const Position& position : patch) {
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					if (dx == 0 && dy == 0) {
						continue;
					}
					const Position next(position.x + dx, position.y + dy, position.z);
					if (!next.isValid() || (bounds && !bounds->contains(next))) {
						continue;
					}
					const uint64_t key = planarKey(next);
					if (!in_patch.contains(key) && seen.insert(key).second) {
						ring.push_back(next);
					}
				}
			}
		}
		return ring;
	}

} // namespace ConnectedPositions

#endif
