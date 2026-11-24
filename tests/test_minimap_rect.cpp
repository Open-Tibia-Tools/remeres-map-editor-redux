#include "rendering/drawers/minimap_buffer.h"
#include "rendering/drawers/minimap_rect.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

// MinimapCache uploads a zero-filled buffer for a page-local dirty rect. These cases pin the two
// properties the old inclusive-end code broke: the rect is converted to an EXCLUSIVE end for
// Map::visitLeaves(), and every tile of the rect is written with its own minimap color. The buffer
// starts at a sentinel value so "not written" is observable and cannot hide behind a zero.

namespace {

	constexpr uint8_t kUnwritten = 0xFE;

	// Deterministic, non-zero and distinct enough to expose a write to the wrong buffer slot.
	// Stays below kUnwritten so a written tile can never be mistaken for an untouched one.
	uint8_t TestColor(int map_x, int map_y) {
		return static_cast<uint8_t>(1 + ((map_x * 7 + map_y * 13) % 200));
	}

	struct FakeNode {
		int map_x = 0;
		int map_y = 0;
	};

	// Minimal Map stand-in. visitLeaves() mirrors SpatialHashGrid::visitLeaves()
	// (source/map/spatial_hash_grid.h): the end is exclusive and nodes are 4x4 tiles.
	class FakeMap {
	public:
		void AddNode(int node_map_x, int node_map_y) {
			const std::pair<int, int> key(node_map_x / kMinimapNodeTiles, node_map_y / kMinimapNodeTiles);
			nodes_.emplace(key, FakeNode { .map_x = node_map_x, .map_y = node_map_y });
		}

		[[nodiscard]] bool HasNodeFor(int map_x, int map_y) const {
			return nodes_.contains(std::pair(map_x / kMinimapNodeTiles, map_y / kMinimapNodeTiles));
		}

		template <typename Visitor>
		void visitLeaves(int min_x, int min_y, int end_x, int end_y, Visitor&& visit) const {
			if (end_x <= min_x || end_y <= min_y) {
				return;
			}
			for (int node_x = min_x / kMinimapNodeTiles; node_x <= (end_x - 1) / kMinimapNodeTiles; ++node_x) {
				for (int node_y = min_y / kMinimapNodeTiles; node_y <= (end_y - 1) / kMinimapNodeTiles; ++node_y) {
					auto it = nodes_.find(std::pair(node_x, node_y));
					if (it != nodes_.end()) {
						visit(&it->second, it->second.map_x, it->second.map_y);
					}
				}
			}
		}

	private:
		std::map<std::pair<int, int>, FakeNode> nodes_;
	};

	std::vector<uint8_t> FillRect(const FakeMap& map, const MinimapDirtyRect& rect, int page_x, int page_y) {
		std::vector<uint8_t> buffer(static_cast<size_t>(rect.width) * rect.height, kUnwritten);
		const MinimapMapBounds bounds = ToMinimapMapBounds(rect, page_x, page_y);
		FillMinimapColorBuffer(map, bounds, buffer, [](const FakeNode* node, int local_x, int local_y) {
			return TestColor(node->map_x + local_x, node->map_y + local_y);
		});
		return buffer;
	}

	void RequireRectFilled(const FakeMap& map, const std::vector<uint8_t>& buffer, const MinimapDirtyRect& rect, int page_x, int page_y) {
		REQUIRE(buffer.size() == static_cast<size_t>(rect.width) * rect.height);
		for (int y = 0; y < rect.height; ++y) {
			for (int x = 0; x < rect.width; ++x) {
				const int map_x = page_x * kMinimapPageSize + rect.x + x;
				const int map_y = page_y * kMinimapPageSize + rect.y + y;
				const uint8_t actual = buffer[static_cast<size_t>(y) * rect.width + x];
				if (map.HasNodeFor(map_x, map_y)) {
					REQUIRE(actual == TestColor(map_x, map_y));
				} else {
					REQUIRE(actual == kUnwritten);
				}
			}
		}
	}

} // namespace

TEST_CASE("dirty rect bounds use an exclusive end", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 10, .y = 20, .width = 1, .height = 1 }, 0, 0);
	REQUIRE(bounds.min_x == 10);
	REQUIRE(bounds.min_y == 20);
	REQUIRE(bounds.end_x == 11);
	REQUIRE(bounds.end_y == 21);
}

TEST_CASE("dirty rect bounds apply the page offset to both ends", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 511, .y = 3, .width = 1, .height = 2 }, 2, 5);
	REQUIRE(bounds.min_x == 2 * kMinimapPageSize + 511);
	REQUIRE(bounds.min_y == 5 * kMinimapPageSize + 3);
	REQUIRE(bounds.end_x == bounds.min_x + 1);
	REQUIRE(bounds.end_y == bounds.min_y + 2);
}

