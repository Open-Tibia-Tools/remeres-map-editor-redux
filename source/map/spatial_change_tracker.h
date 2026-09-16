//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_SPATIAL_CHANGE_TRACKER_H_
#define RME_MAP_SPATIAL_CHANGE_TRACKER_H_

#include "app/definitions.h"
#include "map/position.h"

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <array>
#include <optional>
#include <functional>

/// Coordinate of a 16x16 macro-chunk on floor z
struct ChunkCoord {
	int32_t cx = 0;
	int32_t cy = 0;
	int32_t z = 0;

	constexpr bool operator==(const ChunkCoord& other) const noexcept = default;
	constexpr auto operator<=>(const ChunkCoord& other) const noexcept = default;
};

struct ChunkCoordHash {
	size_t operator()(const ChunkCoord& c) const noexcept {
		size_t h1 = std::hash<int32_t>{}(c.cx);
		size_t h2 = std::hash<int32_t>{}(c.cy);
		size_t h3 = std::hash<int32_t>{}(c.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

/// Coordinate of a 4x4 MapNode on floor z
struct NodeCoord {
	int32_t nx = 0;
	int32_t ny = 0;
	int32_t z = 0;

	constexpr bool operator==(const NodeCoord& other) const noexcept = default;
	constexpr auto operator<=>(const NodeCoord& other) const noexcept = default;
};

struct NodeCoordHash {
	size_t operator()(const NodeCoord& n) const noexcept {
		size_t h1 = std::hash<int32_t>{}(n.nx);
		size_t h2 = std::hash<int32_t>{}(n.ny);
		size_t h3 = std::hash<int32_t>{}(n.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

struct DirtyRect {
	int32_t min_x = 0;
	int32_t min_y = 0;
	int32_t max_x = 0;
	int32_t max_y = 0;
};

class SpatialChangeTracker {
public:
	static constexpr int32_t CHUNK_SIZE = 16;
	static constexpr int32_t CHUNK_SHIFT = 4;
	static constexpr int32_t NODE_SIZE = 4;
	static constexpr int32_t NODE_SHIFT = 2;

	SpatialChangeTracker() = default;
	~SpatialChangeTracker() = default;

	// Non-copyable, movable
	SpatialChangeTracker(const SpatialChangeTracker&) = delete;
	SpatialChangeTracker& operator=(const SpatialChangeTracker&) = delete;
	SpatialChangeTracker(SpatialChangeTracker&&) noexcept = default;
	SpatialChangeTracker& operator=(SpatialChangeTracker&&) noexcept = default;

	// Invalidation marking methods
	void markTileDirty(int32_t x, int32_t y, int32_t z);
	void markTileDirty(const Position& pos) {
		markTileDirty(pos.x, pos.y, pos.z);
	}
	void markNodeDirty(int32_t nx, int32_t ny, int32_t z);
	void markChunkDirty(int32_t cx, int32_t cy, int32_t z);
	void markRegionDirty(int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t z);
	void markAllDirty();

	// Queries
	[[nodiscard]] bool hasChanges() const noexcept {
		return all_dirty_ || !dirty_chunks_.empty();
	}
	[[nodiscard]] bool isAllDirty() const noexcept {
		return all_dirty_;
	}
	[[nodiscard]] uint64_t getGeneration() const noexcept {
		return generation_;
	}
	[[nodiscard]] bool isChunkDirty(int32_t cx, int32_t cy, int32_t z) const noexcept;
	[[nodiscard]] const std::unordered_set<ChunkCoord, ChunkCoordHash>& getDirtyChunks() const noexcept {
		return dirty_chunks_;
	}
	[[nodiscard]] const std::unordered_set<NodeCoord, NodeCoordHash>& getDirtyNodes() const noexcept {
		return dirty_nodes_;
	}
	[[nodiscard]] const std::array<std::optional<DirtyRect>, MAP_LAYERS>& getDirtyRects() const noexcept {
		return dirty_rects_by_floor_;
	}

	// Drain / reset methods for consumers (e.g. ChunkCacheManager, MinimapManager)
	void clearDirty() noexcept;
	[[nodiscard]] std::unordered_set<ChunkCoord, ChunkCoordHash> takeDirtyChunks() noexcept;
	[[nodiscard]] std::array<std::optional<DirtyRect>, MAP_LAYERS> takeDirtyRects() noexcept;

	// Listener registration
	using InvalidationListener = std::function<void(const SpatialChangeTracker&)>;
	uint32_t addListener(InvalidationListener listener);
	void removeListener(uint32_t listener_id);

private:
	void notifyListeners();
	void expandFloorRect(int32_t x, int32_t y, int32_t z);

	bool all_dirty_ = false;
	uint64_t generation_ = 0;
	std::unordered_set<ChunkCoord, ChunkCoordHash> dirty_chunks_;
	std::unordered_set<NodeCoord, NodeCoordHash> dirty_nodes_;
	std::array<std::optional<DirtyRect>, MAP_LAYERS> dirty_rects_by_floor_;

	struct ListenerEntry {
		uint32_t id;
		InvalidationListener callback;
	};
	std::vector<ListenerEntry> listeners_;
	uint32_t next_listener_id_ = 1;
};

#endif
