#include "editor/persistence/minimap_exporter_internal.h"

#include "editor/editor.h"
#include "map/map.h"
#include "map/map_region.h"
#include "rendering/core/minimap_colors.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <webp/encode.h>

#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace minimap_export {
namespace {

constexpr double kCoveredFloorShade = 0.55;
constexpr uint64_t kMaxStitchedImageBytes = 1536ULL * 1024ULL * 1024ULL;

struct RgbImage {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> pixels;
};

struct LoadedChunkImage {
	Chunk chunk;
	RgbImage image;
};

struct StitchedPlacement {
	size_t imageIndex = 0;
	int x = 0;
	int y = 0;
};

struct StitchedLayout {
	int width = 0;
	int height = 0;
	std::vector<StitchedPlacement> placements;
};

[[nodiscard]] uint8_t shadedChannel(uint8_t channel, double shade) {
	return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(channel * shade)), 0, 255));
}

void fillScaledTilePixels(const Bounds& bounds, const Chunk& chunk, double scale, int mapX, int mapY, const RGBQuad& color, double shade, std::vector<uint8_t>& pixels) {
	const int startX = std::clamp(static_cast<int>(std::floor(static_cast<double>(mapX - bounds.minX) * scale)) - chunk.x, 0, chunk.width);
	const int endX = std::clamp(static_cast<int>(std::ceil(static_cast<double>(mapX + 1 - bounds.minX) * scale)) - chunk.x, 0, chunk.width);
	const int startY = std::clamp(static_cast<int>(std::floor(static_cast<double>(mapY - bounds.minY) * scale)) - chunk.y, 0, chunk.height);
	const int endY = std::clamp(static_cast<int>(std::ceil(static_cast<double>(mapY + 1 - bounds.minY) * scale)) - chunk.y, 0, chunk.height);
	if (startX >= endX || startY >= endY) {
		return;
	}

	const uint8_t red = shadedChannel(color.red, shade);
	const uint8_t green = shadedChannel(color.green, shade);
	const uint8_t blue = shadedChannel(color.blue, shade);
	for (int pixelY = startY; pixelY < endY; ++pixelY) {
		for (int pixelX = startX; pixelX < endX; ++pixelX) {
			const size_t pixelIndex = (static_cast<size_t>(pixelY) * chunk.width + pixelX) * PixelFormatRGB;
			pixels[pixelIndex] = red;
			pixels[pixelIndex + 1] = green;
			pixels[pixelIndex + 2] = blue;
		}
	}
}

[[nodiscard]] bool renderFloorIntoChunk(const Map& map, int floor, const Bounds& bounds, const Chunk& chunk, double scale, double shade, std::vector<uint8_t>& pixels) {
	bool wrotePixels = false;
	const int sourceStartX = std::clamp(bounds.minX + static_cast<int>(std::floor(static_cast<double>(chunk.x) / scale)), bounds.minX, bounds.maxX);
	const int sourceStartY = std::clamp(bounds.minY + static_cast<int>(std::floor(static_cast<double>(chunk.y) / scale)), bounds.minY, bounds.maxY);
	const int sourceEndX = std::clamp(bounds.minX + static_cast<int>(std::ceil(static_cast<double>(chunk.x + chunk.width) / scale)), bounds.minX, bounds.maxX + 1);
	const int sourceEndY = std::clamp(bounds.minY + static_cast<int>(std::ceil(static_cast<double>(chunk.y + chunk.height) / scale)), bounds.minY, bounds.maxY + 1);

	map.visitLeaves(sourceStartX, sourceStartY, sourceEndX, sourceEndY, [&](const MapNode* node, int nodeMapX, int nodeMapY) {
		const Floor* nodeFloor = node->getFloor(static_cast<uint32_t>(floor));
		if (!nodeFloor) {
			return;
		}

		for (int localNodeX = 0; localNodeX < 4; ++localNodeX) {
			const int mapX = nodeMapX + localNodeX;
			if (mapX < bounds.minX || mapX > bounds.maxX || mapX < sourceStartX || mapX >= sourceEndX) {
				continue;
			}

			for (int localNodeY = 0; localNodeY < 4; ++localNodeY) {
				const int mapY = nodeMapY + localNodeY;
				if (mapY < bounds.minY || mapY > bounds.maxY || mapY < sourceStartY || mapY >= sourceEndY) {
					continue;
				}

				const auto& location = nodeFloor->locs[static_cast<size_t>(localNodeX * 4 + localNodeY)];
				const Tile* tile = location.get();
				if (!isTileExportable(tile)) {
					continue;
				}

				const uint8_t colorIndex = tile->getMiniMapColor();
				if (colorIndex == 0) {
					continue;
				}

				const RGBQuad& color = minimap_color[colorIndex];
				fillScaledTilePixels(bounds, chunk, scale, mapX, mapY, color, shade, pixels);
				wrotePixels = true;
			}
		}
	});
	return wrotePixels;
}

