#include "rendering/core/graphics_assembler.h"

#include "item_definitions/formats/dat/dat_catalog.h"
#include "rendering/core/sprite_animation.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/sprite_archive.h"
#include "rendering/core/sprite_preloader.h"

#include <algorithm>
#include <format>
#include <memory>

namespace {
	[[nodiscard]] size_t imageSpaceSize(const DatCatalog& catalog) {
		return std::max<size_t>(1, static_cast<size_t>(catalog.max_sprite_id) + 1);
	}

	[[nodiscard]] ImageDimensions toImageDimensions(const DatSpriteDimensions& dimensions) {
		return ImageDimensions {
			.width = dimensions.width,
			.height = dimensions.height,
		};
	}

	bool validateCatalog(const DatCatalog& catalog, std::string& error) {
		if (catalog.max_sprite_id >= MAX_SPRITES) {
			error = std::format(
				"DAT catalog references sprite id {} which exceeds MAX_SPRITES={}.",
				catalog.max_sprite_id,
				MAX_SPRITES);
			return false;
		}

		const bool has_any_entry = std::ranges::any_of(catalog.entries, [](const DatCatalogEntry& entry) {
			return entry.valid();
		});
		if (!has_any_entry) {
			error = "The DAT catalog does not contain any installable entries.";
			return false;
		}

		return true;
	}
}

NormalImage* GraphicsAssembler::ensureImage(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, uint32_t sprite_id) {
	if (sprite_id >= manager.image_space.size()) {
		return nullptr;
	}

	auto& slot = manager.image_space[sprite_id];
	if (!slot) {
		auto image = std::make_unique<NormalImage>();
		image->id = sprite_id;
		const auto dimensions = sprite_archive && sprite_archive->isProtobuf() ? sprite_archive->spriteDimensions(sprite_id) : toImageDimensions(catalog.spriteDimension(sprite_id));
		image->pixel_width = dimensions.width;
		image->pixel_height = dimensions.height;
		slot = std::move(image);
	} else if (slot->isNormalImage()) {
		auto* image = static_cast<NormalImage*>(slot.get());
		const auto dimensions = sprite_archive && sprite_archive->isProtobuf() ? sprite_archive->spriteDimensions(sprite_id) : toImageDimensions(catalog.spriteDimension(sprite_id));
		image->pixel_width = dimensions.width;
		image->pixel_height = dimensions.height;
	}

	return static_cast<NormalImage*>(slot.get());
}

void GraphicsAssembler::installAnimation(GameSprite& sprite, const DatCatalogEntry& entry) {
	if (entry.frames <= 1) {
		sprite.animation_data.reset();
		return;
	}

	rme::rendering::SpriteAnimationData data;
	data.frame_count = entry.frames;

	if (entry.animation.has_value() && !entry.animation->frame_durations.empty()) {
		const auto& animation = *entry.animation;
		data.start_frame = animation.start_frame;
		data.loop_count = static_cast<int8_t>(animation.loop_count);
		data.is_async = animation.asynchronous;

		data.durations.reserve(animation.frame_durations.size());
		uint32_t total_dur = 0;
		uint32_t shortest = 0;

		for (const auto& dur : animation.frame_durations) {
			const uint32_t min_d = dur.minimum;
			const uint32_t max_d = dur.maximum;
			data.durations.push_back(rme::rendering::AnimationDuration {
				.min_ms = min_d,
				.max_ms = max_d
			});
			const uint32_t avg = (min_d + max_d) / 2;
			total_dur += avg;

			const uint32_t candidate = min_d > 0 ? min_d : max_d;
			if (candidate > 0 && (shortest == 0 || candidate < shortest)) {
				shortest = candidate;
			}
		}

		data.total_duration_ms = total_dur;

		if (shortest == 0) {
			data.shortest_step_ms = data.frame_count > 0 ? std::max(1u, total_dur / data.frame_count) : 500;
		} else {
			data.shortest_step_ms = std::max<uint32_t>(16, shortest);
		}
	} else {
		// Pre-10.50 (7.x - 9.x) uniform 500ms pacing: all classic client animations (water, shorelines, borders) are globally synchronized.
		data.shortest_step_ms = rme::rendering::ITEM_FRAME_DURATION;
		data.total_duration_ms = static_cast<uint32_t>(data.frame_count) * rme::rendering::ITEM_FRAME_DURATION;
		data.is_async = false;
		data.start_frame = 0;
	}

	sprite.animation_data = std::move(data);
}

