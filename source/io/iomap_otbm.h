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
#ifndef RME_IOMAP_OTBM_H_
#define RME_IOMAP_OTBM_H_

#include <vector>
#include <wx/datetime.h>
#include <wx/string.h>
#include "io/iomap.h"
#include "io/filehandle.h"
#include "io/otbm/otbm_types.h"

class BinaryNode;

// Pragma pack is VERY important since otherwise it won't be able to load the structs correctly
#pragma pack(1)

enum PodiumFlags : uint8_t {
	PODIUM_SHOW_PLATFORM = 1 << 0, // show the platform below the outfit
	PODIUM_SHOW_OUTFIT = 1 << 1, // show outfit
	PODIUM_SHOW_MOUNT = 1 << 2 // show mount
};

struct OTBM_root_header {
	uint32_t version;
	uint16_t width;
	uint16_t height;
	uint32_t majorVersionItems;
	uint32_t minorVersionItems;
};

struct OTBM_TeleportDest {
	uint16_t x;
	uint16_t y;
	uint8_t z;
};

struct OTBM_Tile_area_coords {
	uint16_t x;
	uint16_t y;
	uint8_t z;
};

struct OTBM_Tile_coords {
	uint8_t x;
	uint8_t y;
};

struct OTBM_TownTemple_coords {
	uint16_t x;
	uint16_t y;
	uint8_t z;
};

struct OTBM_HouseTile_coords {
	uint8_t x;
	uint8_t y;
	uint32_t houseid;
};

struct OTBMStartupPeekResult {
	wxString map_name;
	wxString client_version = "Placeholder";
	uint16_t width = 0;
	uint16_t height = 0;
	int otbm_version = 0;
	uint32_t items_major_version = 0;
	uint32_t items_minor_version = 0;
	wxString house_xml_file;
	wxString spawn_xml_file;
	wxString description;
	wxDateTime modified_time;
	bool has_error = false;
	wxString error_message;
};

#pragma pack()

class IOMapOTBM : public IOMap {
public:
	IOMapOTBM(MapVersion ver) {
		version = ver;
	}
	~IOMapOTBM() override = default;

	using WriteResult = OTBMWriteResult;

	static bool getVersionInfo(const FileName& identifier, MapVersion& out_ver);
	static bool peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info);

	bool loadMap(Map& map, const FileName& identifier) override;
	bool saveMap(Map& map, const FileName& identifier) override;

protected:
	static bool getVersionInfo(NodeFileReadHandle* f, MapVersion& out_ver);

	bool loadMapFromDisk(Map& map, const FileName& identifier);
	bool loadMapFast(Map& map, const uint8_t* data, size_t size);

	bool loadMap(Map& map, NodeFileReadHandle& handle);
	bool loadMapRoot(Map& map, NodeFileReadHandle& f, BinaryNode*& root, BinaryNode*& mapHeaderNode);
	bool readMapAttributes(Map& map, BinaryNode* mapHeaderNode);
	void readMapNodes(Map& map, NodeFileReadHandle& f, BinaryNode* mapHeaderNode);

	void readTileArea(Map& map, BinaryNode* mapNode);
	void readTowns(Map& map, BinaryNode* mapNode);
	void readWaypoints(Map& map, BinaryNode* mapNode);

	bool saveMapToDisk(Map& map, const FileName& identifier);

	bool saveMap(Map& map, NodeFileWriteHandle& handle);

	void writeTileData(const Map& map, NodeFileWriteHandle& f);
	void writeTowns(const Map& map, NodeFileWriteHandle& f);
	WriteResult writeWaypoints(const Map& map, NodeFileWriteHandle& f, MapVersion mapVersion);
};

#endif
