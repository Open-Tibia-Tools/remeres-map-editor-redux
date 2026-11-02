#include "rendering/ui/selection_picker.h"

#include "app/settings.h"
#include "editor/editor.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/outfit.h"
#include "game/spawn.h"
#include "map/map.h"
#include "map/position.h"
#include "map/tile.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "rendering/core/image.h"
#include "rendering/ui/selection_geometry.h"
#include "rendering/utilities/pattern_calculator.h"
#include "ui/gui.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace {

	constexpr int kPixelSelectionSearchRange = 4;
	constexpr uint8_t kPixelSelectionAlphaThreshold = 32;

	enum class PixelTestResult : uint8_t {
		Miss,
		Hit,
		Unknown,
	};

	struct ItemRenderInfo {
		Item* item = nullptr;
		int elevation = 0;
		int sublayer = 0;
		int order = 0;
	};

	bool BoxHit(int pixel_x, int pixel_y, int left, int top, int width, int height) {
		return pixel_x >= left && pixel_x < left + width && pixel_y >= top && pixel_y < top + height;
	}

	PixelTestResult SampleSpriteAlpha(const GameSprite& sprite, size_t index, int pixel_x, int pixel_y) {
		if (index >= sprite.spriteList.size()) {
			return PixelTestResult::Unknown;
		}

		Image* image = sprite.spriteList[index];
		if (!image) {
			return PixelTestResult::Unknown;
		}

		std::unique_ptr<uint8_t[]> rgba = image->getRGBAData();
		if (!rgba) {
			return PixelTestResult::Unknown;
		}

		const ImageDimensions dimensions = image->getDimensions();
		if (pixel_x < 0 || pixel_y < 0 || pixel_x >= dimensions.width || pixel_y >= dimensions.height) {
			return PixelTestResult::Miss;
		}

		const size_t offset = (static_cast<size_t>(pixel_y) * dimensions.width + static_cast<size_t>(pixel_x)) * 4;
		return rgba[offset + 3] > kPixelSelectionAlphaThreshold ? PixelTestResult::Hit : PixelTestResult::Miss;
	}

	template <typename IndexProvider>
	PixelTestResult HitTestSpriteParts(const GameSprite& sprite, const SpriteLayoutMetrics& metrics, int base_x, int base_y, int pixel_x, int pixel_y, IndexProvider&& index_provider) {
		const std::optional<SelectionSpritePart> part = FindSelectionSpritePart(pixel_x, pixel_y, base_x, base_y, metrics);
		if (!part.has_value()) {
			return PixelTestResult::Miss;
		}

		bool unavailable = false;
		const int layers = std::max<int>(1, sprite.layers);
		for (int layer = 0; layer < layers; ++layer) {
			const PixelTestResult result = SampleSpriteAlpha(sprite, index_provider(part->column, part->row, layer), part->local_x, part->local_y);
			if (result == PixelTestResult::Hit) {
				return PixelTestResult::Hit;
			}
			if (result == PixelTestResult::Unknown) {
				unavailable = true;
			}
		}
		return unavailable ? PixelTestResult::Unknown : PixelTestResult::Miss;
	}

	PixelTestResult HitTestItem(const Item& item, const Tile& tile, const Position& tile_pos, int pixel_x, int pixel_y, int elevation) {
		if (item.isMetaItem()) {
			return PixelTestResult::Miss;
		}

		const ItemDefinitionView definition = item.getDefinition();
		GameSprite* sprite = item.getSprite();
		if (!definition || !sprite) {
			return BoxHit(pixel_x, pixel_y, 0, 0, TILE_SIZE, TILE_SIZE) ? PixelTestResult::Hit : PixelTestResult::Miss;
		}

		SpritePatterns patterns = PatternCalculator::Calculate(sprite, definition, &item, &tile, tile_pos, g_gui.gfx.getElapsedTime());
		const SpriteLayoutMetrics metrics = sprite->getPlainLayoutMetrics(
			patterns.subtype.value_or(-1),
			patterns.x,
			patterns.y,
			patterns.z,
			patterns.frame
		);

		const auto [draw_offset_x, draw_offset_y] = sprite->getDrawOffset();
		const int base_x = -elevation - draw_offset_x;
		const int base_y = -elevation - draw_offset_y;

		const PixelTestResult result = HitTestSpriteParts(
			*sprite,
			metrics,
			base_x,
			base_y,
			pixel_x,
			pixel_y,
			[&](int cx, int cy, int layer) {
				return sprite->getIndex(cx, cy, layer, patterns.x, patterns.y, patterns.z, patterns.frame);
			}
		);

		if (result == PixelTestResult::Unknown) {
			return PixelTestResult::Hit;
		}
		return result;
	}

	PixelTestResult HitTestOutfitSprite(GameSprite& sprite, int direction, int addon, int pattern_z, int frame, int pixel_x, int pixel_y) {
		const SpriteLayoutMetrics metrics = sprite.getOutfitLayoutMetrics(direction, addon, pattern_z, frame);
		const auto [draw_offset_x, draw_offset_y] = sprite.getDrawOffset();
		const int base_x = -draw_offset_x;
		const int base_y = -draw_offset_y;

		const PixelTestResult result = HitTestSpriteParts(
			sprite,
			metrics,
			base_x,
			base_y,
			pixel_x,
			pixel_y,
			[&](int cx, int cy, int) {
				return sprite.getIndex(cx, cy, 0, direction, addon, pattern_z, frame);
			}
		);

		if (result == PixelTestResult::Unknown) {
			return PixelTestResult::Hit;
		}
		return result;
	}

	PixelTestResult HitTestCreature(const Creature& creature, int pixel_x, int pixel_y) {
		const Outfit& outfit = creature.getLookType();
		if (outfit.lookItem != 0) {
			return BoxHit(pixel_x, pixel_y, 0, 0, TILE_SIZE, TILE_SIZE) ? PixelTestResult::Hit : PixelTestResult::Miss;
		}

		const Outfit* draw_outfit = &outfit;
		GameSprite* body = g_gui.gfx.getCreatureSprite(outfit.lookType);
		if (!body && outfit.lookType != DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType) {
			draw_outfit = &DEFAULT_UNKNOWN_CREATURE_OUTFIT;
			body = g_gui.gfx.getCreatureSprite(DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType);
		}
		if (!body) {
			return BoxHit(pixel_x, pixel_y, 0, 0, TILE_SIZE, TILE_SIZE) ? PixelTestResult::Hit : PixelTestResult::Miss;
		}

		const long elapsed_time = g_gui.gfx.getElapsedTime();
		const int direction = static_cast<int>(creature.getDirection());
		const int body_frame = body->isAnimated() ? body->getAnimationFrame(elapsed_time) : 0;

		bool unavailable = false;

		int body_pattern_z = 0;
		if (draw_outfit->lookMount != 0) {
			if (GameSprite* mount = g_gui.gfx.getCreatureSprite(draw_outfit->lookMount)) {
				const int mount_frame = mount->isAnimated() ? mount->getAnimationFrame(elapsed_time) : 0;
				const PixelTestResult mount_result = HitTestOutfitSprite(*mount, direction, 0, 0, mount_frame, pixel_x, pixel_y);
				if (mount_result == PixelTestResult::Hit) {
					return PixelTestResult::Hit;
				}
				if (mount_result == PixelTestResult::Unknown) {
					unavailable = true;
				}
				body_pattern_z = body->pattern_z > 1 ? 1 : 0;
			}
		}

		const int addon_count = std::min<int>(std::max<int>(1, body->pattern_y), 32);
		for (int addon = 0; addon < addon_count; ++addon) {
			if (addon > 0 && (addon > 31 || (draw_outfit->lookAddon & (1 << (addon - 1))) == 0)) {
				continue;
			}

			const PixelTestResult result = HitTestOutfitSprite(*body, direction, addon, body_pattern_z, body_frame, pixel_x, pixel_y);
			if (result == PixelTestResult::Hit) {
				return PixelTestResult::Hit;
			}
			if (result == PixelTestResult::Unknown) {
				unavailable = true;
			}
		}

		return unavailable ? PixelTestResult::Unknown : PixelTestResult::Miss;
	}

	bool IsItemHiddenBySettings(const Item& item) {
		if (g_settings.getBoolean(Config::SHOW_ITEMS)) {
			return false;
		}
		return item.getDefinition().hasFlag(ItemFlag::Pickupable);
	}

	bool IsItemInvalidAndHidden(const Item& item) {
		return item.isInvalidOTBMItem() && !g_settings.getBoolean(Config::SHOW_INVALID_TILES);
	}

	bool IsItemUnresolvedAndHidden(const Item& item) {
		return !item.getDefinition() && !g_settings.getBoolean(Config::SHOW_INVALID_TILES);
	}

	std::vector<ItemRenderInfo> BuildItemRenderList(const Tile& tile) {
		std::vector<ItemRenderInfo> render_list;
		render_list.reserve(tile.items.size());

		int elevation = 0;
		int common_step = 0;
		int order = 0;
		for (const std::unique_ptr<Item>& item_ptr : tile.items) {
			Item* item = item_ptr.get();
			if (!item || item->isBorderLayer() || item->isMetaItem()) {
				continue;
			}
			if (IsItemHiddenBySettings(*item) || IsItemInvalidAndHidden(*item) || IsItemUnresolvedAndHidden(*item)) {
				continue;
			}

			const ItemDefinitionView definition = item->getDefinition();
			int sublayer = 0;
			if (item->isAlwaysOnBottom()) {
				sublayer = 3;
			} else if (definition.hasFlag(ItemFlag::TopEffect)) {
				sublayer = 10;
			} else {
				sublayer = 4 + std::min(common_step, 3);
				++common_step;
			}

			render_list.push_back(ItemRenderInfo {
				.item = item,
				.elevation = elevation,
				.sublayer = sublayer,
				.order = order++,
			});

			if (GameSprite* sprite = item->getSprite()) {
				elevation += sprite->draw_height;
			}
		}

		std::ranges::sort(render_list, [](const ItemRenderInfo& left, const ItemRenderInfo& right) {
			if (left.sublayer != right.sublayer) {
				return left.sublayer > right.sublayer;
			}
			return left.order > right.order;
		});

		return render_list;
	}

	SelectionHit MakeItemHit(Tile* tile, Item* item) {
		return SelectionHit {
			.kind = item == tile->ground.get() ? SelectionHitKind::Ground : SelectionHitKind::Item,
			.tile = tile,
			.item = item,
		};
	}

	SelectionHit FindHitOnTile(Tile& tile, const Position& tile_pos, int pixel_x, int pixel_y, bool is_target_tile) {
		const std::vector<ItemRenderInfo> render_list = BuildItemRenderList(tile);

		// TopEffect items (sublayer 10) render in front of creatures (sublayer 8)
		for (const ItemRenderInfo& info : render_list) {
			if (info.sublayer >= 10) {
				if (HitTestItem(*info.item, tile, tile_pos, pixel_x, pixel_y, info.elevation) == PixelTestResult::Hit) {
					return MakeItemHit(&tile, info.item);
				}
			}
		}

		if (tile.creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
			const PixelTestResult result = HitTestCreature(*tile.creature, pixel_x, pixel_y);
			if (result == PixelTestResult::Hit) {
				return SelectionHit {
					.kind = SelectionHitKind::Creature,
					.tile = &tile,
					.creature = tile.creature.get(),
				};
			}
		}

		// Common and bottom items
		for (const ItemRenderInfo& info : render_list) {
			if (info.sublayer < 10) {
				if (HitTestItem(*info.item, tile, tile_pos, pixel_x, pixel_y, info.elevation) == PixelTestResult::Hit) {
					return MakeItemHit(&tile, info.item);
				}
			}
		}

		if (is_target_tile && tile.spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
			return SelectionHit {
				.kind = SelectionHitKind::Spawn,
				.tile = &tile,
				.spawn = tile.spawn.get(),
			};
		}

		for (auto it = tile.items.rbegin(); it != tile.items.rend(); ++it) {
			Item* item = it->get();
			if (!item || !item->isBorderLayer() || item->isMetaItem()) {
				continue;
			}
			if (IsItemHiddenBySettings(*item) || IsItemInvalidAndHidden(*item) || IsItemUnresolvedAndHidden(*item)) {
				continue;
			}
			if (HitTestItem(*item, tile, tile_pos, pixel_x, pixel_y, 0) == PixelTestResult::Hit) {
				return MakeItemHit(&tile, item);
			}
		}

		if (is_target_tile && tile.ground && !IsItemInvalidAndHidden(*tile.ground) && !IsItemUnresolvedAndHidden(*tile.ground)) {
			if (HitTestItem(*tile.ground, tile, tile_pos, pixel_x, pixel_y, 0) == PixelTestResult::Hit) {
				return MakeItemHit(&tile, tile.ground.get());
			}
		}

		return {};
	}

	SelectionHit PickStandard(Editor& editor, const Position& clicked_pos) {
		Tile* tile = editor.map.getTile(clicked_pos);
		if (!tile) {
			return {};
		}

		if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
			return SelectionHit {
				.kind = SelectionHitKind::Spawn,
				.tile = tile,
				.spawn = tile->spawn.get(),
			};
		}

		if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
			return SelectionHit {
				.kind = SelectionHitKind::Creature,
				.tile = tile,
				.creature = tile->creature.get(),
			};
		}

		if (Item* item = tile->getTopItem()) {
			return MakeItemHit(tile, item);
		}

		return {};
	}

	SelectionHit PickPixelPerfect(Editor& editor, const Position& clicked_pos, const SelectionPixelOffset& pixel) {
		if (g_gui.gfx.isUnloaded() || !g_gui.gfx.getSpriteArchive()) {
			return PickStandard(editor, clicked_pos);
		}

		for (int dy = kPixelSelectionSearchRange; dy >= 0; --dy) {
			for (int dx = kPixelSelectionSearchRange; dx >= 0; --dx) {
				Position storage_pos(clicked_pos.x + dx, clicked_pos.y + dy, clicked_pos.z);
				Tile* tile = editor.map.getTile(storage_pos);
				if (!tile) {
					continue;
				}

				const SelectionPixelOffset local {
					.x = pixel.x - dx * TILE_SIZE,
					.y = pixel.y - dy * TILE_SIZE,
				};

				SelectionHit hit = FindHitOnTile(*tile, storage_pos, local.x, local.y, dx == 0 && dy == 0);
				if (hit.HasEntity()) {
					return hit;
				}
			}
		}

		return {};
	}

} // namespace

SelectionMethod SelectionPicker::ActiveMethod() {
	return g_settings.getBoolean(Config::PIXEL_PERFECT_SELECTION) ? SelectionMethod::PixelPerfect : SelectionMethod::Standard;
}

SelectionHit SelectionPicker::Pick(Editor& editor, const Position& clicked_pos, const SelectionPixelOffset& pixel, SelectionMethod method) {
	if (method == SelectionMethod::PixelPerfect) {
		return PickPixelPerfect(editor, clicked_pos, pixel);
	}
	return PickStandard(editor, clicked_pos);
}
