#include "editor/persistence/minimap_exporter.h"

#include "editor/editor.h"
#include "editor/persistence/minimap_exporter_internal.h"
#include "map/map.h"
#include "map/map_region.h"
#include "map/tile.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace minimap_export {

namespace {

constexpr uint64_t kMaxFullImageBytes = 1536ULL * 1024ULL * 1024ULL;
constexpr uint64_t kRgbPixelBytes = 3;

} // namespace

void FloorSelection::push(int floor) {
	if (floor >= 0 && floor < MAP_LAYERS && count < values.size() && !containsFloor(floors(), floor)) {
		values[count++] = floor;
	}
}

std::span<const int> FloorSelection::floors() const {
	return { values.data(), count };
}

bool isTileExportable(const Tile* tile) {
	return tile && !tile->empty();
}

bool containsFloor(std::span<const int> floors, int floor) {
	return std::ranges::find(floors, floor) != floors.end();
}

FloorSelection allFloors() {
	FloorSelection selection;
	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		selection.push(floor);
	}
	return selection;
}

FloorSelection singleFloor(int floor) {
	FloorSelection selection;
	selection.push(std::clamp(floor, 0, MAP_MAX_LAYER));
	return selection;
}

FloorSelection showAllFloorsForTarget(int targetFloor) {
	FloorSelection selection;
	const int floor = std::clamp(targetFloor, 0, MAP_MAX_LAYER);
	if (floor <= GROUND_LAYER) {
		for (int visibleFloor = GROUND_LAYER; visibleFloor >= floor; --visibleFloor) {
			selection.push(visibleFloor);
		}
		return selection;
	}

	for (int visibleFloor = std::min(MAP_MAX_LAYER, floor + 2); visibleFloor >= floor; --visibleFloor) {
		selection.push(visibleFloor);
	}
	return selection;
}

FloorSelection selectedFloorsForOutput(const MinimapExportOptions& options) {
	FloorSelection selection;
	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		if (options.selectedFloors[static_cast<size_t>(floor)]) {
			selection.push(floor);
		}
	}
	return selection;
}

FloorSelection selectedFloorsForBounds(const MinimapExportOptions& options) {
	const FloorSelection outputFloors = selectedFloorsForOutput(options);
	if (options.format == MinimapExportFormat::Otmm || !options.showAllFloors) {
		return outputFloors;
	}

	FloorSelection selection;
	for (const int outputFloor : outputFloors.floors()) {
		const FloorSelection visibleFloors = showAllFloorsForTarget(outputFloor);
		for (const int visibleFloor : visibleFloors.floors()) {
			selection.push(visibleFloor);
		}
	}
	return selection;
}

FloorSelection selectedFloorsForOtmm(const MinimapExportOptions& options) {
	return selectedFloorsForOutput(options);
}

int maxEncodedImageDimension(MinimapExportFormat format) {
	return format == MinimapExportFormat::Webp ? kMaxWebpDimension : kMaxJpegDimension;
}

Bounds findContentBounds(Map& map, std::span<const int> floors) {
	Bounds bounds;

	Bounds mapBounds = findMapBounds(map);
	if (!mapBounds.valid()) {
		return bounds;
	}

	map.visitLeaves(mapBounds.minX, mapBounds.minY, mapBounds.maxX + 1, mapBounds.maxY + 1, [&](const MapNode* node, int nodeMapX, int nodeMapY) {
		for (int localNodeX = 0; localNodeX < 4; ++localNodeX) {
			const int mapX = nodeMapX + localNodeX;
			if (mapX < mapBounds.minX || mapX > mapBounds.maxX) {
				continue;
			}

			for (int localNodeY = 0; localNodeY < 4; ++localNodeY) {
				const int mapY = nodeMapY + localNodeY;
				if (mapY < mapBounds.minY || mapY > mapBounds.maxY) {
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

					bounds.minX = std::min(bounds.minX, mapX);
					bounds.minY = std::min(bounds.minY, mapY);
					bounds.maxX = std::max(bounds.maxX, mapX);
					bounds.maxY = std::max(bounds.maxY, mapY);
				}
			}
		}
	});
	return bounds;
}

