//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_STORE_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_STORE_H_

#include "palette/quick_tilesets/quick_tileset_model.h"

#include <wx/string.h>

#include <functional>
#include <optional>
#include <string>

namespace QuickTilesets {

	// Quick Tilesets state of the loaded client version, persisted in one JSON file shared by all
	// client versions ({"schema":1,"versions":{"<client id>":State}}). Never shows dialogs: errors
	// are returned as text for the caller to report.
	class Store {
	public:
		// Loads the section of `version` from `path`. When the file cannot be read or parsed the
		// state is empty, the store becomes read-only (the file is never overwritten) and the
		// error is returned.
		std::optional<std::string> Load(wxString path, std::string version);
		void Unload();

		[[nodiscard]] bool IsLoaded() const {
			return loaded_;
		}
		[[nodiscard]] const std::string& Version() const {
			return version_;
		}
		[[nodiscard]] bool IsReadOnly() const {
			return read_only_;
		}
		[[nodiscard]] const State& GetState() const {
			return state_;
		}

		struct MutateResult {
			bool changed = false;
			// Set on the first failed save; the store is read-only from then on and later
			// changes are kept in memory only.
			std::optional<std::string> error;
		};

		// Applies `change` (returns whether it changed anything) and saves synchronously.
		MutateResult Mutate(const std::function<bool(State&)>& change);

	private:
		// Re-reads the file, replaces only this version's section and commits atomically.
		std::optional<std::string> Save() const;

		wxString path_;
		std::string version_;
		State state_;
		bool loaded_ = false;
		bool read_only_ = false;
	};

	// Unique id for a new tab (16 hex characters).
	[[nodiscard]] std::string NewTabId();

} // namespace QuickTilesets

#endif
