//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tileset_store.h"

#include <wx/file.h>
#include <wx/filefn.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <random>

namespace QuickTilesets {

	namespace {

		std::filesystem::path ToPath(const wxString& path) {
			return std::filesystem::path(path.ToStdWstring());
		}

		// The current file content, or an empty document when there is no file yet.
		nlohmann::json ReadDocument(const wxString& path) {
			if (!wxFileExists(path)) {
				return EmptyDocument();
			}
			std::ifstream input(ToPath(path), std::ios::binary);
			if (!input) {
				throw std::runtime_error("The Quick Tilesets file cannot be read.");
			}
			nlohmann::json document = nlohmann::json::parse(input);
			ValidateDocument(document);
			return document;
		}

	} // namespace

	std::optional<std::string> Store::Load(wxString path, std::string version) {
		path_ = std::move(path);
		version_ = std::move(version);
		state_ = {};
		loaded_ = true;
		read_only_ = false;
		try {
			state_ = DecodeVersion(ReadDocument(path_), version_);
		} catch (const std::exception& error) {
			state_ = {};
			read_only_ = true;
			return std::format("Quick Tilesets could not be loaded from \"{}\": {} The file is left unchanged and changes will not be saved this session.", path_.ToStdString(wxConvUTF8), error.what());
		}
		return std::nullopt;
	}

	void Store::Unload() {
		path_.clear();
		version_.clear();
		state_ = {};
		loaded_ = false;
		read_only_ = false;
	}

	Store::MutateResult Store::Mutate(const std::function<bool(State&)>& change) {
		MutateResult result;
		if (!loaded_) {
			return result;
		}
		result.changed = change(state_);
		if (!result.changed || read_only_) {
			return result;
		}
		if (auto error = Save()) {
			read_only_ = true;
			result.error = std::move(error);
		}
		return result;
	}

	std::optional<std::string> Store::Save() const {
		try {
			nlohmann::json document = ReadDocument(path_);
			MergeVersionSection(document, version_, state_);

			wxTempFile output(path_);
			const std::string text = document.dump(2);
			if (!output.IsOpened() || !output.Write(text.data(), text.size()) || !output.Commit()) {
				throw std::runtime_error("The file could not be written; the previous file is untouched.");
			}
		} catch (const std::exception& error) {
			return std::format("Quick Tilesets could not be saved to \"{}\": {} Further changes are kept until the editor closes.", path_.ToStdString(wxConvUTF8), error.what());
		}
		return std::nullopt;
	}

	std::string NewTabId() {
		static std::mt19937_64 generator { std::random_device {}() };
		return std::format("{:016x}", generator());
	}

} // namespace QuickTilesets
