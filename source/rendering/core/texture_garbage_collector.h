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

#ifndef RME_TEXTURE_GARBAGE_COLLECTOR_H
#define RME_TEXTURE_GARBAGE_COLLECTOR_H

#include <deque>
#include <vector>
#include <memory>
#include <time.h>

class GameSprite;
class Sprite;

struct TextureGCOptions {
	bool enabled = true;
	int clean_threshold = 1000;
	int clean_pulse = 60;
	int longevity = 300;
};

class TextureGarbageCollector {
public:
	TextureGarbageCollector();
	~TextureGarbageCollector();

	void SetOptions(const TextureGCOptions& options) noexcept {
		options_ = options;
	}
	[[nodiscard]] const TextureGCOptions& GetOptions() const noexcept {
		return options_;
	}

	void GarbageCollect(std::vector<GameSprite*>& resident_game_sprites, std::vector<void*>& resident_images, time_t current_time);
	void Clear();

	void NotifyTextureLoaded();
	void NotifyTextureUnloaded();

	int GetLoadedTexturesCount() const {
		return loaded_textures;
	}

private:
	TextureGCOptions options_;
	int loaded_textures;
	time_t lastclean;
	size_t resident_image_cursor = 0;
	size_t resident_sprite_cursor = 0;
	bool sweep_in_progress = false;
};

#endif
