//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/managers/minimap_manager.h"
#include "app/managers/version_manager.h"
#include "map/map.h"
#include "map/position.h"
#include "ui/gui.h"
#include "rendering/ui/minimap_window.h"
#include <wx/aui/aui.h>

MinimapManager g_minimap;

wxAuiPaneInfo MinimapManager::DefaultPaneInfo() {
	return wxAuiPaneInfo()
		.Name("Minimap")
		.Caption("Minimap")
		.Right()
		.Layer(0)
		.Position(2)
		.CloseButton(true)
		.MaximizeButton(true)
		.BestSize(260, 220);
}

void MinimapManager::NormalizePaneInfo(wxAuiPaneInfo& info) {
	info.Name("Minimap")
		.Caption("Minimap")
		.CloseButton(true)
		.MaximizeButton(true);
}

MinimapManager::MinimapManager() :
	minimap(nullptr) {
}

MinimapManager::~MinimapManager() {
	spdlog::debug("MinimapManager destructor started");
	spdlog::default_logger()->flush();

	// Root window will destroy minimap window if it's a child.
	// But we should detach it from aui_manager if it exists.
	if (minimap && g_gui.aui_manager) {
		spdlog::debug("MinimapManager destructor - detaching minimap from aui_manager");
		spdlog::default_logger()->flush();
		g_gui.aui_manager->DetachPane(minimap);
	}
	spdlog::debug("MinimapManager destructor finished");
	spdlog::default_logger()->flush();
}

MinimapManager::InvalidationKey MinimapManager::makeKey(const Map& map) {
	return {
		.map = &map,
		.generation = map.getGeneration(),
	};
}

void MinimapManager::Create() {
	if (!g_version.IsVersionLoaded()) {
		return;
	}

	if (minimap) {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(minimap);
		NormalizePaneInfo(info);
		info.Show(true);
	} else {
		minimap = newd MinimapWindow(g_gui.root);
		minimap->Show(true);
		g_gui.aui_manager->AddPane(minimap, DefaultPaneInfo());
	}
	g_gui.aui_manager->Update();
}

void MinimapManager::Hide() {
	if (minimap) {
		g_gui.aui_manager->GetPane(minimap).Show(false);
		g_gui.aui_manager->Update();
	}
}

void MinimapManager::Destroy() {
	spdlog::info("MinimapManager::Destroy called");
	spdlog::default_logger()->flush();
	if (minimap) {
		if (g_gui.aui_manager) {
			spdlog::info("MinimapManager::Destroy - detaching minimap from aui_manager");
			spdlog::default_logger()->flush();
			g_gui.aui_manager->DetachPane(minimap);

			spdlog::info("MinimapManager::Destroy - updating aui_manager");
			spdlog::default_logger()->flush();
			g_gui.aui_manager->Update();
		}

		spdlog::info("MinimapManager::Destroy - calling minimap->Destroy()");
		spdlog::default_logger()->flush();
		minimap->Destroy();

		spdlog::info("MinimapManager::Destroy - resetting minimap pointer");
		spdlog::default_logger()->flush();
		minimap = nullptr;
	}
	spdlog::info("MinimapManager::Destroy finished");
	spdlog::default_logger()->flush();
}

void MinimapManager::Update(bool immediate) {
	if (g_gui.IsLoading()) {
		return;
	}

	if (IsVisible()) {
		minimap->RefreshMinimap(immediate);
	}
}

void MinimapManager::InvalidateAll(const Map& map) {
	auto& pending = pending_invalidations_[makeKey(map)];
	pending.invalidate_all = true;
	for (auto& floor_rects : pending.floor_rects) {
		floor_rects.reset();
	}
}

void MinimapManager::MarkTileDirty(const Map& map, const Position& position) {
	if (position.z < 0 || position.z >= MAP_LAYERS) {
		return;
	}

	auto& pending = pending_invalidations_[makeKey(map)];
	if (pending.invalidate_all) {
		return;
	}

	const MinimapDirtyRect tile_rect = {
		.x = position.x,
		.y = position.y,
		.width = 1,
		.height = 1,
	};

	auto& floor_rect = pending.floor_rects[position.z];
	floor_rect = floor_rect ? UnionMinimapRects(*floor_rect, tile_rect) : tile_rect;
}

PendingMinimapInvalidation MinimapManager::TakePendingInvalidation(const Map& map) {
	const InvalidationKey key = makeKey(map);
	auto it = pending_invalidations_.find(key);
	if (it == pending_invalidations_.end()) {
		return {};
	}

	PendingMinimapInvalidation pending = std::move(it->second);
	pending_invalidations_.erase(it);
	return pending;
}

bool MinimapManager::IsVisible() const {
	if (minimap && g_gui.aui_manager) {
		const wxAuiPaneInfo& pi = g_gui.aui_manager->GetPane(minimap);
		if (pi.IsShown()) {
			return true;
		}
	}
	return false;
}
