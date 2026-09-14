//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/icons/sprite_icon_service.h"
#include "app/settings.h"
#include "rendering/core/graphics.h"
#include "rendering/core/template_image.h"
#include "rendering/core/editor_sprite.h"
#include <wx/dcmemory.h>
#include <algorithm>
#include <ranges>
#include <span>

wxBitmap SpriteIconService::Generate(GameSprite* sprite, SpriteSize size, bool rescale) {
	ASSERT(sprite->width >= 1 && sprite->height >= 1);

	const int bgshade = g_settings.getInteger(Config::ICON_BACKGROUND);

	const auto layout_metrics = sprite->getPlainLayoutMetrics(-1, 0, 0, 0, 0);
	int image_size = std::max(layout_metrics.total_width, layout_metrics.total_height);
	wxImage image(image_size, image_size);
	image.Create(image_size, image_size);
	image.InitAlpha();

	unsigned char r = (bgshade >> 16) & 0xFF;
	unsigned char g = (bgshade >> 8) & 0xFF;
	unsigned char b = bgshade & 0xFF;
	unsigned char* rawData = image.GetData();
	unsigned char* rawAlpha = image.GetAlpha();
	int count = image_size * image_size;

	std::span<unsigned char> bgData(rawData, static_cast<size_t>(count) * 3);
	std::span<unsigned char> alphaData(rawAlpha, count);

	for (int i : std::views::iota(0, count)) {
		bgData[i * 3 + 0] = r;
		bgData[i * 3 + 1] = g;
		bgData[i * 3 + 2] = b;
	}
	std::ranges::fill(alphaData, 255);

	for (uint8_t l = 0; l < sprite->layers; l++) {
		for (uint8_t w = 0; w < sprite->width; w++) {
			for (uint8_t h = 0; h < sprite->height; h++) {
				const int i = sprite->getIndex(w, h, l, 0, 0, 0, 0);
				std::unique_ptr<uint8_t[]> data = sprite->spriteList[i]->getRGBData();
				if (data) {
					const auto dimensions = sprite->spriteList[i]->getDimensions();
					wxImage img(dimensions.width, dimensions.height, data.get(), true);
					img.SetMaskColour(0xFF, 0x00, 0xFF);
					int x_offset = 0;
					for (int column = w + 1; column < layout_metrics.num_columns; ++column) {
						x_offset += layout_metrics.column_widths[column];
					}
					int y_offset = 0;
					for (int row = h + 1; row < layout_metrics.num_rows; ++row) {
						y_offset += layout_metrics.row_heights[row];
					}
					image.Paste(img, x_offset, y_offset);
				}
			}
		}
	}

	// Now comes the resizing / antialiasing
	if (rescale && (size == SPRITE_SIZE_16x16 || size == SPRITE_SIZE_64x64 || image.GetWidth() > SPRITE_PIXELS || image.GetHeight() > SPRITE_PIXELS)) {
		int new_size = 32;
		if (size == SPRITE_SIZE_16x16) {
			new_size = 16;
		} else if (size == SPRITE_SIZE_64x64) {
			new_size = 64;
		}
		image.Rescale(new_size, new_size, wxIMAGE_QUALITY_HIGH);
	}

	return wxBitmap(image);
}