bool GraphicsAssembler::installSpriteEntry(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, const DatCatalogEntry& entry, std::vector<std::string>& warnings) {
	auto sprite = std::make_unique<GameSprite>();
	auto* sprite_ptr = sprite.get();

	sprite_ptr->id = entry.client_id;
	sprite_ptr->height = entry.height;
	sprite_ptr->width = entry.width;
	sprite_ptr->layers = entry.layers;
	sprite_ptr->pattern_x = entry.pattern_x;
	sprite_ptr->pattern_y = entry.pattern_y;
	sprite_ptr->pattern_z = entry.pattern_z;
	sprite_ptr->frames = entry.frames;
	sprite_ptr->numsprites = entry.numsprites;
	sprite_ptr->draw_height = entry.draw_height;
	sprite_ptr->drawoffset_x = entry.drawoffset_x;
	sprite_ptr->drawoffset_y = entry.drawoffset_y;
	sprite_ptr->minimap_color = entry.minimap_color;
	sprite_ptr->has_light = entry.has_light;
	sprite_ptr->light = entry.light;
	installAnimation(*sprite_ptr, entry);

	sprite_ptr->sprite_ids = entry.sprite_ids;
	sprite_ptr->spriteList.clear();
	sprite_ptr->spriteList.reserve(entry.sprite_ids.size());
	for (uint32_t sprite_id : entry.sprite_ids) {
		NormalImage* image = ensureImage(manager, catalog, sprite_archive, sprite_id);
		if (!image) {
			warnings.push_back(std::format("GraphicsAssembler: sprite {} references out-of-range image {}.", entry.client_id, sprite_id));
			return false;
		}
		sprite_ptr->spriteList.push_back(image);
	}
	sprite_ptr->updateSimpleStatus();

	manager.sprite_space[entry.client_id] = std::move(sprite);
	return true;
}

void GraphicsAssembler::resetRuntimeState(GraphicManager& manager) {
	SpritePreloader::get().clear();
	manager.unloaded = true;
	manager.sprite_archive_.reset();
	manager.spritefile.clear();
	manager.sprite_space.clear();
	manager.image_space.clear();
	if (manager.atlas_manager_) {
		manager.atlas_manager_->clear();
		manager.atlas_manager_.reset();
	}
}

bool GraphicsAssembler::install(GraphicManager& manager, const DatCatalog& catalog, std::shared_ptr<SpriteArchive> sprite_archive, std::string& error, std::vector<std::string>& warnings) {
	if (!sprite_archive) {
		error = "Sprite archive is missing.";
		return false;
	}

	if (!validateCatalog(catalog, error)) {
		return false;
	}

	const auto sprite_space_size = std::max<size_t>(1, catalog.entries.size());
	const auto image_space_size = imageSpaceSize(catalog);

	resetRuntimeState(manager);
	manager.sprite_space.resize(sprite_space_size);
	manager.image_space.resize(image_space_size);

	for (const auto& entry : catalog.entries) {
		if (!entry.valid()) {
			continue;
		}
		if (!installSpriteEntry(manager, catalog, sprite_archive, entry, warnings)) {
			error = std::format("Failed to install graphics for client id {}.", entry.client_id);
			return false;
		}
	}

	manager.dat_format = catalog.format;
	manager.item_count = catalog.item_count;
	manager.creature_count = catalog.creature_count;
	manager.is_extended = catalog.is_extended;
	manager.has_transparency = catalog.has_transparency;
	manager.has_frame_durations = catalog.has_frame_durations;
	manager.has_frame_groups = catalog.has_frame_groups;
	manager.sprite_archive_ = std::move(sprite_archive);
	manager.spritefile = manager.sprite_archive_->fileName();
	manager.unloaded = false;

	return true;
}
