#include "map/tile_operations.h"
#include "tile_serialization_otbm.h"

#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/house.h"
#include "io/otbm/item_serialization_otbm.h"
#include "io/otbm/fast_otbm_reader.h"
#include "item_definitions/core/item_definition_store.h"
#include "ui/gui.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace {
	constexpr uint32_t KNOWN_TILE_FLAG_MASK =
		TILESTATE_PROTECTIONZONE |
		TILESTATE_DEPRECATED |
		TILESTATE_NOPVP |
		TILESTATE_NOLOGOUT |
		TILESTATE_PVPZONE |
		TILESTATE_REFRESH;

	uint16_t decodeServerIdFromInlineBytes(const std::vector<uint8_t>& rawBytes) {
		if (rawBytes.size() < 3) {
			return 0;
		}
		return static_cast<uint16_t>(rawBytes[1] | (static_cast<uint16_t>(rawBytes[2]) << 8));
	}

	uint16_t decodeServerIdFromNodePayload(const PreservedOTBMNode& node) {
		if (node.rawPayload.size() < 3) {
			return 0;
		}
		return static_cast<uint16_t>(node.rawPayload[1] | (static_cast<uint16_t>(node.rawPayload[2]) << 8));
	}

	std::vector<uint8_t> copyRawBytes(std::string_view rawData, size_t beginOffset, size_t endOffset) {
		const size_t safeBegin = std::min(beginOffset, rawData.size());
		const size_t safeEnd = std::min(endOffset, rawData.size());
		if (safeBegin >= safeEnd) {
			return {};
		}

		return std::vector<uint8_t>(rawData.begin() + static_cast<std::ptrdiff_t>(safeBegin), rawData.begin() + static_cast<std::ptrdiff_t>(safeEnd));
	}

	PreservedOTBMNode capturePreservedNode(BinaryNode* node) {
		PreservedOTBMNode preserved;
		if (!node) {
			return preserved;
		}

		const std::string_view rawPayload = node->rawData();
		preserved.rawPayload.assign(rawPayload.begin(), rawPayload.end());

		for (BinaryNode* childNode : node->children()) {
			preserved.children.push_back(capturePreservedNode(childNode));
		}
		return preserved;
	}

	void writePreservedNode(NodeFileWriteHandle& writer, const PreservedOTBMNode& node) {
		if (node.rawPayload.empty()) {
			return;
		}

		writer.addNode(node.rawPayload.front());
		if (node.rawPayload.size() > 1) {
			writer.addRAW(node.rawPayload.data() + 1, node.rawPayload.size() - 1);
		}
		for (const auto& child : node.children) {
			writePreservedNode(writer, child);
		}
		writer.endNode();
	}

	void writeRawInlineBytes(NodeFileWriteHandle& writer, const std::vector<uint8_t>& rawBytes) {
		if (!rawBytes.empty()) {
			writer.addRAW(rawBytes.data(), rawBytes.size());
		}
	}

	bool hasResolvedDefinition(const std::unique_ptr<Item>& item) {
		return item && item->getDefinition();
	}

	std::unique_ptr<Item> createInvalidPlaceholder(uint16_t serverId) {
		return serverId != 0 ? Item::Create(serverId) : nullptr;
	}

	bool shouldTreatInlineItemAsGround(const Tile& tile) {
		return !tile.hasGround() && tile.items.empty();
	}
}

