#include "rendering/core/multi_draw_indirect_renderer.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <utility>

MultiDrawIndirectRenderer::MultiDrawIndirectRenderer() {
	commands_.reserve(DEFAULT_MAX_COMMANDS);
}

MultiDrawIndirectRenderer::~MultiDrawIndirectRenderer() {
	cleanup();
}

MultiDrawIndirectRenderer::MultiDrawIndirectRenderer(MultiDrawIndirectRenderer&& other) noexcept
	:
	commands_(std::move(other.commands_)),
	command_buffer_(std::move(other.command_buffer_)),
	max_commands_(other.max_commands_),
	available_(other.available_),
	initialized_(other.initialized_) {
	other.available_ = false;
	other.initialized_ = false;
}

MultiDrawIndirectRenderer& MultiDrawIndirectRenderer::operator=(MultiDrawIndirectRenderer&& other) noexcept {
	if (this != &other) {
		cleanup();
		commands_ = std::move(other.commands_);
		command_buffer_ = std::move(other.command_buffer_);
		max_commands_ = other.max_commands_;
		available_ = other.available_;
		initialized_ = other.initialized_;
		other.available_ = false;
		other.initialized_ = false;
	}
	return *this;
}

bool MultiDrawIndirectRenderer::initialize(int max_commands) {
	if (initialized_) {
		return true;
	}

	max_commands_ = max_commands > 0 ? max_commands : DEFAULT_MAX_COMMANDS;
	commands_.reserve(max_commands_);

	// Runtime-only check - function pointer is set by GLAD if GL 4.3+ is available
	available_ = (glMultiDrawElementsIndirect != nullptr);

	if (!available_) {
		// Warning: MDI disabled
		return false;
	}

	// Create buffer for indirect commands
	command_buffer_ = std::make_unique<GLBuffer>();

	// Pre-allocate buffer storage
	glNamedBufferStorage(command_buffer_->GetID(), max_commands_ * sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_STORAGE_BIT);

	initialized_ = true;
	return true;
}

void MultiDrawIndirectRenderer::cleanup() {
	command_buffer_.reset();
	commands_.clear();
	initialized_ = false;
}

void MultiDrawIndirectRenderer::clear() {
	commands_.clear();
}

void MultiDrawIndirectRenderer::addDrawCommand(GLuint count, GLuint instanceCount, GLuint firstIndex, GLuint baseVertex, GLuint baseInstance) {
	if (static_cast<int>(commands_.size()) >= max_commands_) {
		// Max commands reached, ignoring
		return;
	}

	if (instanceCount == 0) {
		return; // Skip empty draws
	}

	DrawElementsIndirectCommand cmd;
	cmd.count = count;
	cmd.instanceCount = instanceCount;
	cmd.firstIndex = firstIndex;
	cmd.baseVertex = baseVertex;
	cmd.baseInstance = baseInstance;

	commands_.push_back(cmd);
}

void MultiDrawIndirectRenderer::upload() {
	if (commands_.empty() || !initialized_) {
		return;
	}

	glNamedBufferSubData(command_buffer_->GetID(), 0, commands_.size() * sizeof(DrawElementsIndirectCommand), commands_.data());
}

void MultiDrawIndirectRenderer::execute() {
	if (commands_.empty() || !available_ || !initialized_) {
		return;
	}

	// Bind buffer for draw call
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command_buffer_->GetID());
	glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
								nullptr, // Offset 0 in bound buffer
								static_cast<GLsizei>(commands_.size()), sizeof(DrawElementsIndirectCommand));
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}
