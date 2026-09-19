#ifndef RME_EDITOR_FACTORY_H
#define RME_EDITOR_FACTORY_H
#include <memory>
#include "app/definitions.h"
#include "app/client_version.h"
#include "editor/persistence/map_load_options.h"
#include <filesystem>

#include <functional>
#include <string_view>

class Editor;
class CopyBuffer;
class LiveClient;

class EditorFactory {
public:
	using ProgressCallback = std::function<void(int percent, std::string_view message)>;

	// Creates a new empty map, may prompt for version if not set
	static std::unique_ptr<Editor> CreateEmpty(CopyBuffer& copybuffer);

	// Loads a map from file, handles version checking/switching
	static std::unique_ptr<Editor> LoadFromFile(
		CopyBuffer& copybuffer,
		const std::filesystem::path& fn,
		const MapLoadOptions& load_options = {},
		ProgressCallback on_progress = nullptr);

	// Joins a live session
	static std::unique_ptr<Editor> JoinLive(CopyBuffer& copybuffer, std::unique_ptr<LiveClient> client);

private:
	// Helper to ensure the correct version is loaded in g_gui
	static bool EnsureVersion(ClientVersionID version);
};

#endif
