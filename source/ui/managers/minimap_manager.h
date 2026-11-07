//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MINIMAP_MANAGER_H_
#define RME_MINIMAP_MANAGER_H_

#include "app/main.h"
#include "rendering/drawers/minimap_cache.h"

#include <array>
#include <optional>
#include <unordered_map>

class MinimapWindow;
class Map;
class Position;

struct PendingMinimapInvalidation {
	bool invalidate_all = false;
	std::array<std::optional<MinimapDirtyRect>, MAP_LAYERS> floor_rects;
};

class MinimapManager {
public:
	MinimapManager();
	~MinimapManager();

	void Create();
	void Hide();
	void Destroy();
	void Update(bool immediate = false);
	bool IsVisible() const;
	void InvalidateAll(const Map& map);
	void MarkTileDirty(const Map& map, const Position& position);
	PendingMinimapInvalidation TakePendingInvalidation(const Map& map);

	MinimapWindow* GetWindow() {
		return minimap;
	}
	void SetWindow(MinimapWindow* mw) {
		minimap = mw;
	}

	[[nodiscard]] static wxAuiPaneInfo DefaultPaneInfo();
	static void NormalizePaneInfo(wxAuiPaneInfo& info);

private:
	struct InvalidationKey {
		const Map* map = nullptr;
		uint64_t generation = 0;

		bool operator==(const InvalidationKey& other) const {
			return map == other.map && generation == other.generation;
		}
	};

	struct InvalidationKeyHash {
		size_t operator()(const InvalidationKey& key) const {
			return std::hash<const Map*>()(key.map) ^ (std::hash<uint64_t>()(key.generation) << 1);
		}
	};

	static InvalidationKey makeKey(const Map& map);

	MinimapWindow* minimap;
	std::unordered_map<InvalidationKey, PendingMinimapInvalidation, InvalidationKeyHash> pending_invalidations_;
};

extern MinimapManager g_minimap;

#endif