wxBitmap SpriteIconService::Generate(GameSprite* sprite, SpriteSize size, const Outfit& outfit, bool rescale, Direction direction) {
	ASSERT(sprite->width >= 1 && sprite->height >= 1);

	const int bgshade = g_settings.getInteger(Config::ICON_BACKGROUND);

	int frame_index = 0;
	if (sprite->pattern_x == 4) {
		frame_index = direction;
	}

	int pattern_z = 0;
	GameSprite* mountSpr = outfit.lookMount != 0 ? g_graphics.getCreatureSprite(outfit.lookMount) : nullptr;
	if (mountSpr) {
		pattern_z = std::min<int>(1, sprite->pattern_z - 1);
	}

	int min_x = 0;
	int min_y = 0;
	int max_x = 0;
	int max_y = 0;
	const auto includeMetrics = [&](const GameSprite::SpriteLayoutMetrics& metrics, int offset_x = 0, int offset_y = 0) {
		min_x = std::min(min_x, offset_x);
		min_y = std::min(min_y, offset_y);
		max_x = std::max(max_x, offset_x + metrics.total_width);
		max_y = std::max(max_y, offset_y + metrics.total_height);
	};

	for (int selected_pattern = 0; selected_pattern < sprite->pattern_y; ++selected_pattern) {
		if (selected_pattern > 0 && (selected_pattern - 1 >= 31 || !(outfit.lookAddon & (1 << (selected_pattern - 1))))) {
			continue;
		}
		includeMetrics(sprite->getOutfitLayoutMetrics(static_cast<int>(direction), selected_pattern, pattern_z, frame_index));
	}
	if (mountSpr) {
		const auto mount_metrics = mountSpr->getOutfitLayoutMetrics(static_cast<int>(direction), 0, 0, mountSpr->pattern_x == 4 ? direction : 0);
		includeMetrics(mount_metrics, -mountSpr->getDrawOffset().first, -mountSpr->getDrawOffset().second);
	}
	const int image_width = max_x - min_x;
	const int image_height = max_y - min_y;
	const int image_size = std::max(image_width, image_height);
	wxImage image(image_size, image_size);
	image.Create(image_size, image_size);
	image.InitAlpha();

	unsigned char r = (bgshade >> 16) & 0xFF;
	unsigned char g = (bgshade >> 8) & 0xFF;
	unsigned char b = bgshade & 0xFF;
	unsigned char* rawData = image.GetData();
	unsigned char* rawAlpha = image.GetAlpha();
	int count = image_size * image_size;

	std::span<unsigned char> bgData(rawData, static_cast<size_t>(count) * 3);
	std::span<unsigned char> alphaData(rawAlpha, count);

	for (int i : std::views::iota(0, count)) {
		bgData[i * 3 + 0] = r;
		bgData[i * 3 + 1] = g;
		bgData[i * 3 + 2] = b;
	}
	std::ranges::fill(alphaData, 255);

	// Mounts
	if (mountSpr) {
		Outfit mountOutfit;
		mountOutfit.lookType = outfit.lookMount;
		mountOutfit.lookHead = outfit.lookMountHead;
		mountOutfit.lookBody = outfit.lookMountBody;
		mountOutfit.lookLegs = outfit.lookMountLegs;
		mountOutfit.lookFeet = outfit.lookMountFeet;

		int mount_frame_index = 0;
		if (mountSpr->pattern_x == 4) {
			mount_frame_index = direction;
		}

		for (uint8_t l = 0; l < mountSpr->layers; l++) {
			for (uint8_t w = 0; w < mountSpr->width; w++) {
				for (uint8_t h = 0; h < mountSpr->height; h++) {
					std::unique_ptr<uint8_t[]> data = nullptr;
					ImageDimensions dimensions {};

					if (mountSpr->layers == 2) {
						if (l == 1) {
							continue;
						}

						auto* image_ptr = mountSpr->getTemplateImage(mountSpr->getIndex(w, h, 0, mount_frame_index, 0, 0, 0), mountOutfit);
						dimensions = image_ptr->getDimensions();
						data = image_ptr->getRGBData();
					} else {
						const int sprite_index = mountSpr->getIndex(w, h, l, mount_frame_index, 0, 0, 0);
						dimensions = mountSpr->spriteList[sprite_index]->getDimensions();
						data = mountSpr->spriteList[sprite_index]->getRGBData();
					}

					if (data) {
						wxImage img(dimensions.width, dimensions.height, data.get(), true);
						img.SetMaskColour(0xFF, 0x00, 0xFF);
						const auto mount_metrics = mountSpr->getOutfitLayoutMetrics(static_cast<int>(direction), 0, 0, mount_frame_index);
						int mount_x = 0;
						for (int column = w + 1; column < mount_metrics.num_columns; ++column) {
							mount_x += mount_metrics.column_widths[column];
						}
						int mount_y = 0;
						for (int row = h + 1; row < mount_metrics.num_rows; ++row) {
							mount_y += mount_metrics.row_heights[row];
						}
						mount_x -= mountSpr->getDrawOffset().first;
						mount_y -= mountSpr->getDrawOffset().second;
						mount_x -= min_x;
						mount_y -= min_y;
						image.Paste(img, mount_x, mount_y);
					}
				}
			}
		}
	}

	for (int pattern_y = 0; pattern_y < sprite->pattern_y; pattern_y++) {
		if (pattern_y > 0) {
			if ((pattern_y - 1 >= 31) || !(outfit.lookAddon & (1 << (pattern_y - 1)))) {
				continue;
			}
		}

		for (uint8_t l = 0; l < sprite->layers; l++) {
			for (uint8_t w = 0; w < sprite->width; w++) {
				for (uint8_t h = 0; h < sprite->height; h++) {
					std::unique_ptr<uint8_t[]> data = nullptr;
					ImageDimensions dimensions {};

					if (sprite->layers == 2) {
						if (l == 1) {
							continue;
						}

						auto* image_ptr = sprite->getTemplateImage(sprite->getIndex(w, h, 0, frame_index, pattern_y, pattern_z, 0), outfit);
						dimensions = image_ptr->getDimensions();
						data = image_ptr->getRGBData();
					} else if (sprite->layers == 4) {
						if (l == 1 || l == 3) {
							continue;
						}
						if (l == 0) {
							auto* image_ptr = sprite->getTemplateImage(sprite->getIndex(w, h, 0, frame_index, pattern_y, pattern_z, 0), outfit);
							dimensions = image_ptr->getDimensions();
							data = image_ptr->getRGBData();
						}
						if (l == 2) {
							auto* image_ptr = sprite->getTemplateImage(sprite->getIndex(w, h, 2, frame_index, pattern_y, pattern_z, 0), outfit);
							dimensions = image_ptr->getDimensions();
							data = image_ptr->getRGBData();
						}
					} else {
						const int sprite_index = sprite->getIndex(w, h, l, frame_index, pattern_y, pattern_z, 0);
						dimensions = sprite->spriteList[sprite_index]->getDimensions();
						data = sprite->spriteList[sprite_index]->getRGBData();
					}

					if (data) {
						wxImage img(dimensions.width, dimensions.height, data.get(), true);
						img.SetMaskColour(0xFF, 0x00, 0xFF);
						const auto pattern_metrics = sprite->getOutfitLayoutMetrics(static_cast<int>(direction), pattern_y, pattern_z, 0);
						int x_offset = 0;
						for (int column = w + 1; column < pattern_metrics.num_columns; ++column) {
							x_offset += pattern_metrics.column_widths[column];
						}
						int y_offset = 0;
						for (int row = h + 1; row < pattern_metrics.num_rows; ++row) {
							y_offset += pattern_metrics.row_heights[row];
						}
						image.Paste(img, x_offset - min_x, y_offset - min_y);
					}
				}
			}
		}
	}

	// Now comes the resizing / antialiasing
	if (rescale && (size == SPRITE_SIZE_16x16 || size == SPRITE_SIZE_64x64 || image.GetWidth() > SPRITE_PIXELS || image.GetHeight() > SPRITE_PIXELS)) {
		int new_size = 32;
		if (size == SPRITE_SIZE_16x16) {
			new_size = 16;
		} else if (size == SPRITE_SIZE_64x64) {
			new_size = 64;
		}
		image.Rescale(new_size, new_size, wxIMAGE_QUALITY_HIGH);
	}

	return wxBitmap(image);
}

