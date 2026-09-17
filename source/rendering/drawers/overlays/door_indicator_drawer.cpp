#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include <nanovg.h>
#include <algorithm>
#include "rendering/core/render_view.h"
#include "rendering/utilities/icon_renderer.h"
#include "util/image_manager.h"

DoorIndicatorDrawer::DoorIndicatorDrawer() {
	requests.reserve(100);
}

DoorIndicatorDrawer::~DoorIndicatorDrawer() = default;

void DoorIndicatorDrawer::addDoor(const Position& pos, bool locked, bool south, bool east) {
	requests.push_back({ pos, locked, south, east });
}

void DoorIndicatorDrawer::clear() {
	requests.clear();
}

void DoorIndicatorDrawer::draw(NVGcontext* vg, const RenderView& view) {
	if (requests.empty() || !vg) {
		return;
	}

	nvgSave(vg);

	const NVGcolor colorLocked = nvgRGBA(255, 0, 0, 255); // Red
	const NVGcolor colorUnlocked = nvgRGBA(102, 255, 0, 255); // Green (#66ff00)

	const float zoomFactor = 1.0f / view.zoom;
	const float iconSize = 12.0f * zoomFactor;
	const float outlineOffset = std::max(1.0f, 1.0f * zoomFactor);
	const float tileSize = 32.0f * zoomFactor;
	const float halfTileSize = tileSize * 0.5f;

	for (const auto& request : requests) {
		// Only render doors on the current floor
		if (request.pos.z != view.floor) {
			continue;
		}

		int unscaled_x, unscaled_y;
		if (!view.IsTileVisible(request.pos.x, request.pos.y, request.pos.z, unscaled_x, unscaled_y)) {
			continue;
		}

		const float x = static_cast<float>(unscaled_x) * zoomFactor;
		const float y = static_cast<float>(unscaled_y) * zoomFactor;

		const std::string_view icon = request.locked ? ICON_LOCK : ICON_LOCK_OPEN;
		const NVGcolor color = request.locked ? colorLocked : colorUnlocked;

		if (request.south) {
			// Center of WEST border
			IconRenderer::DrawIconWithBorder(vg, x, y + halfTileSize, iconSize, outlineOffset, icon, color);
		}
		if (request.east) {
			// Center of NORTH border
			IconRenderer::DrawIconWithBorder(vg, x + halfTileSize, y, iconSize, outlineOffset, icon, color);
		}

		if (!request.south && !request.east) {
			// Center of TILE
			IconRenderer::DrawIconWithBorder(vg, x + halfTileSize, y + halfTileSize, iconSize, outlineOffset, icon, color);
		}
	}

	nvgRestore(vg);
}