[[nodiscard]] std::vector<uint8_t> renderImageChunk(const Map& map, int targetFloor, const Bounds& bounds, const Chunk& chunk, double scale, const MinimapExportOptions& options, bool& wrotePixels) {
	std::vector<uint8_t> pixels(static_cast<size_t>(chunk.width) * chunk.height * PixelFormatRGB, 0);
	wrotePixels = false;

	if (!options.showAllFloors) {
		wrotePixels = renderFloorIntoChunk(map, targetFloor, bounds, chunk, scale, 1.0, pixels);
		return pixels;
	}

	const auto floors = showAllFloorsForTarget(targetFloor);
	for (size_t index = 0; index < floors.count; ++index) {
		const int floor = floors.values[index];
		const double shade = floor == targetFloor || !options.applyShadeToAdjacentFloors ? 1.0 : kCoveredFloorShade;
		wrotePixels = renderFloorIntoChunk(map, floor, bounds, chunk, scale, shade, pixels) || wrotePixels;
	}
	return pixels;
}

[[nodiscard]] bool saveJpg(const std::filesystem::path& file, int width, int height, const std::vector<uint8_t>& pixels) {
	return stbi_write_jpg(file.string().c_str(), width, height, PixelFormatRGB, pixels.data(), 90) != 0;
}

[[nodiscard]] bool saveWebp(const std::filesystem::path& file, int width, int height, const std::vector<uint8_t>& pixels) {
	uint8_t* encoded = nullptr;
	const size_t encodedSize = WebPEncodeLosslessRGB(pixels.data(), width, height, width * PixelFormatRGB, &encoded);
	if (encodedSize == 0 || !encoded) {
		return false;
	}

	std::ofstream stream(file, std::ios::binary);
	const bool opened = stream.good();
	if (opened) {
		stream.write(reinterpret_cast<const char*>(encoded), static_cast<std::streamsize>(encodedSize));
	}

	WebPFree(encoded);
	return opened && stream.good();
}

[[nodiscard]] size_t sortedIndex(std::span<const int> values, int value) {
	const auto iterator = std::ranges::lower_bound(values, value);
	return static_cast<size_t>(std::distance(values.begin(), iterator));
}

[[nodiscard]] std::vector<int> prefixOffsets(std::span<const int> sizes) {
	std::vector<int> offsets(sizes.size(), 0);
	for (size_t index = 1; index < sizes.size(); ++index) {
		offsets[index] = offsets[index - 1] + sizes[index - 1];
	}
	return offsets;
}

[[nodiscard]] StitchedLayout buildCompactLayout(std::span<const LoadedChunkImage> chunks) {
	std::vector<int> columns;
	std::vector<int> rows;
	columns.reserve(chunks.size());
	rows.reserve(chunks.size());

	for (const LoadedChunkImage& chunk : chunks) {
		columns.push_back(chunk.chunk.x);
		rows.push_back(chunk.chunk.y);
	}

	std::ranges::sort(columns);
	std::ranges::sort(rows);
	const auto columnsEnd = std::ranges::unique(columns).begin();
	const auto rowsEnd = std::ranges::unique(rows).begin();
	columns.erase(columnsEnd, columns.end());
	rows.erase(rowsEnd, rows.end());

	std::vector<int> columnWidths(columns.size(), 0);
	std::vector<int> rowHeights(rows.size(), 0);
	for (const LoadedChunkImage& chunk : chunks) {
		const size_t column = sortedIndex(columns, chunk.chunk.x);
		const size_t row = sortedIndex(rows, chunk.chunk.y);
		columnWidths[column] = std::max(columnWidths[column], chunk.image.width);
		rowHeights[row] = std::max(rowHeights[row], chunk.image.height);
	}

	const std::vector<int> columnOffsets = prefixOffsets(columnWidths);
	const std::vector<int> rowOffsets = prefixOffsets(rowHeights);
	StitchedLayout layout {
		.width = columnOffsets.empty() ? 0 : columnOffsets.back() + columnWidths.back(),
		.height = rowOffsets.empty() ? 0 : rowOffsets.back() + rowHeights.back(),
	};
	layout.placements.reserve(chunks.size());

	for (size_t index = 0; index < chunks.size(); ++index) {
		const LoadedChunkImage& chunk = chunks[index];
		layout.placements.push_back({
			.imageIndex = index,
			.x = columnOffsets[sortedIndex(columns, chunk.chunk.x)],
			.y = rowOffsets[sortedIndex(rows, chunk.chunk.y)],
		});
	}
	return layout;
}

[[nodiscard]] bool canSaveFullImage(const MinimapExportOptions& options, const StitchedLayout& layout) {
	return layout.width <= maxEncodedImageDimension(options.format) && layout.height <= maxEncodedImageDimension(options.format);
}

