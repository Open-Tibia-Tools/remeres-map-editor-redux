//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_TILE_DESCRIBER_H_
#define RME_RENDERING_TILE_DESCRIBER_H_

#include <string>

class Tile;

class TileDescriber {
public:
	static std::string GetDescription(Tile* tile, bool showSpawns, bool showCreatures);
};

#endif
