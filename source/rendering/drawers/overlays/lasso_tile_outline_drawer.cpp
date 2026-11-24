#include "rendering/drawers/overlays/lasso_tile_outline_drawer.h"

#include <nanovg.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

	constexpr float kShadeAlpha = 0.2f;
	constexpr float kOutlineAlpha = 0.95f;

	[[nodiscard]] uint64_t PackPoint(int x, int y) noexcept {
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
	}

	// Start and end grid points of one segment, honouring the traversal direction.
	[[nodiscard]] std::pair<LassoPoint, LassoPoint> SegmentEndpoints(const LassoTileSegment& seg) {
		if (seg.horizontal == 0) {
			// Vertical segment at x: forward is down (y → y+1), reversed is up (y+1 → y).
			if (seg.reversed != 0) {
				return { { double(seg.x), double(seg.y) + 1.0 }, { double(seg.x), double(seg.y) } };
			}
			return { { double(seg.x), double(seg.y) }, { double(seg.x), double(seg.y) + 1.0 } };
		}
		// Horizontal segment at y: forward is right (x → x+1), reversed is left (x+1 → x).
		if (seg.reversed != 0) {
			return { { double(seg.x) + 1.0, double(seg.y) }, { double(seg.x), double(seg.y) } };
		}
		return { { double(seg.x), double(seg.y) }, { double(seg.x) + 1.0, double(seg.y) } };
	}

	// Chains boundary segments into closed loops by following the traversal direction.
	// O(perimeter): each segment is visited once.
	[[nodiscard]] std::vector<std::vector<LassoPoint>> ChainSegments(const std::span<const LassoTileSegment> segments) {
		std::vector<bool> visited(segments.size(), false);
		// Map from grid point → segment indices that START at that point.
		std::unordered_map<uint64_t, std::vector<std::size_t>> starting_at;
		for (std::size_t i = 0; i < segments.size(); ++i) {
			const auto [start, end] = SegmentEndpoints(segments[i]);
			starting_at[PackPoint(static_cast<int>(start.x), static_cast<int>(start.y))].push_back(i);
		}

		std::vector<std::vector<LassoPoint>> loops;
		for (std::size_t seed = 0; seed < segments.size(); ++seed) {
			if (visited[seed]) {
				continue;
			}
			std::vector<LassoPoint> loop;
			std::size_t current = seed;
			while (!visited[current]) {
				visited[current] = true;
				const auto [start, end] = SegmentEndpoints(segments[current]);
				loop.push_back(start);
				const uint64_t end_key = PackPoint(static_cast<int>(end.x), static_cast<int>(end.y));
				auto it = starting_at.find(end_key);
				if (it == starting_at.end()) {
					break;
				}
				bool found_next = false;
				for (const std::size_t next : it->second) {
					if (!visited[next]) {
						current = next;
						found_next = true;
						break;
					}
				}
				if (!found_next) {
					break; // loop closed
				}
			}
			if (loop.size() >= 3) {
				loops.push_back(std::move(loop));
			}
		}
		return loops;
	}

} // namespace

void LassoTileOutlineDrawer::draw(NVGcontext* vg, std::span<const LassoTileSegment> segments, int floor, const MainMapViewport& viewport) const {
	if (!vg || segments.empty()) {
		return;
	}

	const float dash_length = 3.0f;
	const float gap_length = 2.0f;
	const float outline_thickness = 1.0f;
	const int floor_offset = MainMapViewMath::GetFloorTileOffset(floor);
	const double scale = viewport.zoom * viewport.scale_factor;

	const auto tile_to_screen = [&](double tx, double ty, float* out_x, float* out_y) {
		const double raw_x = (tx - floor_offset) * TILE_SIZE - viewport.view_scroll_x;
		const double raw_y = (ty - floor_offset) * TILE_SIZE - viewport.view_scroll_y;
		*out_x = static_cast<float>(raw_x / scale);
		*out_y = static_cast<float>(raw_y / scale);
	};

	// 1. Translucent white shade over the selected tile set: chain the boundary segments into
	//    closed loops along the tile grid and fill them. The shade covers exactly the tiles
	//    that will be selected, grid-aligned like box selection.
	nvgSave(vg);
	nvgBeginPath(vg);
	for (const std::vector<LassoPoint>& loop : ChainSegments(segments)) {
		for (std::size_t i = 0; i < loop.size(); ++i) {
			float sx, sy;
			tile_to_screen(loop[i].x, loop[i].y, &sx, &sy);
			if (i == 0) {
				nvgMoveTo(vg, sx, sy);
			} else {
				nvgLineTo(vg, sx, sy);
			}
		}
		nvgClosePath(vg);
	}
	nvgFillColor(vg, nvgRGBA(255, 255, 255, static_cast<unsigned char>(kShadeAlpha * 255.0f)));
	nvgFill(vg);
	nvgRestore(vg);

	// 2. Dashed white lines along the tile boundary: same on-screen pattern as box selection.
	nvgSave(vg);
	nvgBeginPath(vg);
	for (const LassoTileSegment& segment : segments) {
		double dx0, dy0, dx1, dy1;
		MainMapViewMath::MapTileToScreenDIP(segment.x, segment.y, floor, viewport, &dx0, &dy0);
		if (segment.horizontal == 0) {
			MainMapViewMath::MapTileToScreenDIP(segment.x, segment.y + 1, floor, viewport, &dx1, &dy1);
		} else {
			MainMapViewMath::MapTileToScreenDIP(segment.x + 1, segment.y, floor, viewport, &dx1, &dy1);
		}
		const float fx = static_cast<float>(dx1 - dx0);
		const float fy = static_cast<float>(dy1 - dy0);
		const float length = std::sqrt(fx * fx + fy * fy);
		if (length < 0.01f) {
			continue;
		}
		const float ux = fx / length;
		const float uy = fy / length;
		for (float offset = 0.0f; offset < length; offset += dash_length + gap_length) {
			const float extent = std::min(dash_length, length - offset);
			if (extent > 0.01f) {
				const float px = static_cast<float>(dx0) + ux * offset;
				const float py = static_cast<float>(dy0) + uy * offset;
				nvgMoveTo(vg, px, py);
				nvgLineTo(vg, px + ux * extent, py + uy * extent);
			}
		}
	}
	nvgStrokeColor(vg, nvgRGBA(255, 255, 255, static_cast<unsigned char>(kOutlineAlpha * 255.0f)));
	nvgStrokeWidth(vg, outline_thickness);
	nvgStroke(vg);
	nvgRestore(vg);
}
