//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/chunk_mega_buffer.h"
#include "rendering/core/shared_geometry.h"
#include <algorithm>
#include <cstddef>

ChunkMegaBuffer::ChunkMegaBuffer() = default;

ChunkMegaBuffer::~ChunkMegaBuffer() {
	release();
}

ChunkMegaBuffer::ChunkMegaBuffer(ChunkMegaBuffer&& other) noexcept :
	vao_(other.vao_),
	vbo_(other.vbo_),
	total_capacity_(other.total_capacity_),
	allocated_instances_(other.allocated_instances_),
	free_blocks_(std::move(other.free_blocks_)) {
	other.vao_ = 0;
	other.vbo_ = 0;
	other.total_capacity_ = 0;
	other.allocated_instances_ = 0;
}

ChunkMegaBuffer& ChunkMegaBuffer::operator=(ChunkMegaBuffer&& other) noexcept {
	if (this != &other) {
		release();
		vao_ = other.vao_;
		vbo_ = other.vbo_;
		total_capacity_ = other.total_capacity_;
		allocated_instances_ = other.allocated_instances_;
		free_blocks_ = std::move(other.free_blocks_);

		other.vao_ = 0;
		other.vbo_ = 0;
		other.total_capacity_ = 0;
		other.allocated_instances_ = 0;
	}
	return *this;
}

bool ChunkMegaBuffer::initialize(size_t max_instances) {
	release();

	total_capacity_ = max_instances;
	allocated_instances_ = 0;

	// Initial single free block spanning entire buffer
	free_blocks_.push_back(FreeBlock{ 0, static_cast<uint32_t>(total_capacity_) });

	if (!SharedGeometry::Instance().initialize()) {
		return false;
	}

	glCreateBuffers(1, &vbo_);
	if (vbo_ == 0) {
		return false;
	}

	glNamedBufferStorage(vbo_, static_cast<GLsizeiptr>(total_capacity_ * sizeof(TileInstance)), nullptr, GL_DYNAMIC_STORAGE_BIT);

	glCreateVertexArrays(1, &vao_);
	if (vao_ == 0) {
		release();
		return false;
	}

	// Binding 0: Static unit quad geometry
	glVertexArrayVertexBuffer(vao_, 0, SharedGeometry::Instance().getQuadVBO(), 0, 4 * sizeof(float));
	glVertexArrayElementBuffer(vao_, SharedGeometry::Instance().getQuadEBO());

	// Loc 0: Quad pos (vec2)
	glEnableVertexArrayAttrib(vao_, 0);
	glVertexArrayAttribFormat(vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao_, 0, 0);

	// Loc 1: Quad texcoord (vec2)
	glEnableVertexArrayAttrib(vao_, 1);
	glVertexArrayAttribFormat(vao_, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
	glVertexArrayAttribBinding(vao_, 1, 0);

	// Binding 1: Instance data from MegaBuffer
	glVertexArrayVertexBuffer(vao_, 1, vbo_, 0, sizeof(TileInstance));
	glVertexArrayBindingDivisor(vao_, 1, 1);

	// Loc 2: aRect (vec4: x, y, w, h)
	glEnableVertexArrayAttrib(vao_, 2);
	glVertexArrayAttribFormat(vao_, 2, 4, GL_FLOAT, GL_FALSE, offsetof(TileInstance, x));
	glVertexArrayAttribBinding(vao_, 2, 1);

	// Loc 3: aSpriteId (uint)
	glEnableVertexArrayAttrib(vao_, 3);
	glVertexArrayAttribIFormat(vao_, 3, 1, GL_UNSIGNED_INT, offsetof(TileInstance, sprite_id));
	glVertexArrayAttribBinding(vao_, 3, 1);

	// Loc 4: aFlags (uint)
	glEnableVertexArrayAttrib(vao_, 4);
	glVertexArrayAttribIFormat(vao_, 4, 1, GL_UNSIGNED_INT, offsetof(TileInstance, flags));
	glVertexArrayAttribBinding(vao_, 4, 1);

	// Loc 5: aTint (vec4: r, g, b, a)
	glEnableVertexArrayAttrib(vao_, 5);
	glVertexArrayAttribFormat(vao_, 5, 4, GL_FLOAT, GL_FALSE, offsetof(TileInstance, r));
	glVertexArrayAttribBinding(vao_, 5, 1);

	return true;
}

void ChunkMegaBuffer::release() {
	if (vao_ != 0) {
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}
	if (vbo_ != 0) {
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}
	free_blocks_.clear();
	total_capacity_ = 0;
	allocated_instances_ = 0;
}

SlabSlice ChunkMegaBuffer::allocate(uint32_t required_instances) {
	if (required_instances == 0) {
		return SlabSlice{};
	}

	// Align up to block alignment
	const uint32_t aligned_size = ((required_instances + BLOCK_ALIGNMENT - 1) / BLOCK_ALIGNMENT) * BLOCK_ALIGNMENT;

	for (size_t i = 0; i < free_blocks_.size(); ++i) {
		if (free_blocks_[i].size >= aligned_size) {
			const uint32_t offset = free_blocks_[i].offset;

			if (free_blocks_[i].size == aligned_size) {
				free_blocks_.erase(free_blocks_.begin() + i);
			} else {
				free_blocks_[i].offset += aligned_size;
				free_blocks_[i].size -= aligned_size;
			}

			allocated_instances_ += aligned_size;
			return SlabSlice{ offset, aligned_size, 0 };
		}
	}

	// Out of memory in mega-buffer
	return SlabSlice{};
}

void ChunkMegaBuffer::free(SlabSlice& slice) {
	if (!slice.isValid()) {
		return;
	}

	const uint32_t offset = slice.base_instance;
	const uint32_t size = slice.capacity;
	allocated_instances_ -= std::min(allocated_instances_, static_cast<size_t>(size));

	// Insert into free blocks maintaining offset order
	auto it = std::lower_bound(free_blocks_.begin(), free_blocks_.end(), offset, [](const FreeBlock& b, uint32_t val) {
		return b.offset < val;
	});

	it = free_blocks_.insert(it, FreeBlock{ offset, size });

	// Merge with next block if adjacent
	if (it + 1 != free_blocks_.end() && it->offset + it->size == (it + 1)->offset) {
		it->size += (it + 1)->size;
		free_blocks_.erase(it + 1);
	}

	// Merge with previous block if adjacent
	if (it != free_blocks_.begin() && (it - 1)->offset + (it - 1)->size == it->offset) {
		(it - 1)->size += it->size;
		free_blocks_.erase(it);
	}

	slice = SlabSlice{};
}

void ChunkMegaBuffer::upload(SlabSlice& slice, const TileInstance* instances, uint32_t count) {
	if (!slice.isValid() || !instances || count == 0 || vbo_ == 0) {
		return;
	}

	const uint32_t upload_count = std::min(count, slice.capacity);
	glNamedBufferSubData(vbo_, static_cast<GLintptr>(slice.base_instance * sizeof(TileInstance)), static_cast<GLsizeiptr>(upload_count * sizeof(TileInstance)), instances);
	slice.count = upload_count;
}

void ChunkMegaBuffer::bindVAO() const {
	if (vao_ != 0) {
		glBindVertexArray(vao_);
	}
}

void ChunkMegaBuffer::unbindVAO() const {
	glBindVertexArray(0);
}
