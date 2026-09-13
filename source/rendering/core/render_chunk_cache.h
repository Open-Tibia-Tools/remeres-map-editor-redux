#ifndef RME_RENDERING_CORE_RENDER_CHUNK_CACHE_H_
#define RME_RENDERING_CORE_RENDER_CHUNK_CACHE_H_

#include "rendering/core/render_chunk.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace rme::rendering {

class RenderChunkCache {
public:
	RenderChunkCache() = default;
	~RenderChunkCache() = default;

	RenderChunkCache(const RenderChunkCache&) = delete;
	RenderChunkCache& operator=(const RenderChunkCache&) = delete;
	RenderChunkCache(RenderChunkCache&&) noexcept = default;
	RenderChunkCache& operator=(RenderChunkCache&&) noexcept = default;

	[[nodiscard]] RenderChunk* getChunk(int cx, int cy, int floor);
	RenderChunk& getOrCreateChunk(int cx, int cy, int floor);

	void markDirty(int map_x, int map_y, int floor);
	void markAllDirty();
	void clear();

	[[nodiscard]] size_t size() const noexcept {
		return chunks_.size();
	}

private:
	std::unordered_map<uint64_t, std::unique_ptr<RenderChunk>> chunks_;
};

} // namespace rme::rendering

#endif
