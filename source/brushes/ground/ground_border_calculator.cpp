//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/auto_border.h"
#include "map/basemap.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "game/item.h"
#include <array>
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

namespace {

	// Emits the item id of every piece `border` places for one neighbour alignment,
	// in the classic generation order.
	template <typename Emit>
	void forEachBorderPiece(const AutoBorder& border, uint32_t border_alignment, Emit&& emit) {
		BorderType directions[4] = {
			static_cast<BorderType>((GroundBrush::border_types[border_alignment] & 0x000000FF) >> 0),
			static_cast<BorderType>((GroundBrush::border_types[border_alignment] & 0x0000FF00) >> 8),
			static_cast<BorderType>((GroundBrush::border_types[border_alignment] & 0x00FF0000) >> 16),
			static_cast<BorderType>((GroundBrush::border_types[border_alignment] & 0xFF000000) >> 24)
		};

		for (int32_t i = 0; i < 4; ++i) {
			BorderType direction = directions[i];
			if (direction == BORDER_NONE) {
				break;
			}

			if (border.tiles[direction] != 0) {
				emit(border.tiles[direction]);
			} else {
				struct DiagonalComponent {
					BorderType diagonal;
					BorderType h1;
					BorderType h2;
				};
				static constexpr std::array<DiagonalComponent, 4> diagonal_map = { {
					{ NORTHWEST_DIAGONAL, WEST_HORIZONTAL, NORTH_HORIZONTAL },
					{ NORTHEAST_DIAGONAL, EAST_HORIZONTAL, NORTH_HORIZONTAL },
					{ SOUTHWEST_DIAGONAL, SOUTH_HORIZONTAL, WEST_HORIZONTAL },
					{ SOUTHEAST_DIAGONAL, SOUTH_HORIZONTAL, EAST_HORIZONTAL },
				} };

				if (auto it = std::ranges::find_if(diagonal_map, [direction](const auto& d) { return d.diagonal == direction; }); it != diagonal_map.end()) {
					// A border without this diagonal falls back to its two straight
					// pieces, each one if defined (classic RME behaviour). Borders that
					// only define e/s, like the mountain wall, still get a piece at their
					// ne/sw ends instead of nothing.
					for (const BorderType side : { it->h1, it->h2 }) {
						if (border.tiles[side] != 0) {
							emit(border.tiles[side]);
						}
					}
				}
			}
		}
	}

	struct TileItemTraits {
		static bool isBorder(const std::unique_ptr<Item>& item) {
			return item->isBorder();
		}
		static bool isBorderLayer(const std::unique_ptr<Item>& item) {
			return item->isBorderLayer();
		}
		static uint32_t id(const std::unique_ptr<Item>& item) {
			return item->getID();
		}
		static void setId(std::unique_ptr<Item>& item, uint32_t id) {
			item->setID(id);
		}
		static uint32_t borderGroup(const std::unique_ptr<Item>& item) {
			return item->getBorderGroup();
		}
		static BorderType borderAlignment(const std::unique_ptr<Item>& item) {
			return item->getBorderAlignment();
		}
	};

} // namespace

