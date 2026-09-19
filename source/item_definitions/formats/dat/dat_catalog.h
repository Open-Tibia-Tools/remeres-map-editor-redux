#ifndef RME_DAT_CATALOG_H_
#define RME_DAT_CATALOG_H_

#include "app/client_version.h"
#include "item_definitions/core/item_definition_fragments.h"
#include "rendering/core/sprite_light.h"

#include <cstdint>
#include <optional>
#include <vector>

struct DatAnimationFrameDuration {
	uint32_t minimum = 0;
	uint32_t maximum = 0;
};

struct DatAnimationInfo {
	bool asynchronous = false;
	int loop_count = 0;
	int8_t start_frame = 0;
	std::vector<DatAnimationFrameDuration> frame_durations;
};

struct DatSpriteDimensions {
	uint16_t width = 32;
	uint16_t height = 32;
};

struct DatCatalogEntry {
	uint32_t client_id = 0;
	DatItemFragment item_fragment;
	uint8_t height = 0;
	uint8_t width = 0;
	uint8_t layers = 0;
	uint8_t pattern_x = 0;
	uint8_t pattern_y = 0;
	uint8_t pattern_z = 0;
	uint8_t frames = 0;
	uint32_t numsprites = 0;
	uint16_t draw_height = 0;
	uint16_t drawoffset_x = 0;
	uint16_t drawoffset_y = 0;
	uint16_t minimap_color = 0;
	bool has_light = false;
	SpriteLight light {};
	std::vector<uint32_t> sprite_ids;
	std::optional<DatAnimationInfo> animation;

	[[nodiscard]] bool valid() const {
		return client_id != 0;
	}
};

struct DatCatalog {
	uint32_t signature = 0;
	DatFormat format = DAT_FORMAT_UNKNOWN;
	bool is_extended = false;
	bool has_transparency = false;
	bool has_frame_durations = false;
	bool has_frame_groups = false;
	uint32_t item_count = 0;
	uint32_t creature_count = 0;
	uint32_t effect_count = 0;
	uint32_t distance_count = 0;
	uint32_t max_sprite_id = 0;
	std::vector<DatCatalogEntry> entries;
	std::vector<DatSpriteDimensions> sprite_dimensions;

	[[nodiscard]] uint32_t lastEntryId() const {
		return static_cast<uint32_t>(item_count) + static_cast<uint32_t>(creature_count);
	}

	[[nodiscard]] const DatCatalogEntry* entry(uint32_t id) const {
		if (id >= entries.size()) {
			return nullptr;
		}
		const auto& candidate = entries[id];
		return candidate.client_id == id ? &candidate : nullptr;
	}

	[[nodiscard]] DatCatalogEntry* entry(uint32_t id) {
		if (id >= entries.size()) {
			return nullptr;
		}
		auto& candidate = entries[id];
		return candidate.client_id == id ? &candidate : nullptr;
	}

	[[nodiscard]] DatSpriteDimensions spriteDimension(uint32_t id) const {
		if (id >= sprite_dimensions.size()) {
			return {};
		}
		return sprite_dimensions[id];
	}
};

#endif