void TileSerializationOTBM::readTileArea(IOMapOTBM& iomap, Map& map, BinaryNode* mapNode) {
	uint16_t base_x, base_y;
	uint8_t base_z;
	if (!mapNode->getU16(base_x) || !mapNode->getU16(base_y) || !mapNode->getU8(base_z)) {
		return;
	}

	for (BinaryNode* tileNode : mapNode->children()) {
		uint8_t tile_type;
		if (!tileNode->getByte(tile_type)) {
			continue;
		}

		if (tile_type != OTBM_TILE && tile_type != OTBM_HOUSETILE) {
			continue;
		}

		uint8_t x_offset, y_offset;
		if (!tileNode->getU8(x_offset) || !tileNode->getU8(y_offset)) {
			continue;
		}

		const Position pos(base_x + x_offset, base_y + y_offset, base_z);

		if (map.getTile(pos)) {
			continue;
		}

		Tile* tile = map.createTile(pos.x, pos.y, pos.z);
		House* house = nullptr;

		if (tile_type == OTBM_HOUSETILE) {
			uint32_t house_id;
			if (tileNode->getU32(house_id) && house_id != 0) {
				house = map.houses.getHouse(house_id);
				if (!house) {
					auto new_house = std::make_unique<House>(map);
					house = new_house.get();
					new_house->setID(house_id);
					map.houses.addHouse(std::move(new_house));
				}
			}
		}

		uint8_t attribute;
		bool stop_attributes = false;
		while (!stop_attributes) {
			const size_t attributeOffset = tileNode->getReadOffset();
			if (!tileNode->getU8(attribute)) {
				break;
			}

			switch (attribute) {
				case OTBM_ATTR_TILE_FLAGS: {
					uint32_t flags = 0;
					if (tileNode->getU32(flags)) {
						tile->setMapFlags(flags);
						const uint32_t unknownBits = flags & ~KNOWN_TILE_FLAG_MASK;
						if (unknownBits != 0) {
							tile->recordUnknownMapFlags(flags, unknownBits);
						}
					}
					break;
				}
				case OTBM_ATTR_ITEM: {
					auto item = ItemSerializationOTBM::createFromStream(iomap, tileNode);
					const auto rawItemBytes = copyRawBytes(tileNode->rawData(), attributeOffset, tileNode->getReadOffset());
					if (hasResolvedDefinition(item)) {
						tile->addItem(std::move(item));
					} else {
						const bool treatAsGround = shouldTreatInlineItemAsGround(*tile);
						const uint16_t serverId = item ? item->getID() : decodeServerIdFromInlineBytes(rawItemBytes);
						if (!item) {
							item = createInvalidPlaceholder(serverId);
						}
						if (item) {
							item->setInvalidOTBMData(InvalidOTBMItemData {
								.kind = treatAsGround ? InvalidOTBMItemKind::MissingGround : InvalidOTBMItemKind::MissingItem,
								.rawInlineBytes = rawItemBytes,
							});
							tile->addItem(std::move(item));
						} else if (!rawItemBytes.empty()) {
							tile->addOpaqueTileAttribute(OpaqueTileAttributeRecord {
								.rawBytes = rawItemBytes,
							});
						}
					}
					break;
				}
				default: {
					tile->addOpaqueTileAttribute(OpaqueTileAttributeRecord {
						.rawBytes = copyRawBytes(tileNode->rawData(), attributeOffset, tileNode->rawData().size()),
					});
					stop_attributes = true;
					break;
				}
			}
		}

		for (BinaryNode* itemNode : tileNode->children()) {
			uint8_t item_type;
			if (!itemNode->getByte(item_type)) {
				tile->addOpaqueChildNode(capturePreservedNode(itemNode));
				continue;
			}

			if (item_type == OTBM_ITEM) {
				auto item = ItemSerializationOTBM::createFromStream(iomap, itemNode);
				if (!hasResolvedDefinition(item)) {
					PreservedOTBMNode rawNode = capturePreservedNode(itemNode);
					const uint16_t serverId = item ? item->getID() : decodeServerIdFromNodePayload(rawNode);
					if (!item) {
						item = createInvalidPlaceholder(serverId);
					}
					if (item) {
						item->setInvalidOTBMData(InvalidOTBMItemData {
							.kind = InvalidOTBMItemKind::MissingItem,
							.rawNode = std::move(rawNode),
						});
						tile->addItem(std::move(item));
					} else if (!rawNode.empty()) {
						tile->addOpaqueChildNode(std::move(rawNode));
					}
					continue;
				}

				if (item) {
					if (!ItemSerializationOTBM::unserializeItemNode(iomap, itemNode, *item)) {
						item->setInvalidOTBMData(InvalidOTBMItemData {
							.kind = InvalidOTBMItemKind::MissingItem,
							.rawNode = capturePreservedNode(itemNode),
						});
						tile->addItem(std::move(item));
					} else {
						tile->addItem(std::move(item));
					}
				}
			} else {
				tile->addOpaqueChildNode(capturePreservedNode(itemNode));
			}
		}

		TileOperations::update(tile);
		if (house) {
			house->addTile(tile);
		}
	}
}

