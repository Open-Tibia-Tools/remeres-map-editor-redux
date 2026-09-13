#ifndef RME_RENDERING_CORE_NORMAL_IMAGE_H_
#define RME_RENDERING_CORE_NORMAL_IMAGE_H_

#include "rendering/core/image.h"

#include <vector>

class GameSprite;

class NormalImage : public Image {
public:
	NormalImage();
	~NormalImage() override;

	bool isNormalImage() const override {
		return true;
	}

	const AtlasRegion* getAtlasRegion();

	// We use the sprite id as key
	uint32_t id;
	const AtlasRegion* atlas_region;
	bool is_preloading = false;

	// This contains the pixel data
	uint16_t size;
	std::unique_ptr<uint8_t[]> dump;
	uint16_t pixel_width = TextureAtlas::BASE_SLOT_SIZE;
	uint16_t pixel_height = TextureAtlas::BASE_SLOT_SIZE;

	void clean(time_t time, int longevity) override;

	std::unique_ptr<uint8_t[]> getRGBData() override;
	std::unique_ptr<uint8_t[]> getRGBAData() override;
	ImageDimensions getDimensions() const override {
		return ImageDimensions { pixel_width, pixel_height };
	}

	void fulfillPreload(std::unique_ptr<uint8_t[]> preloaded_data);
	void addParent(GameSprite* sprite);

	GameSprite* parent = nullptr;
	std::vector<GameSprite*> parents;
};

#endif
