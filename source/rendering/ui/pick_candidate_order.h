#ifndef RME_RENDERING_UI_PICK_CANDIDATE_ORDER_H_
#define RME_RENDERING_UI_PICK_CANDIDATE_ORDER_H_

// Order in which pixel-perfect picking visits candidate anchor tiles (dx, dy >= 0 south-east of the
// clicked tile). Sprites extend up-left from their anchor, so any of these tiles can cover the click.
//
// The order mirrors the render depth (rendering/core/render_depth.h): the diagonal dx + dy dominates
// every sublayer, so diagonals are visited front to back (largest first). Inside one diagonal every
// non-ground layer is in front of every ground, so callers test all non-ground layers of a diagonal
// before any of its grounds. Pure index math, no map or wx dependency.
namespace PickCandidateOrder {

	constexpr int MaxDiagonal(int range) {
		return range * 2;
	}

	// Calls visit(dx, dy) for every candidate on `diagonal` within [0, range], dy descending.
	template <typename Visitor>
	constexpr void ForEachOnDiagonal(int range, int diagonal, Visitor&& visit) {
		for (int dy = range; dy >= 0; --dy) {
			const int dx = diagonal - dy;
			if (dx < 0 || dx > range) {
				continue;
			}
			visit(dx, dy);
		}
	}

} // namespace PickCandidateOrder

#endif
