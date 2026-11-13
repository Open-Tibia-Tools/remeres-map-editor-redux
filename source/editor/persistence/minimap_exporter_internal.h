#ifndef RME_EDITOR_PERSISTENCE_MINIMAP_EXPORTER_INTERNAL_H_
#define RME_EDITOR_PERSISTENCE_MINIMAP_EXPORTER_INTERNAL_H_

#include "app/definitions.h"
#include "editor/persistence/minimap_exporter.h"

#include <array>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

class Map;
class Tile;

namespace minimap_export {

struct Bounds {
	int minX = std::numeric_limits<int>::max();
	int minY = std::numeric_limits<int>::max();
	int maxX = std::numeric_limits<int>::min();
	int maxY = std::numeric_limits<int>::min();

	[[nodiscard]] bool valid() const {
		return minX <= maxX && minY <= maxY;
	}
};

struct Chunk {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

struct FloorSelection {
	std::array<int, MAP_LAYERS> values {};
	size_t count = 0;

	void push(int floor);
	[[nodiscard]] std::span<const int> floors() const;
};

[[nodiscard]] bool isTileExportable(const Tile* tile);
[[nodiscard]] bool containsFloor(std::span<const int> floors, int floor);
[[nodiscard]] FloorSelection allFloors();
[[nodiscard]] FloorSelection singleFloor(int floor);
[[nodiscard]] FloorSelection showAllFloorsForTarget(int targetFloor);
[[nodiscard]] FloorSelection selectedFloorsForOutput(const MinimapExportOptions& options);
[[nodiscard]] FloorSelection selectedFloorsForBounds(const MinimapExportOptions& options);
[[nodiscard]] FloorSelection selectedFloorsForOtmm(const MinimapExportOptions& options);
[[nodiscard]] int maxEncodedImageDimension(MinimapExportFormat format);
[[nodiscard]] Bounds findContentBounds(Map& map, std::span<const int> floors);
[[nodiscard]] Bounds findMapBounds(const Map& map);
[[nodiscard]] std::vector<Chunk> buildChunks(const Bounds& bounds, int outputImageSize, int chunkImageSize);
[[nodiscard]] double exportScaleForBounds(const Bounds& bounds, int imageSize);
[[nodiscard]] std::filesystem::path exportRootDirectory(const MinimapExportOptions& options);
[[nodiscard]] std::filesystem::path floorDirectory(const MinimapExportOptions& options, int floor);
[[nodiscard]] std::filesystem::path fullImagesDirectory(const MinimapExportOptions& options);
[[nodiscard]] bool ensureDirectory(const std::filesystem::path& directory);
[[nodiscard]] std::filesystem::path outputFile(const std::filesystem::path& directory, const std::string& baseName, const std::string& extension);
[[nodiscard]] std::filesystem::path chunkFile(const MinimapExportOptions& options, int floor, const Chunk& chunk);
[[nodiscard]] std::filesystem::path fullImageFile(const MinimapExportOptions& options, int floor);

[[nodiscard]] MinimapExportResult exportOtmm(Editor& editor, const MinimapExportOptions& options, const MinimapExporter::ProgressCallback& progress);
[[nodiscard]] MinimapExportResult exportImages(Editor& editor, const MinimapExportOptions& options, const MinimapExporter::ProgressCallback& progress);

} // namespace minimap_export

#endif
