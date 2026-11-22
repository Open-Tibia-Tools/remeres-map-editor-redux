#include "editor/persistence/minimap_exporter_internal.h"

#include "editor/editor.h"
#include "map/map.h"
#include "map/map_region.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <utility>
#include <vector>
#include <zlib.h>

namespace minimap_export {
namespace {

constexpr uint32_t kOtmmSignature = 0x4D4d544F;
constexpr uint16_t kOtmmVersion = 1;
constexpr int kMinimapBlockSize = 64;
constexpr int kMinimapBlocksPerAxis = 65536 / kMinimapBlockSize;
constexpr int kDefaultTileSpeed = 10;

enum MinimapTileFlags : uint8_t {
	MinimapTileWasSeen = 1,
	MinimapTileNotPathable = 2,
	MinimapTileNotWalkable = 4,
};

struct MinimapTileRecord {
	uint8_t flags = 0;
	uint8_t color = INVALID_MINIMAP_COLOR;
	uint8_t speed = kDefaultTileSpeed;
};

static_assert(sizeof(MinimapTileRecord) == 3);

class MinimapBlock {
public:
	void updateTile(int x, int y, MinimapTileRecord record) {
		tiles_[static_cast<size_t>((y % kMinimapBlockSize) * kMinimapBlockSize + (x % kMinimapBlockSize))] = record;
	}

	[[nodiscard]] const std::array<MinimapTileRecord, kMinimapBlockSize * kMinimapBlockSize>& tiles() const {
		return tiles_;
	}

private:
	std::array<MinimapTileRecord, kMinimapBlockSize * kMinimapBlockSize> tiles_;
};

class BinaryWriter {
public:
	explicit BinaryWriter(const std::filesystem::path& file) :
		stream_(file, std::ios::binary) {
	}

	[[nodiscard]] bool ok() const {
		return stream_.good();
	}

	[[nodiscard]] std::streamoff tell() {
		return static_cast<std::streamoff>(stream_.tellp());
	}

	void seek(std::streamoff position) {
		stream_.seekp(position);
	}