void SpriteIconService::DrawTo(Sprite* sprite, wxDC* dc, SpriteSize sz, int start_x, int start_y, int width, int height) {
	if (!sprite || !dc) {
		return;
	}

	const int sprite_dim = (sz == SPRITE_SIZE_64x64) ? 64 : (sz == SPRITE_SIZE_32x32 ? 32 : 16);
	int src_width = (width == -1) ? sprite_dim : width;
	int src_height = (height == -1) ? sprite_dim : height;

	if (auto* es = dynamic_cast<EditorSprite*>(sprite)) {
		wxBitmap* bmp = es->getBitmap(sz);
		if (bmp && bmp->IsOk()) {
			dc->DrawBitmap(*bmp, start_x, start_y, true);
		}
		return;
	}

	if (auto* cs = dynamic_cast<CreatureSprite*>(sprite)) {
		if (cs->parent) {
			DrawTo(cs->parent, dc, sz, cs->outfit, start_x, start_y, width, height);
		}
		return;
	}

	if (auto* gs = dynamic_cast<GameSprite*>(sprite)) {
		wxBitmap bmp = Generate(gs, sz);
		if (bmp.IsOk()) {
			wxMemoryDC mdc(bmp);
			dc->StretchBlit(start_x, start_y, src_width, src_height, &mdc, 0, 0, bmp.GetWidth(), bmp.GetHeight(), wxCOPY, true);
		}
		return;
	}
}

void SpriteIconService::DrawTo(GameSprite* sprite, wxDC* dc, SpriteSize sz, const Outfit& outfit, int start_x, int start_y, int width, int height) {
	if (!sprite || !dc) {
		return;
	}

	const int sprite_dim = (sz == SPRITE_SIZE_64x64) ? 64 : (sz == SPRITE_SIZE_32x32 ? 32 : 16);
	int src_width = (width == -1) ? sprite_dim : width;
	int src_height = (height == -1) ? sprite_dim : height;

	wxBitmap bmp = Generate(sprite, sz, outfit);
	if (bmp.IsOk()) {
		wxMemoryDC mdc(bmp);
		dc->StretchBlit(start_x, start_y, src_width, src_height, &mdc, 0, 0, bmp.GetWidth(), bmp.GetHeight(), wxCOPY, true);
	}
}