void TileSerializationOTBM::readTileAreaFast(
	IOMapOTBM& iomap,
	Map& map,
	FastOTBMNode& mapNode,
	const std::array<size_t, 16>* cell_indices,
	std::vector<std::pair<uint32_t, Tile*>>& out_house_tiles,
	uint64_t& out_tile_count)
{
	uint16_t base_x, base_y;
	uint8_t base_z;
	if (!mapNode.stream.getU16(base_x) || !mapNode.stream.getU16(base_y) || !mapNode.stream.getU8(base_z)) {
		return;
	}

	mapNode.forEachChild([&](FastOTBMNode& tileNode) {
		const uint8_t tile_type = tileNode.type;
		if (tile_type != OTBM_TILE && tile_type != OTBM_HOUSETILE) {
			return;
		}

		uint8_t x_offset, y_offset;
		if (!tileNode.stream.getU8(x_offset) || !tileNode.stream.getU8(y_offset)) {
			return;
		}

		const int x = base_x + x_offset;
		const int y = base_y + y_offset;
		const int z = base_z;

		uint32_t house_id = 0;
		if (tile_type == OTBM_HOUSETILE) {
			if (!tileNode.stream.getU32(house_id)) {
				house_id = 0;
			}
		}

		Tile* tile = nullptr;
		if (cell_indices) {
			const size_t cell_idx = (*cell_indices)[((y_offset >> 6) << 2) | (x_offset >> 6)];
			tile = map.createTileInCell(cell_idx, x, y, z);
		} else {
			tile = map.createTile(x, y, z);
		}

		if (!tile || tile->size() > 0) {
			return;
		}

		++out_tile_count;

		bool stop_attributes = false;
		while (!stop_attributes && tileNode.stream.hasMoreProps()) {
			const uint8_t* attrStart = tileNode.stream.p;
			uint8_t attribute = 0;
			if (!tileNode.stream.getU8(attribute)) {
				break;
			}

			switch (attribute) {
				case OTBM_ATTR_TILE_FLAGS: {
					uint32_t flags = 0;
					if (tileNode.stream.getU32(flags)) {
						tile->setMapFlags(flags);
						const uint32_t unknownBits = flags & ~KNOWN_TILE_FLAG_MASK;
						if (unknownBits != 0) {
							tile->recordUnknownMapFlags(flags, unknownBits);
						}
					}
					break;
				}
				case OTBM_ATTR_ITEM: {
					const uint8_t* itemStart = attrStart;
					auto item = ItemSerializationOTBM::createFromStream(iomap, tileNode.stream);
					const uint8_t* itemEnd = tileNode.stream.p;
					if (hasResolvedDefinition(item)) {
						tile->addItemFast(std::move(item));
					} else {
						FastOTBMStream rawStream(itemStart, itemEnd);
						std::vector<uint8_t> rawItemBytes;
						while (rawStream.hasMoreProps()) {
							uint8_t b = 0;
							if (rawStream.getByte(b)) {
								rawItemBytes.push_back(b);
							} else {
								break;
							}
						}

						const bool treatAsGround = shouldTreatInlineItemAsGround(*tile);
						const uint16_t serverId = item ? item->getID() : decodeServerIdFromInlineBytes(rawItemBytes);
						if (!item) {
							item = createInvalidPlaceholder(serverId);
						}
						if (item) {
							item->setInvalidOTBMData(InvalidOTBMItemData {
								.kind = treatAsGround ? InvalidOTBMItemKind::MissingGround : InvalidOTBMItemKind::MissingItem,
								.rawInlineBytes = rawItemBytes,
							});
							tile->addItemFast(std::move(item));
						} else if (!rawItemBytes.empty()) {
							tile->addOpaqueTileAttribute(OpaqueTileAttributeRecord {
								.rawBytes = rawItemBytes,
							});
						}
					}
					break;
				}
				default: {
					FastOTBMStream rawStream(attrStart, tileNode.stream.end);
					std::vector<uint8_t> rawBytes;
					while (rawStream.hasMoreProps()) {
						uint8_t b = 0;
						if (rawStream.getByte(b)) {
							rawBytes.push_back(b);
						} else {
							break;
						}
					}
					tile->addOpaqueTileAttribute(OpaqueTileAttributeRecord {
						.rawBytes = std::move(rawBytes),
					});
					stop_attributes = true;
					break;
				}
			}
		}

		tileNode.forEachChild([&](FastOTBMNode& itemNode) {
			const uint8_t item_type = itemNode.type;
			if (item_type == OTBM_ITEM) {
				auto item = ItemSerializationOTBM::createFromStream(iomap, itemNode.stream);
				if (!hasResolvedDefinition(item)) {
					PreservedOTBMNode rawNode = itemNode.capturePreserved();
					const uint16_t serverId = item ? item->getID() : decodeServerIdFromNodePayload(rawNode);
					if (!item) {
						item = createInvalidPlaceholder(serverId);
					}
					if (item) {
						item->setInvalidOTBMData(InvalidOTBMItemData {
							.kind = InvalidOTBMItemKind::MissingItem,
							.rawNode = std::move(rawNode),
						});
						tile->addItemFast(std::move(item));
					} else if (!rawNode.empty()) {
						tile->addOpaqueChildNode(std::move(rawNode));
					}
					return;
				}

				if (item) {
					if (!ItemSerializationOTBM::unserializeItemNode(iomap, itemNode, *item)) {
						item->setInvalidOTBMData(InvalidOTBMItemData {
							.kind = InvalidOTBMItemKind::MissingItem,
							.rawNode = itemNode.capturePreserved(),
						});
						tile->addItemFast(std::move(item));
					} else {
						tile->addItemFast(std::move(item));
					}
				}
			} else {
				tile->addOpaqueChildNode(itemNode.capturePreserved());
			}
		});

		TileOperations::update(tile);
		if (house_id != 0) {
			out_house_tiles.emplace_back(house_id, tile);
		}
	});
}

