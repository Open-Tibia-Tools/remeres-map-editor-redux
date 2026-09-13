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

#ifndef RME_MAP_LAYER_DRAWER_H
#define RME_MAP_LAYER_DRAWER_H

#include <iosfwd>

class Editor;
class TileRenderer;
class GridDrawer;
struct RenderView;
struct DrawingOptions;
struct LightBuffer;
struct RenderFrameContext;
class SpriteBatch;
class PrimitiveRenderer;

namespace rme::rendering {
class RenderChunkCache;
}

class MapLayerDrawer {
public:
	MapLayerDrawer(TileRenderer* tile_renderer, GridDrawer* grid_drawer, Editor* editor, rme::rendering::RenderChunkCache* chunk_cache = nullptr);
	~MapLayerDrawer();

	void SetChunkCache(rme::rendering::RenderChunkCache* chunk_cache) {
		chunk_cache_ = chunk_cache;
	}

	void Draw(SpriteBatch& sprite_batch, int map_z, bool live_client, const RenderFrameContext& ctx, LightBuffer& light_buffer, bool light_collection_only = false);
	void Draw(SpriteBatch& sprite_batch, int map_z, bool live_client, const RenderView& view, const DrawingOptions& options, LightBuffer& light_buffer, bool light_collection_only = false);

private:
	TileRenderer* tile_renderer;
	GridDrawer* grid_drawer;
	Editor* editor;
	rme::rendering::RenderChunkCache* chunk_cache_ = nullptr;
};

#endif
