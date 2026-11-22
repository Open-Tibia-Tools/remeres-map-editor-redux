#ifndef RME_RENDERING_UI_PICK_CANDIDATE_ORDER_H_
#define RME_RENDERING_UI_PICK_CANDIDATE_ORDER_H_

// Order in which pixel-perfect picking visits candidate anchor tiles (dx, dy >= 0 south-east of the
// clicked tile). Sprites extend up-left from their anchor, so any of these tiles can cover the click.
//
// The order mirrors the render depth (rendering/core/render_depth.h): the diagonal dx + dy dominates
// every sublayer, so diagonals are visited front to back (largest first). Inside one diagonal the
// chunk-cache baker draws tiles in tx ascending / ty descending order, so the last-baked (frontmost)
// candidate is dx largest / dy smallest; ForEachOnDiagonal yields that candidate first. The dynamic
// tile renderer uses the opposite tie order at exactly equal depth; that ambiguity is documented in
// docs/rendering/multitile-sprite-geometry.md and cannot be resolved for both paths at once.
// Pure index math, no map or wx dependency.
namespace PickCandidateOrder {

	constexpr int MaxDiagonal(int range) {
		return range * 2;
	}

	// Calls visit(dx, dy) for every candidate on `diagonal` within [0, range],
	// dx descending / dy ascending (chunk-cache painter order).
	template <typename Visitor>
	constexpr void ForEachOnDiagonal(int range, int diagonal, Visitor&& visit) {
		for (int dx = range; dx >= 0; --dx) {
			const int dy = diagonal - dx;
			if (dy < 0 || dy > range) {
				continue;
			}
			visit(dx, dy);
		}
	}

} // namespace PickCandidateOrder

#endif
