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

#ifndef RME_TILE_SERIALIZATION_OTBM_H_
#define RME_TILE_SERIALIZATION_OTBM_H_

#include <functional>

class Map;
class BinaryNode;
class NodeFileWriteHandle;
class Tile;
class IOMapOTBM;
class FastOTBMNode;

class TileSerializationOTBM {
public:
	static void readTileArea(IOMapOTBM& iomap, Map& map, BinaryNode* mapNode);
	static void readTileAreaFast(IOMapOTBM& iomap, Map& map, FastOTBMNode& mapNode, const std::array<size_t, 16>* cell_indices, std::vector<std::pair<uint32_t, Tile*>>& out_house_tiles, uint64_t& out_tile_count);
	static void writeTileData(const IOMapOTBM& iomap, const Map& map, NodeFileWriteHandle& f, const std::function<void(int)>& progressCb = nullptr);
	static void serializeTile(const IOMapOTBM& iomap, const Tile* tile, NodeFileWriteHandle& f);
};

#endif
