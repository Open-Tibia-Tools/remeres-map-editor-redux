#include "rendering/core/graphics_assembler.h"

#include "item_definitions/formats/dat/dat_catalog.h"
#include "rendering/core/animator.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/sprite_archive.h"
#include "rendering/core/sprite_preloader.h"

#include <algorithm>
#include <format>
#include <memory>
#include <wx/string.h>

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

	bool validateCatalog(const DatCatalog& catalog, wxString& error) {
		if (catalog.max_sprite_id >= MAX_SPRITES) {
			error = wxString::FromUTF8(std::format(
				"DAT catalog references sprite id {} which exceeds MAX_SPRITES={}.",
				catalog.max_sprite_id,
				MAX_SPRITES));
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
	if (!entry.animation.has_value()) {
		sprite.animator.reset();
		return;
	}

	const auto& animation = *entry.animation;
	sprite.animator = std::make_unique<Animator>(entry.frames, animation.start_frame, animation.loop_count, animation.asynchronous);
	for (size_t frame_index = 0; frame_index < animation.frame_durations.size(); ++frame_index) {
		FrameDuration* duration = sprite.animator->getFrameDuration(static_cast<int>(frame_index));
		duration->setValues(static_cast<int>(animation.frame_durations[frame_index].minimum), static_cast<int>(animation.frame_durations[frame_index].maximum));
	}
	sprite.animator->reset();
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
	manager.resident_images.clear();
	manager.resident_game_sprites.clear();
	manager.collector.Clear();
	if (manager.atlas_manager_) {
		manager.atlas_manager_->clear();
		manager.atlas_manager_.reset();
	}
}

bool GraphicsAssembler::install(GraphicManager& manager, const DatCatalog& catalog, std::shared_ptr<SpriteArchive> sprite_archive, wxString& error, std::vector<std::string>& warnings) {
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
			error = wxString::FromUTF8(std::format("Failed to install graphics for client id {}.", entry.client_id));
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