TEST_CASE("a 1x1 dirty rect is written instead of skipped", "[minimap]") {
	FakeMap map;
	map.AddNode(8, 20);

	const MinimapDirtyRect rect { .x = 10, .y = 20, .width = 1, .height = 1 };
	const std::vector<uint8_t> buffer = FillRect(map, rect, 0, 0);

	REQUIRE(buffer.size() == 1);
	RequireRectFilled(map, buffer, rect, 0, 0);
}

TEST_CASE("dirty rects on node edges include the node they touch", "[minimap]") {
	FakeMap map;
	for (int node_x = 0; node_x <= 8; node_x += kMinimapNodeTiles) {
		for (int node_y = 0; node_y <= 8; node_y += kMinimapNodeTiles) {
			map.AddNode(node_x, node_y);
		}
	}

	SECTION("last tile of a node") {
		const MinimapDirtyRect rect { .x = 3, .y = 7, .width = 1, .height = 1 };
		RequireRectFilled(map, FillRect(map, rect, 0, 0), rect, 0, 0);
	}
	SECTION("first tile of a node") {
		const MinimapDirtyRect rect { .x = 4, .y = 8, .width = 1, .height = 1 };
		RequireRectFilled(map, FillRect(map, rect, 0, 0), rect, 0, 0);
	}
	SECTION("last tile of a node row") {
		const MinimapDirtyRect rect { .x = 7, .y = 3, .width = 1, .height = 1 };
		RequireRectFilled(map, FillRect(map, rect, 0, 0), rect, 0, 0);
	}
}

TEST_CASE("a rect whose last tile starts a new node includes that node", "[minimap]") {
	FakeMap map;
	for (int node_x = 0; node_x <= 8; node_x += kMinimapNodeTiles) {
		for (int node_y = 0; node_y <= 4; node_y += kMinimapNodeTiles) {
			map.AddNode(node_x, node_y);
		}
	}

	// Tiles 2..8 in x cover nodes 0, 1 and 2; tiles 2..4 in y cover nodes 0 and 1.
	// The old inclusive-end code stopped at node 1, leaving the x = 8 column untouched.
	const MinimapDirtyRect rect { .x = 2, .y = 2, .width = 7, .height = 3 };
	RequireRectFilled(map, FillRect(map, rect, 0, 0), rect, 0, 0);
}

TEST_CASE("page offsets move the dirty rect in map space", "[minimap]") {
	constexpr int page_x = 2;
	constexpr int page_y = 3;

	FakeMap map;
	map.AddNode(page_x * kMinimapPageSize + 508, page_y * kMinimapPageSize + 508);

	const MinimapDirtyRect rect { .x = 511, .y = 511, .width = 1, .height = 1 };
	RequireRectFilled(map, FillRect(map, rect, page_x, page_y), rect, page_x, page_y);
}

TEST_CASE("a full page is covered end to end", "[minimap]") {
	FakeMap map;
	for (int node_x = 0; node_x < kMinimapPageSize; node_x += kMinimapNodeTiles) {
		for (int node_y = 0; node_y < kMinimapPageSize; node_y += kMinimapNodeTiles) {
			map.AddNode(node_x, node_y);
		}
	}

	const MinimapDirtyRect rect { .x = 0, .y = 0, .width = kMinimapPageSize, .height = kMinimapPageSize };
	const std::vector<uint8_t> buffer = FillRect(map, rect, 0, 0);
	REQUIRE(buffer.size() == static_cast<size_t>(kMinimapPageSize) * kMinimapPageSize);
	REQUIRE(std::none_of(buffer.begin(), buffer.end(), [](uint8_t value) {
		return value == kUnwritten;
	}));

	const std::pair<int, int> samples[] = {
		{ 0, 0 },
		{ kMinimapPageSize - 1, 0 },
		{ 0, kMinimapPageSize - 1 },
		{ kMinimapPageSize - 1, kMinimapPageSize - 1 },
		{ 256, 128 },
	};
	for (const auto& [x, y] : samples) {
		REQUIRE(buffer[static_cast<size_t>(y) * kMinimapPageSize + x] == TestColor(x, y));
	}
}

TEST_CASE("tiles without a node stay unwritten", "[minimap]") {
	FakeMap map;
	map.AddNode(0, 0);

	const MinimapDirtyRect rect { .x = 0, .y = 0, .width = 8, .height = 8 };
	const std::vector<uint8_t> buffer = FillRect(map, rect, 0, 0);

	REQUIRE(buffer.size() == 64);
	for (int y = 0; y < rect.height; ++y) {
		for (int x = 0; x < rect.width; ++x) {
			const uint8_t actual = buffer[static_cast<size_t>(y) * rect.width + x];
			if (x < kMinimapNodeTiles && y < kMinimapNodeTiles) {
				REQUIRE(actual == TestColor(x, y));
			} else {
				REQUIRE(actual == kUnwritten);
			}
		}
	}
}

TEST_CASE("an empty dirty rect writes nothing", "[minimap]") {
	FakeMap map;
	const MinimapDirtyRect rect { .x = 5, .y = 5, .width = 0, .height = 1 };
	REQUIRE(FillRect(map, rect, 0, 0).empty());
}
