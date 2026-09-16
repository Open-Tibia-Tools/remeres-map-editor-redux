//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "rendering/core/texture_garbage_collector.h"
#include "rendering/core/graphics.h"
#include "rendering/core/image.h"
#include <algorithm>
#include <chrono>

TextureGarbageCollector::TextureGarbageCollector() :
	loaded_textures(0),
	lastclean(0) {
}

TextureGarbageCollector::~TextureGarbageCollector() {
}

void TextureGarbageCollector::Clear() {
	loaded_textures = 0;
	lastclean = time(nullptr);
	resident_image_cursor = 0;
	resident_sprite_cursor = 0;
	sweep_in_progress = false;
}

void TextureGarbageCollector::NotifyTextureLoaded() {
	loaded_textures++;
}

void TextureGarbageCollector::NotifyTextureUnloaded() {
	loaded_textures--;
}

void TextureGarbageCollector::GarbageCollect(std::vector<GameSprite*>& resident_game_sprites, std::vector<void*>& resident_images, time_t current_time) {
	if (!options_.enabled) {
		sweep_in_progress = false;
		resident_image_cursor = 0;
		resident_sprite_cursor = 0;
		return;
	}

	const int clean_threshold = options_.clean_threshold;
	const int clean_pulse = options_.clean_pulse;
	if (!sweep_in_progress) {
		if (loaded_textures <= clean_threshold || current_time - lastclean <= clean_pulse) {
			return;
		}

		sweep_in_progress = true;
		resident_image_cursor = resident_images.size();
		resident_sprite_cursor = resident_game_sprites.size();
	}

	const int longevity = options_.longevity;
	const auto sweep_start = std::chrono::steady_clock::now();
	constexpr auto TIME_BUDGET = std::chrono::microseconds(1000);
	constexpr size_t CHECK_INTERVAL = 128;
	size_t items_since_budget_check = 0;

	const auto budget_exhausted = [&](bool force_check = false) {
		if (!force_check) {
			++items_since_budget_check;
			if (items_since_budget_check < CHECK_INTERVAL) {
				return false;
			}
		}

		items_since_budget_check = 0;
		return std::chrono::steady_clock::now() - sweep_start >= TIME_BUDGET;
	};

	while (resident_image_cursor > 0) {
		if (resident_image_cursor > resident_images.size()) {
			resident_image_cursor = resident_images.size();
		}
		if (resident_image_cursor == 0) {
			break;
		}

		--resident_image_cursor;
		Image* img = static_cast<Image*>(resident_images[resident_image_cursor]);
		img->clean(current_time, longevity);

		const bool evicted = !img->isGLLoaded;
		if (evicted) {
			if (resident_image_cursor < resident_images.size() - 1) {
				resident_images[resident_image_cursor] = resident_images.back();
			}

			resident_images.pop_back();
		}

		if (budget_exhausted(evicted)) {
			return;
		}
	}

	while (resident_sprite_cursor > 0) {
		if (resident_sprite_cursor > resident_game_sprites.size()) {
			resident_sprite_cursor = resident_game_sprites.size();
		}
		if (resident_sprite_cursor == 0) {
			break;
		}

		--resident_sprite_cursor;
		GameSprite* gs = resident_game_sprites[resident_sprite_cursor];
		gs->clean(current_time, longevity);

		if (budget_exhausted()) {
			return;
		}
	}

	sweep_in_progress = false;
	lastclean = current_time;
}