void TileSerializationOTBM::writeTileData(const IOMapOTBM& iomap, const Map& map, NodeFileWriteHandle& f, const std::function<void(int)>& progressCb) {
	uint32_t tiles_saved = 0;
	bool first = true;
	int local_x = -1, local_y = -1, local_z = -1;

	const uint64_t total_tiles = map.getTileCount();

	// Use modern ranges for iteration if possible, but the spatial hash grid iterator is already custom
	auto sorted_cells = map.getGrid().getSortedCells();
	for (const auto& sorted_cell : sorted_cells) {
		SpatialHashGrid::GridCell* cell = sorted_cell.cell;
		if (!cell) {
			continue;
		}

		for (int i = 0; i < SpatialHashGrid::NODES_IN_CELL; ++i) {
			MapNode* node = cell->nodes[i].get();
			if (!node) {
				continue;
			}

			for (int j = 0; j < MAP_LAYERS; ++j) {
				Floor* floor = node->getFloor(j);
				if (!floor) {
					continue;
				}

				for (int k = 0; k < SpatialHashGrid::TILES_PER_NODE; ++k) {
					Tile* save_tile = floor->locs[k].get();
					if (!save_tile || save_tile->size() == 0) {
						continue;
					}

					++tiles_saved;
					if (tiles_saved % 8192 == 0 && total_tiles > 0 && progressCb) {
						progressCb(std::min(100, static_cast<int>(100.0 * tiles_saved / total_tiles)));
					}

					const Position& pos = save_tile->getPosition();

					// Decide if new node should be created
					if (first || pos.x < local_x || pos.x >= local_x + 256 || pos.y < local_y || pos.y >= local_y + 256 || pos.z != local_z) {
						if (!first) {
							f.endNode();
						}
						first = false;

						f.addNode(OTBM_TILE_AREA);
						f.addU16(local_x = pos.x & 0xFF00);
						f.addU16(local_y = pos.y & 0xFF00);
						f.addU8(local_z = pos.z);
					}

					serializeTile(iomap, save_tile, f);
				}
			}
		}
	}

	if (!first) {
		f.endNode();
	}
}

void TileSerializationOTBM::serializeTile(const IOMapOTBM& iomap, const Tile* save_tile, NodeFileWriteHandle& f) {
	f.addNode(save_tile->isHouseTile() ? OTBM_HOUSETILE : OTBM_TILE);

	f.addU8(save_tile->getX() & 0xFF);
	f.addU8(save_tile->getY() & 0xFF);

	if (save_tile->isHouseTile()) {
		f.addU32(save_tile->getHouseID());
	}

	if (save_tile->getMapFlags()) {
		f.addU8(OTBM_ATTR_TILE_FLAGS);
		f.addU32(save_tile->getMapFlags());
	}

	if (save_tile->ground) {
		Item* ground = save_tile->ground.get();
		if (!ground->isMetaItem()) {
			const InvalidOTBMItemData* invalidData = ground->getInvalidOTBMData();
			if (invalidData && invalidData->hasRawInlineBytes()) {
				writeRawInlineBytes(f, invalidData->rawInlineBytes);
			} else if (invalidData && invalidData->hasRawNode()) {
				writePreservedNode(f, *invalidData->rawNode);
			} else if (ground->isComplex()) {
				ItemSerializationOTBM::serializeItemNode(iomap, f, *ground);
			} else {
				f.addU8(OTBM_ATTR_ITEM);
				ItemSerializationOTBM::serializeItemCompact(iomap, f, *ground);
			}
		}
	}

	for (const auto& item : save_tile->items) {
		if (!item->isMetaItem()) {
			const InvalidOTBMItemData* invalidData = item->getInvalidOTBMData();
			if (invalidData && invalidData->hasRawInlineBytes()) {
				writeRawInlineBytes(f, invalidData->rawInlineBytes);
			} else if (invalidData && invalidData->hasRawNode()) {
				writePreservedNode(f, *invalidData->rawNode);
			} else {
				ItemSerializationOTBM::serializeItemNode(iomap, f, *item);
			}
		}
	}

	if (const InvalidZoneState* invalidZones = save_tile->getInvalidZones()) {
		for (const auto& opaqueAttribute : invalidZones->opaqueTileAttributes) {
			writeRawInlineBytes(f, opaqueAttribute.rawBytes);
		}
		for (const auto& opaqueChildNode : invalidZones->opaqueChildNodes) {
			writePreservedNode(f, opaqueChildNode);
		}
	}

	f.endNode();
}
