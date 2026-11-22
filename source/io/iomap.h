//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_IO_H_
#define RME_MAP_IO_H_

#include "app/client_version.h"
#include <filesystem>
#include <functional>
#include <string_view>

enum ImportType {
	IMPORT_DONT,
	IMPORT_MERGE,
	IMPORT_SMART_MERGE,
	IMPORT_INSERT,
};

class Map;

class IOMap {
protected:
	std::vector<std::string> warnings;
	std::string errorstr;

public:
	IOMap() {
		version.otbm = MAP_OTBM_1;
		version.client = OTB_VERSION_NONE;
	}
	virtual ~IOMap() = default;

	MapVersion version;

	std::vector<std::string>& getWarnings() {
		return warnings;
	}
	const std::vector<std::string>& getWarnings() const {
		return warnings;
	}
	std::string& getError() {
		return errorstr;
	}
	const std::string& getError() const {
		return errorstr;
	}

	using ProgressCallback = std::function<void(int percent, std::string_view message)>;

	void setProgressCallback(ProgressCallback cb) {
		progress_callback = std::move(cb);
	}
	void reportProgress(int percent, std::string_view message = "") const {
		if (progress_callback) {
			progress_callback(percent, message);
		}
	}

	virtual bool loadMap(Map& map, const std::filesystem::path& identifier) = 0;
	virtual bool saveMap(Map& map, const std::filesystem::path& identifier) = 0;

protected:
	ProgressCallback progress_callback;
};

class VirtualIOMap : public IOMap {
public:
	VirtualIOMap(MapVersion v) {
		version = v;
	}

	bool loadMap(Map& map, const std::filesystem::path& identifier) override {
		return false;
	}
	bool saveMap(Map& map, const std::filesystem::path& identifier) override {
		return false;
	}
};

#endif