Bounds findMapBounds(const Map& map) {
	if (map.getWidth() <= 0 || map.getHeight() <= 0) {
		return {};
	}

	return {
		.minX = 0,
		.minY = 0,
		.maxX = map.getWidth() - 1,
		.maxY = map.getHeight() - 1,
	};
}

double exportScaleForBounds(const Bounds& bounds, int imageSize) {
	if (!bounds.valid() || imageSize <= 0) {
		return 1.0;
	}

	const int width = bounds.maxX - bounds.minX + 1;
	const int height = bounds.maxY - bounds.minY + 1;
	const int largestAxis = std::max(width, height);
	if (largestAxis <= 0) {
		return 1.0;
	}

	return std::max(1.0, static_cast<double>(imageSize) / static_cast<double>(largestAxis));
}

std::vector<Chunk> buildChunks(const Bounds& bounds, int outputImageSize, int chunkImageSize) {
	std::vector<Chunk> chunks;
	if (!bounds.valid() || outputImageSize <= 0 || chunkImageSize <= 0) {
		return chunks;
	}

	const double scale = exportScaleForBounds(bounds, outputImageSize);
	const int outputWidth = static_cast<int>(std::ceil(static_cast<double>(bounds.maxX - bounds.minX + 1) * scale));
	const int outputHeight = static_cast<int>(std::ceil(static_cast<double>(bounds.maxY - bounds.minY + 1) * scale));
	const int columns = (outputWidth + chunkImageSize - 1) / chunkImageSize;
	const int rows = (outputHeight + chunkImageSize - 1) / chunkImageSize;
	chunks.reserve(static_cast<size_t>(columns * rows));

	for (int y = 0; y < outputHeight; y += chunkImageSize) {
		for (int x = 0; x < outputWidth; x += chunkImageSize) {
			chunks.push_back({
				.x = x,
				.y = y,
				.width = std::min(chunkImageSize, outputWidth - x),
				.height = std::min(chunkImageSize, outputHeight - y),
			});
		}
	}
	return chunks;
}

wxFileName exportRootDirectory(const MinimapExportOptions& options) {
	wxFileName directory(options.outputDirectory);
	directory.AppendDir(wxString::FromUTF8(options.fileBaseName.c_str()));
	return directory;
}

wxFileName floorDirectory(const MinimapExportOptions& options, int floor) {
	wxFileName directory(exportRootDirectory(options));
	directory.AppendDir(wxString::Format("%d", floor));
	return directory;
}

wxFileName fullImagesDirectory(const MinimapExportOptions& options) {
	wxFileName directory(exportRootDirectory(options));
	directory.AppendDir("full_images");
	return directory;
}

bool ensureDirectory(const wxFileName& directory) {
	return directory.DirExists() || wxFileName::Mkdir(directory.GetFullPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
}

wxFileName outputFile(const wxFileName& directory, const std::string& baseName, const std::string& extension) {
	wxFileName file(directory);
	const std::string fullName = baseName + extension;
	file.SetFullName(wxString::FromUTF8(fullName.c_str()));
	return file;
}

wxFileName chunkFile(const MinimapExportOptions& options, int floor, const Chunk& chunk) {
	const std::string extension = options.format == MinimapExportFormat::Jpg ? ".jpg" : ".webp";
	wxFileName file(floorDirectory(options, floor));
	file.SetFullName(wxString::FromUTF8(options.fileBaseName.c_str()) + wxString::Format("_z%d_x%d_y%d", floor, chunk.x, chunk.y) + wxString::FromUTF8(extension.c_str()));
	return file;
}

wxFileName fullImageFile(const MinimapExportOptions& options, int floor) {
	const std::string extension = options.format == MinimapExportFormat::Jpg ? ".jpg" : ".webp";
	wxFileName file(fullImagesDirectory(options));
	file.SetFullName(wxString::FromUTF8(options.fileBaseName.c_str()) + wxString::Format("_z%d_full", floor) + wxString::FromUTF8(extension.c_str()));
	return file;
}

} // namespace minimap_export

