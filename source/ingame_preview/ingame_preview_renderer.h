#ifndef RME_INGAME_PREVIEW_RENDERER_H_
#define RME_INGAME_PREVIEW_RENDERER_H_

#include "app/main.h"
#include "map/position.h"
#include "game/creature.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/light_defaults.h"
#include <memory>
#include <string>

class Tile;
class BaseMap;
class TileRenderer;
class SpriteBatch;
class PrimitiveRenderer;
class LightDrawer;
struct LightBuffer;
class CreatureDrawer;
class CreatureNameDrawer;
class SpriteDrawer;
struct Outfit;
struct NVGcontext;

namespace IngamePreview {

	class FloorVisibilityCalculator;

	/**
	 * High-level renderer for the in-game preview window.
	 */
	class IngamePreviewRenderer {
	public:
		IngamePreviewRenderer(TileRenderer* tile_renderer);
		~IngamePreviewRenderer();

		/**
		 * Render the preview.
		 */
		void Render(NVGcontext* vg, const BaseMap& map, int viewport_x, int viewport_y, int viewport_width, int viewport_height, const Position& camera_pos, float zoom, bool lighting_enabled, uint8_t client_brightness_percent, const Outfit& preview_outfit, Direction preview_direction, float walk_progress, int offset_x, int offset_y);

		void SetLightIntensity(uint8_t intensity) {
			light_intensity = intensity;
		}
		void SetServerLightColor(uint8_t color) {
			server_light_color = color;
		}

		void SetName(const std::string& name) {
			preview_name = name;
		}

	private:
		TileRenderer* tile_renderer;
		std::unique_ptr<FloorVisibilityCalculator> floor_calculator;

		uint8_t light_intensity = rme::lighting::DEFAULT_SERVER_LIGHT_INTENSITY;
		uint8_t server_light_color = rme::lighting::DEFAULT_SERVER_LIGHT_COLOR;
		std::string preview_name = "You";

		// Internal rendering resources (could be shared or managed)
		std::unique_ptr<SpriteBatch> sprite_batch;
		std::unique_ptr<PrimitiveRenderer> primitive_renderer;
		std::unique_ptr<LightBuffer> light_buffer;
		std::shared_ptr<LightDrawer> light_drawer;

		// Drawers
		std::unique_ptr<CreatureDrawer> creature_drawer;
		std::unique_ptr<CreatureNameDrawer> creature_name_drawer;
		std::unique_ptr<SpriteDrawer> sprite_drawer;

		int GetTileElevationOffset(const Tile* tile) const;
	};

} // namespace IngamePreview

#endif // RME_INGAME_PREVIEW_RENDERER_H_