void GroundBorderCalculator::calculate(BaseMap* map, Tile* tile, const BorderUpdate::Options& options) {
	static const auto extractGroundBrushFromTile = [](BaseMap* map, int x, int y, int z) -> GroundBrush* {
		Tile* tile = map->getTile(x, y, z);
		if (tile) {
			return tile->getGroundBrush();
		}
		return nullptr;
	};

	ASSERT(tile);

	GroundBrush* borderBrush;
	if (tile->ground) {
		borderBrush = tile->ground->getGroundBrush();
	} else {
		borderBrush = nullptr;
	}

	const Position& position = tile->getPosition();
	int x = position.x;
	int y = position.y;
	int z = position.z;

	// Pair of visited / what border type
	std::pair<bool, GroundBrush*> neighbours[8] = {
		{ false, nullptr }, { false, nullptr }, { false, nullptr }, { false, nullptr }, { false, nullptr }, { false, nullptr }, { false, nullptr }, { false, nullptr }
	};

	static constexpr std::array<std::pair<int32_t, int32_t>, 8> offsets = { { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } } };

	for (size_t i = 0; i < offsets.size(); ++i) {
		const auto& [dx, dy] = offsets[i];

		int nx = x + dx;
		int ny = y + dy;

		neighbours[i] = { false, extractGroundBrushFromTile(map, nx, ny, z) };
	}

	static std::vector<const GroundBrush::BorderBlock*> specificList;
	specificList.clear();

	std::vector<GroundBrush::BorderCluster> borderList;
	for (int32_t i = 0; i < 8; ++i) {
		auto& [visited, other] = neighbours[i];
		if (visited) {
			continue;
		}

		if (borderBrush) {
			if (other) {
				if (other->getID() == borderBrush->getID()) {
					continue;
				}

				if (other->hasOuterBorder() || borderBrush->hasInnerBorder()) {
					bool only_mountain = false;
					if (/*!borderBrush->hasInnerBorder() && */ (other->friendOf(borderBrush) || borderBrush->friendOf(other))) {
						if (!other->hasOptionalBorder()) {
							continue;
						}
						only_mountain = true;
					}

					uint32_t tiledata = 0;
					for (int32_t j = i; j < 8; ++j) {
						auto& [other_visited, other_brush] = neighbours[j];
						if (!other_visited && other_brush && other_brush->getID() == other->getID()) {
							other_visited = true;
							tiledata |= 1 << j;
						}
					}

					if (tiledata != 0) {
						// Add mountain if appropriate!
						if (other->hasOptionalBorder() && tile->hasOptionalBorder()) {
							GroundBrush::BorderCluster borderCluster;
							borderCluster.alignment = tiledata;
							borderCluster.z = 0x7FFFFFFF; // Above all other borders
							borderCluster.border = other->optional_border;

							borderList.push_back(borderCluster);
							if (other->useSoloOptionalBorder()) {
								only_mountain = true;
							}
						}

						if (!only_mountain) {
							const GroundBrush::BorderBlock* borderBlock = GroundBrush::getBrushTo(borderBrush, other);
							if (borderBlock) {
								bool found = false;
								for (GroundBrush::BorderCluster& borderCluster : borderList) {
									if (borderCluster.border == borderBlock->autoborder) {
										borderCluster.alignment |= tiledata;
										if (borderCluster.z < other->getZ()) {
											borderCluster.z = other->getZ();
										}

										if (!borderBlock->specific_cases.empty()) {
											specificList.push_back(borderBlock);
										}

										found = true;
										break;
									}
								}

								if (!found) {
									GroundBrush::BorderCluster borderCluster;
									borderCluster.alignment = tiledata;
									borderCluster.z = other->getZ();
									borderCluster.border = borderBlock->autoborder;

									borderList.push_back(borderCluster);
									if (!borderBlock->specific_cases.empty()) {
										specificList.push_back(borderBlock);
									}
								}
							}
						}
					}
				}
				// Border against nothing (or undefined tile)
				uint32_t tiledata = 0;
				for (int32_t j = i; j < 8; ++j) {
					auto& [other_visited, other_brush] = neighbours[j];
					if (!other_visited && !other_brush) {
						other_visited = true;
						tiledata |= 1 << j;
					}
				}

				if (tiledata != 0) {
					const GroundBrush::BorderBlock* borderBlock = GroundBrush::getBrushTo(borderBrush, nullptr);
					if (borderBlock) {
						if (borderBlock->autoborder) {
							bool found = false;
							for (GroundBrush::BorderCluster& borderCluster : borderList) {
								if (borderCluster.border == borderBlock->autoborder) {
									borderCluster.alignment |= tiledata;
									borderCluster.z = -1000;
									found = true;
									break;
								}
							}

							if (!found) {
								GroundBrush::BorderCluster borderCluster;
								borderCluster.alignment = tiledata;
								borderCluster.z = -1000;
								borderCluster.border = borderBlock->autoborder;
								borderList.push_back(borderCluster);
							}
						}

						if (!borderBlock->specific_cases.empty()) {
							specificList.push_back(borderBlock);
						}
					}
				}
				continue;
			} else {
				// Border against nothing (or undefined tile)
				uint32_t tiledata = 0;
				for (int32_t j = i; j < 8; ++j) {
					auto& [other_visited, other_brush] = neighbours[j];
					if (!other_visited && !other_brush) {
						other_visited = true;
						tiledata |= 1 << j;
					}
				}

				if (tiledata != 0) {
					const GroundBrush::BorderBlock* borderBlock = GroundBrush::getBrushTo(borderBrush, nullptr);
					if (borderBlock) {
						if (borderBlock->autoborder) {
							bool found = false;
							for (GroundBrush::BorderCluster& borderCluster : borderList) {
								if (borderCluster.border == borderBlock->autoborder) {
									borderCluster.alignment |= tiledata;
									borderCluster.z = -1000;
									found = true;
									break;
								}
							}

							if (!found) {
								GroundBrush::BorderCluster borderCluster;
								borderCluster.alignment = tiledata;
								borderCluster.z = -1000;
								borderCluster.border = borderBlock->autoborder;
								borderList.push_back(borderCluster);
							}
						}

						if (!borderBlock->specific_cases.empty()) {
							specificList.push_back(borderBlock);
						}
					}
				}
				continue;
			}
		} else if (other && other->hasOuterZilchBorder()) {
			uint32_t tiledata = 0;
			for (int32_t j = i; j < 8; ++j) {
				auto& [other_visited, other_brush] = neighbours[j];
				if (!other_visited && other_brush && other_brush->getID() == other->getID()) {
					other_visited = true;
					tiledata |= 1 << j;
				}
			}

			if (tiledata != 0) {
				const GroundBrush::BorderBlock* borderBlock = GroundBrush::getBrushTo(nullptr, other);
				if (borderBlock) {
					if (borderBlock->autoborder) {
						bool found = false;
						for (GroundBrush::BorderCluster& borderCluster : borderList) {
							if (borderCluster.border == borderBlock->autoborder) {
								borderCluster.alignment |= tiledata;
								if (borderCluster.z < other->getZ()) {
									borderCluster.z = other->getZ();
								}
								found = true;
								break;
							}
						}

						if (!found) {
							GroundBrush::BorderCluster borderCluster;
							borderCluster.alignment = tiledata;
							borderCluster.z = other->getZ();
							borderCluster.border = borderBlock->autoborder;
							borderList.push_back(borderCluster);
						}
					}

					if (!borderBlock->specific_cases.empty()) {
						specificList.push_back(borderBlock);
					}
				}

				// Add mountain if appropriate!
				if (other->hasOptionalBorder() && tile->hasOptionalBorder()) {
					GroundBrush::BorderCluster borderCluster;
					borderCluster.alignment = tiledata;
					borderCluster.z = 0x7FFFFFFF; // Above all other borders
					borderCluster.border = other->optional_border;

					borderList.push_back(borderCluster);
				} else if (options.mayClearOptionalBorder()) {
					tile->setOptionalBorder(false);
				}
			}
		}
		// Check tile as done
		visited = true;
	}

	if (!options.isClassic()) {
		applyWithOptions(tile, borderBrush, neighbours, borderList, specificList, options);
		return;
	}

	// Clean current borders
	std::erase_if(tile->items, [](const std::unique_ptr<Item>& item) {
		return item->isBorder();
	});

	// Sort borders based on z-order
	std::ranges::sort(borderList, [](const GroundBrush::BorderCluster& a, const GroundBrush::BorderCluster& b) {
		return a.z < b.z;
	});

	std::ranges::sort(specificList);
	auto [first, last] = std::ranges::unique(specificList);
	specificList.erase(first, last);

	TileOperations::cleanBorders(tile);

	while (!borderList.empty()) {
		GroundBrush::BorderCluster& borderCluster = borderList.back();
		if (!borderCluster.border) {
			borderList.pop_back();
			continue;
		}

		forEachBorderPiece(*borderCluster.border, borderCluster.alignment, [tile](uint32_t item_id) {
			TileOperations::addBorderItem(tile, Item::Create(item_id));
		});

		borderList.pop_back();
	}

	for (const GroundBrush::BorderBlock* borderBlock : specificList) {
		for (const auto& specificCaseBlockPtr : borderBlock->specific_cases) {
			const GroundBrush::SpecificCaseBlock* specificCaseBlock = specificCaseBlockPtr.get();
			uint32_t matches = 0;
			for (const auto& item : tile->items) {
				if (!item->isBorder()) {
					break;
				}

				if (specificCaseBlock->match_group > 0) {
					if (item->getBorderGroup() == specificCaseBlock->match_group && item->getBorderAlignment() == specificCaseBlock->group_match_alignment) {
						++matches;
						continue;
					}
				}

				for (uint16_t matchId : specificCaseBlock->items_to_match) {
					if (item->getID() == matchId) {
						++matches;
					}
				}
			}

			if (matches >= specificCaseBlock->items_to_match.size()) {
				auto& tileItems = tile->items;
				auto it = tileItems.begin();

				// if delete_all mode, consider the border replaced
				bool replaced = specificCaseBlock->delete_all;

				while (it != tileItems.end()) {
					Item* item = it->get();
					if (!item->isBorder()) {
						++it;
						continue;
					}

					bool inc = true;
					for (uint16_t matchId : specificCaseBlock->items_to_match) {
						if (item->getID() == matchId) {
							if (!replaced && item->getID() == specificCaseBlock->to_replace_id) {
								// replace the matching border, delete everything else
								item->setID(specificCaseBlock->with_id);
								replaced = true;
							} else {
								if (specificCaseBlock->delete_all || !specificCaseBlock->keepBorder) {
									it = tileItems.erase(it);
									inc = false;
									break;
								}
							}
						}
					}

					if (inc) {
						++it;
					}
				}
			}
		}
	}
}

