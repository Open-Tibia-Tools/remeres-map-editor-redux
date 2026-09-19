//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_atlas_lut.h"
#include "rendering/core/texture_atlas.h"
#include "rendering/core/atlas_manager.h"
#include <algorithm>
#include <spdlog/spdlog.h>

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

	glCreateBuffers(1, &ssbo_);
	if (ssbo_ == 0) {
		spdlog::error("[SpriteAtlasLUT] Failed to create SSBO buffer");
		return false;
	}

	glNamedBufferData(ssbo_, static_cast<GLsizeiptr>(cpu_entries_.size() * sizeof(SpriteLUTEntry)), cpu_entries_.data(), GL_DYNAMIC_DRAW);

	gpu_capacity_ = cpu_entries_.size();
	has_dirty_entries_ = false;
	dirty_min_id_ = UINT32_MAX;
	dirty_max_id_ = 0;

	spdlog::info("[SpriteAtlasLUT] Initialized SSBO with capacity {} entries ({} KB GPU buffer) | SSBO ID: {}",
		cpu_entries_.size(), (cpu_entries_.size() * sizeof(SpriteLUTEntry)) / 1024, ssbo_);
	return true;
}

void SpriteAtlasLUT::release() {
	if (ssbo_ != 0) {
		spdlog::info("[SpriteAtlasLUT] Released SSBO buffer ID {} (capacity was {} entries)", ssbo_, gpu_capacity_);
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

	const size_t old_capacity = cpu_entries_.size();
	size_t new_capacity = cpu_entries_.size() == 0 ? DEFAULT_INITIAL_CAPACITY : cpu_entries_.size();
	while (new_capacity < required_capacity) {
		new_capacity *= 2;
	}

	cpu_entries_.resize(new_capacity);

	if (ssbo_ != 0) {
		glNamedBufferData(ssbo_, static_cast<GLsizeiptr>(cpu_entries_.size() * sizeof(SpriteLUTEntry)), cpu_entries_.data(), GL_DYNAMIC_DRAW);
		gpu_capacity_ = cpu_entries_.size();
		has_dirty_entries_ = false;
		dirty_min_id_ = UINT32_MAX;
		dirty_max_id_ = 0;

		spdlog::info("[SpriteAtlasLUT] Capacity expanded: {} -> {} entries ({} KB GPU buffer) | SSBO ID: {}",
			old_capacity, new_capacity, (new_capacity * sizeof(SpriteLUTEntry)) / 1024, ssbo_);
	}
}

void SpriteAtlasLUT::updateSprite(uint32_t sprite_id, const AtlasRegion& region) {
	uint32_t slot = sprite_id;
	if (sprite_id == AtlasRegion::INVALID_SENTINEL || sprite_id == AtlasManager::WHITE_PIXEL_ID) {
		slot = WHITE_PIXEL_LUT_INDEX;
	}

	if (slot >= MAX_SUPPORTED_SPRITES) {
		return;
	}

	ensureCapacity(static_cast<size_t>(slot) + 1);

	SpriteLUTEntry& entry = cpu_entries_[slot];
	entry.u_min = region.u_min;
	entry.v_min = region.v_min;
	entry.u_max = region.u_max;
	entry.v_max = region.v_max;
	entry.layer = static_cast<float>(region.atlas_index);
	entry.valid = 1.0f;

	dirty_min_id_ = std::min(dirty_min_id_, slot);
	dirty_max_id_ = std::max(dirty_max_id_, slot);
	has_dirty_entries_ = true;
}

void SpriteAtlasLUT::invalidateSprite(uint32_t sprite_id) {
	uint32_t slot = sprite_id;
	if (sprite_id == AtlasRegion::INVALID_SENTINEL || sprite_id == AtlasManager::WHITE_PIXEL_ID) {
		slot = WHITE_PIXEL_LUT_INDEX;
	}

	if (slot >= cpu_entries_.size()) {
		return;
	}

	cpu_entries_[slot].valid = 0.0f;
	dirty_min_id_ = std::min(dirty_min_id_, slot);
	dirty_max_id_ = std::max(dirty_max_id_, slot);
	has_dirty_entries_ = true;
}

void SpriteAtlasLUT::flush() {
	if (!has_dirty_entries_ || ssbo_ == 0 || dirty_min_id_ > dirty_max_id_) {
		return;
	}

	const size_t offset = dirty_min_id_ * sizeof(SpriteLUTEntry);
	const size_t size = (dirty_max_id_ - dirty_min_id_ + 1) * sizeof(SpriteLUTEntry);

	glNamedBufferSubData(ssbo_, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), cpu_entries_.data() + dirty_min_id_);

	has_dirty_entries_ = false;
	dirty_min_id_ = UINT32_MAX;
	dirty_max_id_ = 0;
}

void SpriteAtlasLUT::bind(GLuint binding_point) {
	if (ssbo_ != 0) {
		if (has_dirty_entries_) {
			flush();
		}
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
