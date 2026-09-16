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

#ifndef RME_GRAPHICS_H_
#define RME_GRAPHICS_H_

#include "game/outfit.h"
#include "util/common.h"
#include <deque>
#include <memory>
#include <map>

struct NVGcontext;
struct NVGDeleter {
	void operator()(NVGcontext* nvg) const;
};

#include <unordered_map>
#include <list>
#include <vector>

#include "app/client_version.h"

#include "animator.h"

class MapCanvas;
class GraphicManager;
class FileReadHandle;
class Animator;
class SpriteArchive;

#include "rendering/core/sprite_light.h"
#include "rendering/core/texture_garbage_collector.h"
#include "rendering/core/render_timer.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/image.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/template_image.h"

class GraphicManager {
public:
	GraphicManager();
	~GraphicManager();

	void clear();
	void cleanSoftwareSprites() {}

	Sprite* getSprite(int id);
	void updateTime();

	void pauseAnimation() {
		animation_timer->Pause();
	}
	void resumeAnimation() {
		animation_timer->Resume();
	}
	[[nodiscard]] GameSprite* getGameSprite(int id);
	GameSprite* getCreatureSprite(int id);
	void insertSprite(int id, std::unique_ptr<Sprite> sprite);
	// Overload for compatibility with existing raw pointer calls (takes ownership)
	void insertSprite(int id, Sprite* sprite) {
		insertSprite(id, std::unique_ptr<Sprite>(sprite));
	}

	long getElapsedTime() const {
		return animation_timer->getElapsedTime();
	}

	time_t getCachedTime() const {
		return cached_time_;
	}

	uint16_t getItemSpriteMaxID() const;
	uint16_t getCreatureSpriteMaxID() const;

	// Cleans old & unused textures according to config settings
	void garbageCollection();
	void setGCOptions(const TextureGCOptions& options) {
		collector.SetOptions(options);
	}
	[[nodiscard]] const TextureGCOptions& getGCOptions() const {
		return collector.GetOptions();
	}

	std::filesystem::path getMetadataPath() const {
		return client_version ? std::filesystem::path(client_version->getMetadataPath().GetFullPath().ToStdString()) : std::filesystem::path();
	}
	std::filesystem::path getSpritesPath() const {
		return client_version ? std::filesystem::path(client_version->getSpritesPath().GetFullPath().ToStdString()) : std::filesystem::path();
	}

	bool hasTransparency() const;
	bool isUnloaded() const;

	const std::string& getSpriteFile() const {
		return spritefile;
	}
	bool isExtended() const {
		return is_extended;
	}
	std::shared_ptr<SpriteArchive> getSpriteArchive() const {
		return sprite_archive_;
	}

	ClientVersion* client_version;

	// Sprite Atlas (Phase 2) - manages all game sprites in a texture array
	AtlasManager* getAtlasManager() {
		return atlas_manager_.get();
	}
	bool hasAtlasManager() const {
		return atlas_manager_ != nullptr && atlas_manager_->isValid();
	}
	// Lazy initialization of atlas
	bool ensureAtlasManager();

private:
	std::atomic<bool> unloaded;
	std::string spritefile;
	std::shared_ptr<SpriteArchive> sprite_archive_;

	// Atlas manager for Phase 2 texture array rendering
	std::unique_ptr<AtlasManager> atlas_manager_ = nullptr;

	// These are indexed by ID for O(1) access
	using SpriteVector = std::vector<std::unique_ptr<Sprite>>;
	SpriteVector sprite_space;
	using ImageVector = std::vector<std::unique_ptr<Image>>;
	ImageVector image_space;

	// Active Resident Sets: Track only what's currently occupying memory/VRAM
	// This avoids O(N) scans of the entire database.
	std::vector<void*> resident_images;
	std::vector<GameSprite*> resident_game_sprites;

	DatFormat dat_format;
	uint16_t item_count;
	uint16_t creature_count;
	bool is_extended;
	bool has_transparency;
	bool has_frame_durations;
	bool has_frame_groups;
	TextureGarbageCollector collector;

	std::unique_ptr<RenderTimer> animation_timer;
	time_t cached_time_ = 0;

	friend class Image;
	friend class NormalImage;
	friend class TemplateImage;
	friend class SpritePreloader;
	friend class GraphicsAssembler;
};

#include "minimap_colors.h"

extern GraphicManager g_graphics;

#endif