[[nodiscard]] MinimapExportResult stitchFloorImages(const MinimapExportOptions& options, int floor, std::span<const LoadedChunkImage> loadedChunks) {
	if (loadedChunks.empty()) {
		return { .ok = true };
	}

	const StitchedLayout layout = buildCompactLayout(loadedChunks);
	if (layout.width <= 0 || layout.height <= 0) {
		return { .ok = false, .error = "Failed to calculate stitched minimap image bounds." };
	}
	if (!canSaveFullImage(options, layout)) {
		return { .ok = false, .error = "Selected image size exceeds the maximum dimension supported by this format." };
	}

	const uint64_t stitchedBytes = static_cast<uint64_t>(layout.width) * static_cast<uint64_t>(layout.height) * PixelFormatRGB;
	if (stitchedBytes > kMaxStitchedImageBytes) {
		return { .ok = false, .error = "Stitched minimap image for floor " + std::to_string(floor) + " is too large to allocate. Select a smaller image size." };
	}

	std::vector<uint8_t> pixels(static_cast<size_t>(stitchedBytes), 0);
	for (const StitchedPlacement& placement : layout.placements) {
		const LoadedChunkImage& chunk = loadedChunks[placement.imageIndex];
		const int rowBytes = chunk.image.width * PixelFormatRGB;
		for (int y = 0; y < chunk.image.height; ++y) {
			const auto sourceOffset = static_cast<size_t>(y) * rowBytes;
			const auto targetOffset = (static_cast<size_t>(placement.y + y) * layout.width + placement.x) * PixelFormatRGB;
			std::memcpy(pixels.data() + targetOffset, chunk.image.pixels.data() + sourceOffset, static_cast<size_t>(rowBytes));
		}
	}

	const std::filesystem::path file = fullImageFile(options, floor);
	const bool saved = options.format == MinimapExportFormat::Jpg
		? saveJpg(file, layout.width, layout.height, pixels)
		: saveWebp(file, layout.width, layout.height, pixels);
	if (!saved) {
		return { .ok = false, .error = "Failed to write stitched minimap image: " + file.string() };
	}

	return { .ok = true, .filesWritten = 1 };
}

} // namespace

MinimapExportResult exportImages(Editor& editor, const MinimapExportOptions& options, const MinimapExporter::ProgressCallback& progress) {
	Bounds bounds = findMapBounds(editor.map);
	if (!bounds.valid()) {
		const FloorSelection boundsFloors = selectedFloorsForBounds(options);
		bounds = findContentBounds(editor.map, boundsFloors.floors());
	}
	const std::vector<Chunk> chunks = buildChunks(bounds, options.imageSize, options.imageSize);
	if (chunks.empty()) {
		return { .ok = true };
	}
	const double scale = exportScaleForBounds(bounds, options.imageSize);

	const FloorSelection outputFloors = selectedFloorsForOutput(options);
	const uint64_t total = static_cast<uint64_t>(chunks.size() * outputFloors.count + outputFloors.count);
	uint64_t completed = 0;
	size_t filesWritten = 0;

	const std::filesystem::path fullImages = fullImagesDirectory(options);
	if (!ensureDirectory(fullImages)) {
		return { .ok = false, .error = "Failed to create full minimap image folder: " + fullImages.string() };
	}

	for (const int floor : outputFloors.floors()) {
		const std::filesystem::path chunksDirectory = floorDirectory(options, floor);
		if (!ensureDirectory(chunksDirectory)) {
			return { .ok = false, .filesWritten = filesWritten, .error = "Failed to create minimap floor folder: " + chunksDirectory.string() };
		}

		std::vector<LoadedChunkImage> loadedChunks;
		loadedChunks.reserve(chunks.size());
		for (const Chunk& chunk : chunks) {
			bool wrotePixels = false;
			std::vector<uint8_t> pixels = renderImageChunk(editor.map, floor, bounds, chunk, scale, options, wrotePixels);
			++completed;
			if (progress) {
				progress(completed, total);
			}
			(void)wrotePixels;

			const std::filesystem::path file = chunkFile(options, floor, chunk);
			const bool saved = options.format == MinimapExportFormat::Jpg
				? saveJpg(file, chunk.width, chunk.height, pixels)
				: saveWebp(file, chunk.width, chunk.height, pixels);
			if (!saved) {
				return { .ok = false, .filesWritten = filesWritten, .error = "Failed to write minimap image: " + file.string() };
			}
			loadedChunks.push_back({
				.chunk = chunk,
				.image = {
					.width = chunk.width,
					.height = chunk.height,
					.pixels = std::move(pixels),
				},
			});
			++filesWritten;
		}

		MinimapExportResult stitchResult = stitchFloorImages(options, floor, loadedChunks);
		if (!stitchResult.ok) {
			stitchResult.filesWritten += filesWritten;
			return stitchResult;
		}
		filesWritten += stitchResult.filesWritten;
		++completed;
		if (progress) {
			progress(completed, total);
		}
	}

	return { .ok = true, .filesWritten = filesWritten };
}

} // namespace minimap_export
