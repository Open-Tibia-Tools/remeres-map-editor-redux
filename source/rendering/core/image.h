#ifndef RME_RENDERING_CORE_IMAGE_H_
#define RME_RENDERING_CORE_IMAGE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include "rendering/core/atlas_manager.h" // For AtlasRegion

struct ImageDimensions {
	uint16_t width = TextureAtlas::BASE_SLOT_SIZE;
	uint16_t height = TextureAtlas::BASE_SLOT_SIZE;

	[[nodiscard]] size_t pixelCount() const {
		return static_cast<size_t>(width) * static_cast<size_t>(height);
	}
};

class Image {
public:
	Image();
	virtual ~Image() = default;

	bool isGLLoaded = false;
	uint32_t generation_id = 0;


	virtual std::unique_ptr<uint8_t[]> getRGBData() = 0;
	virtual std::unique_ptr<uint8_t[]> getRGBAData() = 0;
	virtual ImageDimensions getDimensions() const {
		return {};
	}

	virtual bool isNormalImage() const {
		return false;
	}

protected:
	// Helper to handle atlas interactions
	const AtlasRegion* EnsureAtlasSprite(uint32_t sprite_id, std::unique_ptr<uint8_t[]> preloaded_data = nullptr, std::optional<ImageDimensions> dimensions = std::nullopt);
};

#endif
