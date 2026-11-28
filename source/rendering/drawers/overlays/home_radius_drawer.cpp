#include "rendering/drawers/overlays/home_radius_drawer.h"

#include "editor/selection.h"
#include "game/creature.h"
#include "map/map.h"
#include "map/tile.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "ui/theme.h"
#include "util/nvg_utils.h"

#include <nanovg.h>

namespace rme::rendering {

	namespace {

		// Larger selections (a whole area) show no radii; they would cover the view.
		constexpr size_t kMaxSelectedHomes = 64;
		constexpr unsigned char kFillAlpha = 30;
		constexpr unsigned char kStrokeAlpha = 170;
		constexpr float kStrokeWidth = 1.5f;

		const Creature* homeOf(const Tile* tile, int floor) {
			if (!tile || !tile->creature || tile->getZ() != floor || !tile->creature->getHomeRadius()) {
				return nullptr;
			}
			return tile->creature.get();
		}

		// Calls fn(tile, radius) for the home under the mouse and the selected homes on the shown floor.
		template <typename Fn>
		void forEachShownHome(const Map& map, const RenderView& view, const DrawingOptions& options, const Selection& selection, Fn&& fn) {
			if (options.ingame || !options.show_creatures) {
				return;
			}
			const Tile* hovered = map.getTile(view.mouse_map_x, view.mouse_map_y, view.floor);
			if (const Creature* home = homeOf(hovered, view.floor); home && !hovered->isSelected()) {
				fn(*hovered, *home->getHomeRadius());
			}
			if (selection.size() > kMaxSelectedHomes) {
				return;
			}
			for (const Tile* tile : selection.getTiles()) {
				if (const Creature* home = homeOf(tile, view.floor)) {
					fn(*tile, *home->getHomeRadius());
				}
			}
		}

	} // namespace

	bool HomeRadiusDrawer::hasHomes(const Map& map, const RenderView& view, const DrawingOptions& options, const Selection& selection) const {
		bool found = false;
		forEachShownHome(map, view, options, selection, [&found](const Tile&, int) { found = true; });
		return found;
	}

	void HomeRadiusDrawer::draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options, const Selection& selection) const {
		if (!vg) {
			return;
		}
		const float inv_zoom = 1.0f / view.zoom;
		const float tile_size = 32.0f * inv_zoom;
		NVGcolor fill = NvgUtils::ToNvColor(Theme::Get(Theme::Role::HomeRadius));
		NVGcolor stroke = fill;
		fill.a = static_cast<float>(kFillAlpha) / 255.0f;
		stroke.a = static_cast<float>(kStrokeAlpha) / 255.0f;

		forEachShownHome(map, view, options, selection, [&](const Tile& tile, int radius) {
			const Position& position = tile.getPosition();
			int unscaled_x = 0;
			int unscaled_y = 0;
			view.getScreenPosition(position.x, position.y, position.z, unscaled_x, unscaled_y);
			const float x = static_cast<float>(unscaled_x) * inv_zoom;
			const float y = static_cast<float>(unscaled_y) * inv_zoom;
			const float reach = static_cast<float>(radius) * tile_size;

			nvgBeginPath(vg);
			nvgRect(vg, x - reach, y - reach, tile_size + reach * 2.0f, tile_size + reach * 2.0f);
			nvgFillColor(vg, fill);
			nvgFill(vg);
			nvgStrokeColor(vg, stroke);
			nvgStrokeWidth(vg, kStrokeWidth);
			nvgStroke(vg);
		});
	}

} // namespace rme::rendering