namespace {

[[nodiscard]] bool isSafeFileBaseName(const std::string& fileBaseName) {
	if (fileBaseName.contains("..") || fileBaseName.contains('/') || fileBaseName.contains('\\')) {
		return false;
	}

	const std::filesystem::path path(fileBaseName);
	return path.filename().string() == fileBaseName;
}

[[nodiscard]] MinimapExportResult validateOptions(Editor& editor, const MinimapExportOptions& options) {
	constexpr uint64_t maxFullImageBytes = 1536ULL * 1024ULL * 1024ULL;
	constexpr uint64_t rgbPixelBytes = 3;

	if (options.fileBaseName.empty()) {
		return { .ok = false, .error = "File name is required." };
	}
	if (!isSafeFileBaseName(options.fileBaseName)) {
		return { .ok = false, .error = "File name must not contain path separators or parent directory references." };
	}
	if (options.imageSize <= 0) {
		return { .ok = false, .error = "Image size must be greater than zero." };
	}
	if (minimap_export::selectedFloorsForOutput(options).count == 0) {
		return { .ok = false, .error = "Select at least one floor to export." };
	}
	if (options.format != MinimapExportFormat::Otmm) {
		const int maxDimension = minimap_export::maxEncodedImageDimension(options.format);
		if (options.imageSize > maxDimension) {
			return { .ok = false, .error = "Selected image size exceeds the maximum dimension supported by this format." };
		}

		const minimap_export::Bounds bounds = minimap_export::findMapBounds(editor.map);
		if (bounds.valid()) {
			const double scale = minimap_export::exportScaleForBounds(bounds, options.imageSize);
			const int outputWidth = static_cast<int>(std::ceil(static_cast<double>(bounds.maxX - bounds.minX + 1) * scale));
			const int outputHeight = static_cast<int>(std::ceil(static_cast<double>(bounds.maxY - bounds.minY + 1) * scale));
			if (outputWidth > maxDimension || outputHeight > maxDimension) {
				return { .ok = false, .error = "The selected format cannot encode the resulting minimap dimensions." };
			}

			const uint64_t fullImageBytes = static_cast<uint64_t>(outputWidth) * static_cast<uint64_t>(outputHeight) * rgbPixelBytes;
			if (fullImageBytes > maxFullImageBytes) {
				return { .ok = false, .error = "Selected image size is too large to allocate safely. Select a smaller image size." };
			}
		}
	}
	if (!options.outputDirectory.DirExists()) {
		return { .ok = false, .error = "Output folder not found." };
	}
	if (!options.outputDirectory.IsDirWritable()) {
		return { .ok = false, .error = "Output folder is not writable." };
	}
	return { .ok = true };
}

} // namespace

MinimapExportResult MinimapExporter::Export(Editor& editor, const MinimapExportOptions& options, ProgressCallback progress) {
	if (MinimapExportResult validation = validateOptions(editor, options); !validation.ok) {
		return validation;
	}

	try {
		const wxFileName rootDirectory = minimap_export::exportRootDirectory(options);
		if (!minimap_export::ensureDirectory(rootDirectory)) {
			return { .ok = false, .error = "Failed to create minimap export folder: " + rootDirectory.GetFullPath().ToStdString() };
		}
		if (!rootDirectory.IsDirWritable()) {
			return { .ok = false, .error = "Minimap export folder is not writable: " + rootDirectory.GetFullPath().ToStdString() };
		}

		if (options.format == MinimapExportFormat::Otmm) {
			return minimap_export::exportOtmm(editor, options, progress);
		}
		return minimap_export::exportImages(editor, options, progress);
	} catch (const std::bad_alloc&) {
		return { .ok = false, .error = "There is not enough memory available to complete the operation." };
	} catch (const std::exception& exception) {
		return { .ok = false, .error = exception.what() };
	}
}