void GroundBorderCalculator::applyWithOptions(
	Tile* tile,
	const GroundBrush* tile_brush,
	std::span<const std::pair<bool, GroundBrush*>, 8> neighbours,
	std::vector<GroundBrush::BorderCluster>& borderList,
	std::vector<const GroundBrush::BorderBlock*>& specificList,
	const BorderUpdate::Options& options
) {
	const GroundBrush* active = options.preservesOtherBrushes() ? options.active_brush : nullptr;

	// Slot 0 is the active brush; the rest is the ownership of every other ground
	// brush that can justify a border on this tile: its own ground and its 8
	// neighbours. Item ids shared with one of them are kept.
	std::array<const BorderUpdate::BorderOwnership*, 10> owners {};
	std::size_t owner_count = 0;
	const auto addOwner = [&](const GroundBrush* brush) {
		if (!active || !brush) {
			return;
		}
		const BorderUpdate::BorderOwnership* ownership = &brush->getBorderOwnership();
		const auto owners_end = owners.begin() + owner_count;
		if (std::find(owners.begin(), owners_end, ownership) == owners_end) {
			owners[owner_count++] = ownership;
		}
	};
	addOwner(active);
	addOwner(tile_brush);
	for (const auto& neighbour : neighbours) {
		addOwner(neighbour.second);
	}
	const std::span<const BorderUpdate::BorderOwnership* const> active_and_context(owners.data(), owner_count);
	const std::span<const BorderUpdate::BorderOwnership* const> context = owner_count > 0 ? active_and_context.subspan(1) : active_and_context;

	// AllBrushes rewrites every brush border, like the classic path.
	const auto canRewrite = [active, context](uint32_t item_id) {
		return !active || BorderUpdate::IsRewritableByActive(item_id, active->getBorderOwnership(), context);
	};

	BorderUpdate::RemoveRewritableBorders<TileItemTraits>(tile->items, canRewrite);

	// Same ordering as the classic path.
	std::ranges::sort(borderList, [](const GroundBrush::BorderCluster& a, const GroundBrush::BorderCluster& b) {
		return a.z < b.z;
	});

	std::ranges::sort(specificList);
	auto [first, last] = std::ranges::unique(specificList);
	specificList.erase(first, last);

	// CurrentBrushOnly generates only the active brush's own autoborders; a seam
	// drawn with another brush's border is that brush's to recalculate.
	std::vector<std::unique_ptr<Item>> generated;
	while (!borderList.empty()) {
		const GroundBrush::BorderCluster& borderCluster = borderList.back();
		if (borderCluster.border && (!active || active->usesAutoBorder(borderCluster.border))) {
			forEachBorderPiece(*borderCluster.border, borderCluster.alignment, [&generated](uint32_t item_id) {
				if (std::unique_ptr<Item> item = Item::Create(item_id)) {
					ASSERT(item->isBorder());
					generated.push_back(std::move(item));
				}
			});
		}
		borderList.pop_back();
	}

	// The classic path front-inserts piece by piece, which stacks the set in reverse
	// generation order. Keep that order and splice the whole set at one boundary.
	std::ranges::reverse(generated);
	// A scoped update keeps other brushes' pieces, so it must not stack a piece
	// that a kept item already shows.
	BorderUpdate::InsertGenerated<TileItemTraits>(tile->items, generated, options.placement, [&](uint32_t item_id) {
		return active && BorderUpdate::IsRepresentedOnTile<TileItemTraits>(item_id, tile->items, active_and_context);
	});

	for (const GroundBrush::BorderBlock* borderBlock : specificList) {
		if (active && !active->ownsBorderBlock(borderBlock)) {
			continue;
		}
		for (const auto& specificCaseBlock : borderBlock->specific_cases) {
			BorderUpdate::ApplySpecificCase<TileItemTraits>(tile->items, *specificCaseBlock, canRewrite);
		}
	}

	TileOperations::update(tile);
}
