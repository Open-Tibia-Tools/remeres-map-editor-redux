//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IOMAP_SEC_H_
#define RME_IOMAP_SEC_H_

#include "io/iomap.h"

#include <filesystem>

class ClientVersion;
struct OTBMStartupPeekResult;

// Opens maps of the original game server: a folder of sector files
// (`XXXX-YYYY-ZZ.sec`), each a 32x32 block of tiles on one floor. Opening any
// sector file, or the folder itself, loads every sector in that folder.
//
// The files hold server type IDs, so the map is read with a client version
// in DatSrv mode: Tibia.dat / Tibia.spr from the client folder and the item
// types from the server's own objects.srv. Nothing comes from the editor's
// data directory.
//
// Saving writes the sector files back in the server's own format and, in
// the server data folder that holds the map folder, the files around it:
// monster homes (dat/monster.db), houses (dat/houses.dat), towns and marks
// (dat/map.dat) and NPC homes (npc/*.npc). Files whose content did not
// change are left alone. Each of those files has its own handler in
// io/sec/data/. Only the sector files and dat/objects.srv are needed to open
// a map; a folder without the others gets them once the map has towns,
// houses or monsters, and a dat/ folder with map.dat and a copy of
// objects.srv when it has none.
//
// Loading also reads the tiles that dat/moveuse.dat, the raids (mon/*.evt)
// and the start positions in dat/map.dat refer to into Map::server_markers.
// They are shown on the map and never saved.
class IOMapSec : public IOMap {
public:
	// True for a sector file, or for a folder that contains sector files.
	[[nodiscard]] static bool isSectorMap(const FileName& identifier);

	// The folder holding the sector files of `identifier`.
	[[nodiscard]] static std::filesystem::path sectorFolder(const FileName& identifier);

	// True when saving to `identifier` means writing sector files: a folder,
	// or any `.sec` file name (its folder is the one written).
	[[nodiscard]] static bool isSectorTarget(const FileName& identifier);

	// The server's objects.srv: `<data>/dat/objects.srv` next to the map folder.
	[[nodiscard]] static wxFileName objectsFile(const FileName& identifier);

	// A DatSrv client version, preferring the loaded one and then one with a
	// configured client folder; nullptr when none exists.
	[[nodiscard]] static ClientVersion* findClientVersion();

	// Fills the startup screen's map preview from the folder listing alone.
	static bool peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info);

	bool loadMap(Map& map, const FileName& identifier) override;
	bool saveMap(Map& map, const FileName& identifier) override;
};

#endif