	template <typename T>
	void writeValue(T value) {
		stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void writeBytes(std::span<const uint8_t> bytes) {
		stream_.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	void writeString(const std::string& text) {
		writeValue(static_cast<uint16_t>(text.size()));
		stream_.write(text.data(), static_cast<std::streamsize>(text.size()));
	}

private:

	std::ofstream stream_;
};

[[nodiscard]] uint8_t tileSpeed(const Tile& tile) {
	if (!tile.ground) {
		return kDefaultTileSpeed;
	}

	const auto definition = tile.ground->getDefinition();
	if (!definition) {
		return kDefaultTileSpeed;
	}

	const int speed = static_cast<int>(std::ceil(static_cast<double>(definition.attribute(ItemAttributeKey::WaySpeed)) / 10.0));
	return static_cast<uint8_t>(std::clamp(speed, 0, 255));
}

[[nodiscard]] MinimapTileRecord toOtmmTileRecord(const Tile& tile) {
	uint8_t flags = MinimapTileWasSeen;
	if (tile.isBlocking()) {
		flags |= MinimapTileNotWalkable;
	}

	return {
		.flags = flags,
		.color = tile.getMiniMapColor(),
		.speed = tileSpeed(tile),
	};
}

[[nodiscard]] uint32_t blockIndex(const Position& position) {
	return static_cast<uint32_t>((position.y / kMinimapBlockSize) * kMinimapBlocksPerAxis + (position.x / kMinimapBlockSize));
}

[[nodiscard]] std::pair<uint16_t, uint16_t> blockOrigin(uint32_t index) {
	const uint16_t x = static_cast<uint16_t>((index % kMinimapBlocksPerAxis) * kMinimapBlockSize);
	const uint16_t y = static_cast<uint16_t>((index / kMinimapBlocksPerAxis) * kMinimapBlockSize);
	return { x, y };
}

using FloorBlocks = std::array<std::map<uint32_t, MinimapBlock>, MAP_LAYERS>;

[[nodiscard]] FloorBlocks readBlocks(Map& map, std::span<const int> floors, const MinimapExporter::ProgressCallback& progress) {
	FloorBlocks blocks;
	const uint64_t total = map.size();
	uint64_t completed = 0;
	const Bounds bounds = findMapBounds(map);
	if (!bounds.valid()) {
		return blocks;
	}

	map.visitLeaves(bounds.minX, bounds.minY, bounds.maxX + 1, bounds.maxY + 1, [&](const MapNode* node, int nodeMapX, int nodeMapY) {
		for (int localNodeX = 0; localNodeX < 4; ++localNodeX) {
			const int mapX = nodeMapX + localNodeX;
			if (mapX < bounds.minX || mapX > bounds.maxX) {
				continue;
			}

			for (int localNodeY = 0; localNodeY < 4; ++localNodeY) {
				const int mapY = nodeMapY + localNodeY;
				if (mapY < bounds.minY || mapY > bounds.maxY) {
					continue;
				}

				for (const int floor : floors) {
					const Floor* nodeFloor = node->getFloor(static_cast<uint32_t>(floor));
					if (!nodeFloor) {
						continue;
					}

					const Tile* tile = nodeFloor->locs[static_cast<size_t>(localNodeX * 4 + localNodeY)].get();
					if (!isTileExportable(tile)) {
						continue;
					}

					++completed;
					if (progress && completed % 8192 == 0) {
						progress(completed, total);
					}

					const Position position(mapX, mapY, floor);
					MinimapBlock& block = blocks[static_cast<size_t>(floor)][blockIndex(position)];
					block.updateTile(mapX % kMinimapBlockSize, mapY % kMinimapBlockSize, toOtmmTileRecord(*tile));
				}
			}
		}
	});

	if (progress) {
		progress(total, total);
	}
	return blocks;
}

} // namespace

MinimapExportResult exportOtmm(Editor& editor, const MinimapExportOptions& options, const MinimapExporter::ProgressCallback& progress) {
	BinaryWriter writer(outputFile(exportRootDirectory(options), options.fileBaseName, ".otmm"));
	if (!writer.ok()) {
		return { .ok = false, .error = "Unable to open OTMM output file." };
	}

	writer.writeValue(kOtmmSignature);
	writer.writeValue(uint16_t { 0 });
	writer.writeValue(kOtmmVersion);
	writer.writeValue(uint32_t { 0 });
	writer.writeString("OTMM 1.0");

	const auto dataStart = static_cast<uint16_t>(writer.tell());
	writer.seek(sizeof(uint32_t));
	writer.writeValue(dataStart);
	writer.seek(dataStart);

	const auto floors = selectedFloorsForOtmm(options);
	FloorBlocks blocks = readBlocks(editor.map, floors.floors(), progress);
	constexpr size_t blockSize = kMinimapBlockSize * kMinimapBlockSize * sizeof(MinimapTileRecord);
	std::vector<uint8_t> compressed(compressBound(static_cast<uLong>(blockSize)));

	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		for (const auto& [index, block] : blocks[static_cast<size_t>(floor)]) {
			const auto [x, y] = blockOrigin(index);
			writer.writeValue(x);
			writer.writeValue(y);
			writer.writeValue(static_cast<uint8_t>(floor));

			uLongf compressedSize = static_cast<uLongf>(compressed.size());
			const auto* source = reinterpret_cast<const Bytef*>(block.tiles().data());
			if (compress2(compressed.data(), &compressedSize, source, static_cast<uLong>(blockSize), 3) != Z_OK) {
				return { .ok = false, .error = "Failed to compress OTMM minimap block." };
			}

			writer.writeValue(static_cast<uint16_t>(compressedSize));
			writer.writeBytes(std::span<const uint8_t>(compressed.data(), static_cast<size_t>(compressedSize)));
		}
	}

	writer.writeValue(uint16_t { 65535 });
	writer.writeValue(uint16_t { 65535 });
	writer.writeValue(uint8_t { 255 });
	if (!writer.ok()) {
		return { .ok = false, .error = "Failed while writing OTMM output file." };
	}

	return { .ok = true, .filesWritten = 1 };
}

} // namespace minimap_export
