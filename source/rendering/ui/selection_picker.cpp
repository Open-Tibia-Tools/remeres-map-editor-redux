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
#include "rendering/core/item_render_rules.h"
#include "rendering/ui/pick_candidate_order.h"
#include "rendering/ui/selection_geometry.h"
#include "rendering/utilities/pattern_calculator.h"
#include "ui/gui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace {

	constexpr int kPixelSelectionSearchRange = 4;
	constexpr uint8_t kPixelSelectionAlphaThreshold = 32;

	// Front-to-back item sublayers. Ground and border are handled explicitly as well; the
	// spawn marker is checked together with BottomItem.
	constexpr std::array<int, 9> kPickDepthOrder = {
		static_cast<int>(rme::rendering::RenderSublayer::TopItem),
		static_cast<int>(rme::rendering::RenderSublayer::DynamicEntity),
		static_cast<int>(rme::rendering::RenderSublayer::CommonItem) + 3,
		static_cast<int>(rme::rendering::RenderSublayer::CommonItem) + 2,
		static_cast<int>(rme::rendering::RenderSublayer::CommonItem) + 1,
		static_cast<int>(rme::rendering::RenderSublayer::CommonItem),
		static_cast<int>(rme::rendering::RenderSublayer::BottomItem),
		static_cast<int>(rme::rendering::RenderSublayer::Border),
		static_cast<int>(rme::rendering::RenderSublayer::Ground),
	};

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

	PixelTestResult SampleSpriteAlpha(const GameSprite& sprite, size_t index, int pixel_x, int pixel_y, int radius = 0) {
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
		if (rgba[offset + 3] > kPixelSelectionAlphaThreshold) {
			return PixelTestResult::Hit;
		}

		if (radius > 0) {
			for (int r = 1; r <= radius; ++r) {
				for (int dy = -r; dy <= r; ++dy) {
					for (int dx = -r; dx <= r; ++dx) {
						if (std::abs(dx) != r && std::abs(dy) != r) continue;
						const int nx = pixel_x + dx;
						const int ny = pixel_y + dy;
						if (nx >= 0 && nx < dimensions.width && ny >= 0 && ny < dimensions.height) {
							const size_t noffset = (static_cast<size_t>(ny) * dimensions.width + static_cast<size_t>(nx)) * 4;
							if (rgba[noffset + 3] > kPixelSelectionAlphaThreshold) {
								return PixelTestResult::Hit;
							}
						}
					}
				}
			}
		}

		return PixelTestResult::Miss;
	}

	template <typename IndexProvider>
	PixelTestResult HitTestSpriteParts(const GameSprite& sprite, const SpriteLayoutMetrics& metrics, int base_x, int base_y, int pixel_x, int pixel_y, IndexProvider&& index_provider, int alpha_radius = 0) {
		const std::optional<SelectionSpritePart> part = FindSelectionSpritePart(pixel_x, pixel_y, base_x, base_y, metrics);
		if (!part.has_value()) {
			return PixelTestResult::Miss;
		}

		bool unavailable = false;
		const int layers = std::max<int>(1, sprite.layers);
		for (int layer = 0; layer < layers; ++layer) {
			const PixelTestResult result = SampleSpriteAlpha(sprite, index_provider(part->column, part->row, layer), part->local_x, part->local_y, alpha_radius);
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

		const int alpha_radius = item.isFluidContainer() ? 4 : 0;
		const PixelTestResult result = HitTestSpriteParts(
			*sprite,
			metrics,
			base_x,
			base_y,
			pixel_x,
			pixel_y,
			[&](int cx, int cy, int layer) {
				return sprite->getIndex(cx, cy, layer, patterns.x, patterns.y, patterns.z, patterns.frame);
			},
			alpha_radius
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

		const bool show_invalid = g_settings.getBoolean(Config::SHOW_INVALID_TILES);
		int elevation = 0;
		int common_step = 0;
		int order = 0;
		for (const std::unique_ptr<Item>& item_ptr : tile.items) {
			Item* item = item_ptr.get();
			if (!item || item->isBorderLayer()) {
				continue;
			}

			const ItemDefinitionView definition = item->getDefinition();
			const bool invalid = item->isInvalidOTBMItem();
			if (invalid && (!show_invalid || !definition)) {
				continue;
			}

			const rme::rendering::RenderSublayer layer = rme::rendering::ResolveItemSublayer(
				item->isAlwaysOnBottom(),
				definition && definition.hasFlag(ItemFlag::TopEffect)
			);
			int sublayer = static_cast<int>(layer);
			if (layer == rme::rendering::RenderSublayer::CommonItem) {
				sublayer += rme::rendering::CommonItemElevationStep(common_step);
				++common_step;
			}

			// Hidden items still consume a common-item stack slot, exactly like the
			// tile renderer advances its common counter before ItemDrawer filters.
			if (item->isMetaItem()) {
				continue;
			}
			if (IsItemHiddenBySettings(*item) || IsItemInvalidAndHidden(*item) || IsItemUnresolvedAndHidden(*item)) {
				continue;
			}

			render_list.push_back(ItemRenderInfo {
				.item = item,
				.elevation = elevation,
				.sublayer = sublayer,
				.order = order++,
			});

			if (GameSprite* sprite = item->getSprite()) {
				rme::rendering::AdvanceItemElevation(elevation, sprite->draw_height, sprite->hasElevation());
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

	// Candidate anchor tile cached for one pixel-perfect pick. The render list is
	// built lazily because most candidates are rejected without touching items.
	struct PickCandidate {
		int dx = 0;
		int dy = 0;
		Tile* tile = nullptr;
		Position position;
		int local_x = 0;
		int local_y = 0;
		bool render_list_built = false;
		std::vector<ItemRenderInfo> render_list;
	};

	SelectionHit FindGroundHitOnTile(Tile& tile, const Position& tile_pos, int pixel_x, int pixel_y) {
		// Tested on every candidate: a multi-tile ground sprite (e.g. mountain 919) extends up-left from its
		// anchor, so its visible part can cover the clicked tile while the anchor is a neighbour. HitTestItem()
		// misses for pixels outside the sprite parts, so 1x1 grounds of neighbouring candidates never match.
		if (tile.ground && !IsItemInvalidAndHidden(*tile.ground) && !IsItemUnresolvedAndHidden(*tile.ground)) {
			if (HitTestItem(*tile.ground, tile, tile_pos, pixel_x, pixel_y, 0) == PixelTestResult::Hit) {
				return MakeItemHit(&tile, tile.ground.get());
			}
		}
		return {};
	}

	// Tests one render-depth sublayer on one candidate tile. The caller walks
	// diagonals and sublayers front-to-back, so this function must only report a
	// hit for the requested depth.
	SelectionHit FindHitOnTileAtDepth(PickCandidate& candidate, int depth_sub) {
		if (!candidate.tile) {
			return {};
		}

		Tile& tile = *candidate.tile;
		const bool is_clicked_tile = candidate.dx == 0 && candidate.dy == 0;

		if (depth_sub == static_cast<int>(rme::rendering::RenderSublayer::Ground)) {
			return FindGroundHitOnTile(tile, candidate.position, candidate.local_x, candidate.local_y);
		}

		if (depth_sub == static_cast<int>(rme::rendering::RenderSublayer::DynamicEntity)) {
			if (tile.creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
				if (HitTestCreature(*tile.creature, candidate.local_x, candidate.local_y) == PixelTestResult::Hit) {
					return SelectionHit {
						.kind = SelectionHitKind::Creature,
						.tile = &tile,
						.creature = tile.creature.get(),
					};
				}
			}
			return {};
		}

		if (depth_sub == static_cast<int>(rme::rendering::RenderSublayer::Border)) {
			for (auto it = tile.items.rbegin(); it != tile.items.rend(); ++it) {
				Item* item = it->get();
				if (!item || !item->isBorderLayer() || item->isMetaItem()) {
					continue;
				}
				if (IsItemHiddenBySettings(*item) || IsItemInvalidAndHidden(*item) || IsItemUnresolvedAndHidden(*item)) {
					continue;
				}
				if (HitTestItem(*item, tile, candidate.position, candidate.local_x, candidate.local_y, 0) == PixelTestResult::Hit) {
					return MakeItemHit(&tile, item);
				}
			}
			return {};
		}

		const bool is_item_depth = depth_sub == static_cast<int>(rme::rendering::RenderSublayer::TopItem) ||
			depth_sub == static_cast<int>(rme::rendering::RenderSublayer::BottomItem) ||
			(depth_sub >= static_cast<int>(rme::rendering::RenderSublayer::CommonItem) &&
			 depth_sub < static_cast<int>(rme::rendering::RenderSublayer::CommonItem) + 4);
		if (!is_item_depth) {
			return {};
		}

		if (!candidate.render_list_built) {
			candidate.render_list = BuildItemRenderList(tile);
			candidate.render_list_built = true;
		}

		for (const ItemRenderInfo& info : candidate.render_list) {
			if (info.sublayer != depth_sub) {
				continue;
			}
			if (HitTestItem(*info.item, tile, candidate.position, candidate.local_x, candidate.local_y, info.elevation) == PixelTestResult::Hit) {
				return MakeItemHit(&tile, info.item);
			}
		}

		// Spawn markers render above items but below the border layer; preserve the
		// existing target-tile-only rule.
		if (depth_sub == static_cast<int>(rme::rendering::RenderSublayer::BottomItem) && is_clicked_tile &&
		    tile.spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
			return SelectionHit {
				.kind = SelectionHitKind::Spawn,
				.tile = &tile,
				.spawn = tile.spawn.get(),
			};
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

		constexpr int candidate_span = kPixelSelectionSearchRange + 1;
		std::array<PickCandidate, candidate_span * candidate_span> candidates;
		for (int dy = 0; dy <= kPixelSelectionSearchRange; ++dy) {
			for (int dx = 0; dx <= kPixelSelectionSearchRange; ++dx) {
				PickCandidate& candidate = candidates[dy * candidate_span + dx];
				candidate.dx = dx;
				candidate.dy = dy;
				candidate.position = Position(clicked_pos.x + dx, clicked_pos.y + dy, clicked_pos.z);
				candidate.tile = editor.map.getTile(candidate.position);
				candidate.local_x = pixel.x - dx * TILE_SIZE;
				candidate.local_y = pixel.y - dy * TILE_SIZE;
			}
		}

		// Front to back in render-depth order: diagonal first, then the effective
		// sublayer (TopEffect, creature, common 3..0, bottom, spawn, border, ground).
		// For a given depth the candidates use the chunk-cache painter tie order.
		for (int diagonal = PickCandidateOrder::MaxDiagonal(kPixelSelectionSearchRange); diagonal >= 0; --diagonal) {
			for (const int depth_sub : kPickDepthOrder) {
				SelectionHit hit;
				PickCandidateOrder::ForEachOnDiagonal(kPixelSelectionSearchRange, diagonal, [&](int dx, int dy) {
					if (hit.HasEntity()) {
						return;
					}
					hit = FindHitOnTileAtDepth(candidates[dy * candidate_span + dx], depth_sub);
				});
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
