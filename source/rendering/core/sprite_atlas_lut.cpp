//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_atlas_lut.h"
#include "rendering/core/texture_atlas.h"
#include <algorithm>

SpriteAtlasLUT::SpriteAtlasLUT() = default;

SpriteAtlasLUT::~SpriteAtlasLUT() {
	release();
}

SpriteAtlasLUT::SpriteAtlasLUT(SpriteAtlasLUT&& other) noexcept :
	ssbo_(other.ssbo_),
	cpu_entries_(std::move(other.cpu_entries_)),
	gpu_capacity_(other.gpu_capacity_),
	dirty_min_id_(other.dirty_min_id_),
	dirty_max_id_(other.dirty_max_id_),
	has_dirty_entries_(other.has_dirty_entries_) {
	other.ssbo_ = 0;
	other.gpu_capacity_ = 0;
	other.dirty_min_id_ = UINT32_MAX;
	other.dirty_max_id_ = 0;
	other.has_dirty_entries_ = false;
}

SpriteAtlasLUT& SpriteAtlasLUT::operator=(SpriteAtlasLUT&& other) noexcept {
	if (this != &other) {
		release();
		ssbo_ = other.ssbo_;
		cpu_entries_ = std::move(other.cpu_entries_);
		gpu_capacity_ = other.gpu_capacity_;
		dirty_min_id_ = other.dirty_min_id_;
		dirty_max_id_ = other.dirty_max_id_;
		has_dirty_entries_ = other.has_dirty_entries_;

		other.ssbo_ = 0;
		other.gpu_capacity_ = 0;
		other.dirty_min_id_ = UINT32_MAX;
		other.dirty_max_id_ = 0;
		other.has_dirty_entries_ = false;
	}
	return *this;
}

bool SpriteAtlasLUT::initialize(size_t initial_capacity) {
	release();

	cpu_entries_.resize(std::max(initial_capacity, DEFAULT_INITIAL_CAPACITY));

	glGenBuffers(1, &ssbo_);
	if (ssbo_ == 0) {
		return false;
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(cpu_entries_.size() * sizeof(SpriteLUTEntry)), cpu_entries_.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	gpu_capacity_ = cpu_entries_.size();
	has_dirty_entries_ = false;
	dirty_min_id_ = UINT32_MAX;
	dirty_max_id_ = 0;
	return true;
}

void SpriteAtlasLUT::release() {
	if (ssbo_ != 0) {
		glDeleteBuffers(1, &ssbo_);
		ssbo_ = 0;
	}
	cpu_entries_.clear();
	gpu_capacity_ = 0;
	has_dirty_entries_ = false;
	dirty_min_id_ = UINT32_MAX;
	dirty_max_id_ = 0;
}

void SpriteAtlasLUT::ensureCapacity(size_t required_capacity) {
	if (required_capacity <= cpu_entries_.size()) {
		return;
	}

	size_t new_capacity = cpu_entries_.size() == 0 ? DEFAULT_INITIAL_CAPACITY : cpu_entries_.size();
	while (new_capacity < required_capacity) {
		new_capacity *= 2;
	}

	cpu_entries_.resize(new_capacity);

	if (ssbo_ != 0) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(cpu_entries_.size() * sizeof(SpriteLUTEntry)), cpu_entries_.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		gpu_capacity_ = cpu_entries_.size();
		has_dirty_entries_ = false;
		dirty_min_id_ = UINT32_MAX;
		dirty_max_id_ = 0;
	}
}

void SpriteAtlasLUT::updateSprite(uint32_t sprite_id, const AtlasRegion& region) {
	ensureCapacity(sprite_id + 1);

	SpriteLUTEntry& entry = cpu_entries_[sprite_id];
	entry.u_min = region.u_min;
	entry.v_min = region.v_min;
	entry.u_max = region.u_max;
	entry.v_max = region.v_max;
	entry.layer = static_cast<float>(region.atlas_index);
	entry.valid = 1.0f;

	dirty_min_id_ = std::min(dirty_min_id_, sprite_id);
	dirty_max_id_ = std::max(dirty_max_id_, sprite_id);
	has_dirty_entries_ = true;
}

void SpriteAtlasLUT::invalidateSprite(uint32_t sprite_id) {
	if (sprite_id >= cpu_entries_.size()) {
		return;
	}

	cpu_entries_[sprite_id].valid = 0.0f;
	dirty_min_id_ = std::min(dirty_min_id_, sprite_id);
	dirty_max_id_ = std::max(dirty_max_id_, sprite_id);
	has_dirty_entries_ = true;
}

void SpriteAtlasLUT::flush() {
	if (!has_dirty_entries_ || ssbo_ == 0 || dirty_min_id_ > dirty_max_id_) {
		return;
	}

	const size_t offset = dirty_min_id_ * sizeof(SpriteLUTEntry);
	const size_t size = (dirty_max_id_ - dirty_min_id_ + 1) * sizeof(SpriteLUTEntry);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), cpu_entries_.data() + dirty_min_id_);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	has_dirty_entries_ = false;
	dirty_min_id_ = UINT32_MAX;
	dirty_max_id_ = 0;
}

void SpriteAtlasLUT::bind(GLuint binding_point) const {
	if (ssbo_ != 0) {
		const_cast<SpriteAtlasLUT*>(this)->flush();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, ssbo_);
	}
}

void SpriteAtlasLUT::unbind(GLuint binding_point) const {
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, 0);
}

const SpriteLUTEntry* SpriteAtlasLUT::getEntry(uint32_t sprite_id) const {
	if (sprite_id < cpu_entries_.size()) {
		return &cpu_entries_[sprite_id];
	}
	return nullptr;
}
