//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_CHUNK_MEGA_BUFFER_H_
#define RME_RENDERING_CORE_CHUNK_MEGA_BUFFER_H_

#include <glad/glad.h>
#include "rendering/core/tile_instance.h"
#include <cstdint>
#include <vector>

struct SlabSlice {
	uint32_t base_instance = 0;
	uint32_t capacity = 0;
	uint32_t count = 0;

	[[nodiscard]] constexpr bool isValid() const noexcept {
		return capacity > 0;
	}
};

/**
 * Slab-allocated mega-buffer managing persistent GPU memory for cached chunks.
 * Solves ImguiMapEditor's RAM flaw by avoiding 1 VBO handle per chunk.
 */
class ChunkMegaBuffer {
public:
	// Default capacity: 1,048,576 instances * 48 bytes = ~50MB VRAM
	static constexpr size_t DEFAULT_CAPACITY = 1048576;
	static constexpr uint32_t BLOCK_ALIGNMENT = 128; // Align allocations to 128 instances

	ChunkMegaBuffer();
	~ChunkMegaBuffer();

	ChunkMegaBuffer(const ChunkMegaBuffer&) = delete;
	ChunkMegaBuffer& operator=(const ChunkMegaBuffer&) = delete;
	ChunkMegaBuffer(ChunkMegaBuffer&& other) noexcept;
	ChunkMegaBuffer& operator=(ChunkMegaBuffer&& other) noexcept;

	bool initialize(size_t max_instances = DEFAULT_CAPACITY);
	void release();

	/**
	 * Sub-allocate a slice of instances for a chunk.
	 */
	[[nodiscard]] SlabSlice allocate(uint32_t required_instances);

	/**
	 * Free a slice back to the slab allocator.
	 */
	void free(SlabSlice& slice);

	/**
	 * Upload instance data into an allocated slab slice.
	 */
	void upload(SlabSlice& slice, const TileInstance* instances, uint32_t count);

	/**
	 * Bind VAO for rendering chunks.
	 */
	void bindVAO() const;
	void unbindVAO() const;

	[[nodiscard]] GLuint getVAO() const noexcept {
		return vao_;
	}
	[[nodiscard]] GLuint getVBO() const noexcept {
		return vbo_;
	}
	[[nodiscard]] size_t getTotalCapacity() const noexcept {
		return total_capacity_;
	}
	[[nodiscard]] size_t getAllocatedInstances() const noexcept {
		return allocated_instances_;
	}
	[[nodiscard]] bool isValid() const noexcept {
		return vao_ != 0 && vbo_ != 0;
	}

private:
	struct FreeBlock {
		uint32_t offset;
		uint32_t size;
	};

	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	size_t total_capacity_ = 0;
	size_t allocated_instances_ = 0;

	std::vector<FreeBlock> free_blocks_;
};

#endif
