#ifndef RME_RENDERING_CORE_LIGHT_BUFFER_H_
#define RME_RENDERING_CORE_LIGHT_BUFFER_H_

#include <vector>
#include <cstdint>
#include <algorithm>
#include "rendering/core/sprite_light.h"

struct RenderView;

struct LightBuffer {
	struct Light {
		int32_t pixel_x = 0;
		int32_t pixel_y = 0;
		uint8_t color = 0;
		uint8_t intensity = 0;
	};

	struct TileLight {
		uint32_t start = 0;
		uint8_t color = 0;
	};

	std::vector<Light> lights;
	std::vector<TileLight> tiles;
	int origin_x = 0;
	int origin_y = 0;
	int width = 0;
	int height = 0;
	uint32_t current_floor_light_start = 0;

	void Prepare(const RenderView& view);
	void SetFloorLightStart() noexcept {
		current_floor_light_start = static_cast<uint32_t>(lights.size());
	}
	void AddLight(int pixel_x, int pixel_y, const SpriteLight& light);
	void AddTileLight(int tile_x, int tile_y, const SpriteLight& light);
	void AddScreenLight(int screen_x, int screen_y, const RenderView& view, const SpriteLight& light);
	void SetFieldBrightness(int tile_x, int tile_y, uint32_t start, uint8_t color = 0);
	void Clear();

	[[nodiscard]] bool ContainsTile(int tile_x, int tile_y) const noexcept;
	[[nodiscard]] int IndexOf(int tile_x, int tile_y) const noexcept;
};

#endif
