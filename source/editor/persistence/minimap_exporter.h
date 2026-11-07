#ifndef RME_EDITOR_PERSISTENCE_MINIMAP_EXPORTER_H_
#define RME_EDITOR_PERSISTENCE_MINIMAP_EXPORTER_H_

#include "app/definitions.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

class Editor;

enum class MinimapExportFormat : uint8_t {
	Otmm,
	Jpg,
	Webp,
};

// Largest image side each encoder accepts
inline constexpr int kMaxWebpDimension = 16383;
inline constexpr int kMaxJpegDimension = 65500;

using MinimapExportFloorMask = std::array<bool, MAP_LAYERS>;

[[nodiscard]] constexpr MinimapExportFloorMask allMinimapExportFloors() {
	MinimapExportFloorMask floors {};
	floors.fill(true);
	return floors;
}

struct MinimapExportOptions {
	std::filesystem::path outputDirectory;
	std::string fileBaseName;
	MinimapExportFormat format = MinimapExportFormat::Otmm;
	MinimapExportFloorMask selectedFloors = allMinimapExportFloors();
	int imageSize = 1024;
	bool showAllFloors = false;
	bool applyShadeToAdjacentFloors = false;
};

struct MinimapExportResult {
	bool ok = false;
	size_t filesWritten = 0;
	std::string error;
};

class MinimapExporter {
public:
	using ProgressCallback = std::function<void(uint64_t completed, uint64_t total)>;

	[[nodiscard]] static MinimapExportResult Export(Editor& editor, const MinimapExportOptions& options, ProgressCallback progress = {});
};

#endif
