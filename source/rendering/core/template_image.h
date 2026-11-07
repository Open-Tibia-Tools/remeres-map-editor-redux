#ifndef RME_RENDERING_CORE_TEMPLATE_IMAGE_H_
#define RME_RENDERING_CORE_TEMPLATE_IMAGE_H_

#include "rendering/core/image.h"
#include "game/outfit.h"

class GameSprite;

class TemplateImage : public Image {
public:
	TemplateImage(GameSprite* parent, int v, const Outfit& outfit);
	~TemplateImage() override;


	virtual std::unique_ptr<uint8_t[]> getRGBData() override;
	virtual std::unique_ptr<uint8_t[]> getRGBAData() override;
	ImageDimensions getDimensions() const override;

	const AtlasRegion* getAtlasRegion(bool allow_upload = true);
	const AtlasRegion* atlas_region;

	uint32_t texture_id; // Unique ID for AtlasManager key
	GameSprite* parent;
	int sprite_index;
	uint8_t lookHead;
	uint8_t lookBody;
	uint8_t lookLegs;
	uint8_t lookFeet;
};

#endif
