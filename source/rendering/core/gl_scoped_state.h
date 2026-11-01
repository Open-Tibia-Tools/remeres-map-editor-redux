#ifndef RME_RENDERING_CORE_GL_SCOPED_STATE_H_
#define RME_RENDERING_CORE_GL_SCOPED_STATE_H_

#include <glad/glad.h>
#include <cassert>
#include <utility>

/**
 * @brief RAII wrapper for glEnable/glDisable
 *
 * Intended for short-lived scope-based state changes, not for persistent state modifications.
 */
/**
 * @brief RAII wrapper for glDepthMask
 */
class ScopedGLDepthMask {
public:
	[[nodiscard]] explicit ScopedGLDepthMask(GLboolean write_enable) {
		glGetBooleanv(GL_DEPTH_WRITEMASK, &was_write_enabled_);
		glDepthMask(write_enable);
	}

	~ScopedGLDepthMask() {
		restore();
	}

	ScopedGLDepthMask(const ScopedGLDepthMask&) = delete;
	ScopedGLDepthMask& operator=(const ScopedGLDepthMask&) = delete;

	ScopedGLDepthMask(ScopedGLDepthMask&& other) noexcept :
		was_write_enabled_(other.was_write_enabled_),
		active_(std::exchange(other.active_, false)) {
	}

	ScopedGLDepthMask& operator=(ScopedGLDepthMask&& other) noexcept {
		if (this != &other) {
			restore();
			was_write_enabled_ = other.was_write_enabled_;
			active_ = std::exchange(other.active_, false);
		}
		return *this;
	}

private:
	void restore() const {
		if (active_) {
			glDepthMask(was_write_enabled_);
		}
	}

private:
	GLboolean was_write_enabled_ = GL_TRUE;
	bool active_ = true;
};

class ScopedGLCapability {
public:
	[[nodiscard]] explicit ScopedGLCapability(GLenum capability, bool enable = true) :
		capability_(capability) {
		was_enabled_ = glIsEnabled(capability);
		if (enable) {
			if (!was_enabled_) {
				glEnable(capability);
			}
		} else {
			if (was_enabled_) {
				glDisable(capability);
			}
		}
	}

	~ScopedGLCapability() {
		restore();
	}

	ScopedGLCapability(const ScopedGLCapability&) = delete;
	ScopedGLCapability& operator=(const ScopedGLCapability&) = delete;

	ScopedGLCapability(ScopedGLCapability&& other) noexcept :
		capability_(other.capability_),
		was_enabled_(other.was_enabled_),
		active_(std::exchange(other.active_, false)) {
	}

	ScopedGLCapability& operator=(ScopedGLCapability&& other) noexcept {
		if (this != &other) {
			restore();
			capability_ = other.capability_;
			was_enabled_ = other.was_enabled_;
			active_ = std::exchange(other.active_, false);
		}
		return *this;
	}

private:
	void restore() const {
		if (active_) {
			if (was_enabled_) {
				glEnable(capability_);
			} else {
				glDisable(capability_);
			}
		}
	}

private:
	GLenum capability_;
	GLboolean was_enabled_;
	bool active_ = true;
};

/**
 * @brief RAII wrapper for glBlendFunc and glBlendEquation
 *
 * Intended for short-lived scope-based state changes, not for persistent state modifications.
 */
class ScopedGLBlend {
public:
	[[nodiscard]] ScopedGLBlend(GLenum sfactor, GLenum dfactor) {
		save_state();
		glBlendFunc(sfactor, dfactor);
	}

	[[nodiscard]] ScopedGLBlend(GLenum sfactor, GLenum dfactor, GLenum equation) {
		save_state();
		glBlendFunc(sfactor, dfactor);
		glBlendEquation(equation);
	}

	~ScopedGLBlend() {
		restore();
	}

	ScopedGLBlend(const ScopedGLBlend&) = delete;
	ScopedGLBlend& operator=(const ScopedGLBlend&) = delete;

	ScopedGLBlend(ScopedGLBlend&& other) noexcept :
		prev_src_rgb_(other.prev_src_rgb_),
		prev_dst_rgb_(other.prev_dst_rgb_),
		prev_src_alpha_(other.prev_src_alpha_),
		prev_dst_alpha_(other.prev_dst_alpha_),
		prev_eq_rgb_(other.prev_eq_rgb_),
		prev_eq_alpha_(other.prev_eq_alpha_),
		active_(std::exchange(other.active_, false)) {
	}

	ScopedGLBlend& operator=(ScopedGLBlend&& other) noexcept {
		if (this != &other) {
			restore();
			prev_src_rgb_ = other.prev_src_rgb_;
			prev_dst_rgb_ = other.prev_dst_rgb_;
			prev_src_alpha_ = other.prev_src_alpha_;
			prev_dst_alpha_ = other.prev_dst_alpha_;
			prev_eq_rgb_ = other.prev_eq_rgb_;
			prev_eq_alpha_ = other.prev_eq_alpha_;
			active_ = std::exchange(other.active_, false);
		}
		return *this;
	}

private:
	void restore() const {
		if (active_) {
			glBlendFuncSeparate(prev_src_rgb_, prev_dst_rgb_, prev_src_alpha_, prev_dst_alpha_);
			glBlendEquationSeparate(prev_eq_rgb_, prev_eq_alpha_);
		}
	}

private:
	void save_state() {
		glGetIntegerv(GL_BLEND_SRC_RGB, &prev_src_rgb_);
		glGetIntegerv(GL_BLEND_DST_RGB, &prev_dst_rgb_);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_src_alpha_);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_dst_alpha_);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &prev_eq_rgb_);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prev_eq_alpha_);
	}

	GLint prev_src_rgb_ = GL_ONE, prev_dst_rgb_ = GL_ZERO;
	GLint prev_src_alpha_ = GL_ONE, prev_dst_alpha_ = GL_ZERO;
	GLint prev_eq_rgb_ = GL_FUNC_ADD, prev_eq_alpha_ = GL_FUNC_ADD;
	bool active_ = true;
};

/**
 * @brief RAII wrapper for glFramebuffer
 *
 * Saves current READ and DRAW framebuffer bindings on construction and restores them on destruction.
 */
class ScopedGLFramebuffer {
public:
	[[nodiscard]] explicit ScopedGLFramebuffer(GLenum target, GLuint framebuffer) :
		target_(target) {
		assert(target_ == GL_FRAMEBUFFER || target_ == GL_READ_FRAMEBUFFER || target_ == GL_DRAW_FRAMEBUFFER);

		if (target_ == GL_FRAMEBUFFER || target_ == GL_READ_FRAMEBUFFER) {
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_);
		}
		if (target_ == GL_FRAMEBUFFER || target_ == GL_DRAW_FRAMEBUFFER) {
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_);
		}

		glBindFramebuffer(target_, framebuffer);
	}

	~ScopedGLFramebuffer() {
		restore();
	}

	ScopedGLFramebuffer(const ScopedGLFramebuffer&) = delete;
	ScopedGLFramebuffer& operator=(const ScopedGLFramebuffer&) = delete;

	ScopedGLFramebuffer(ScopedGLFramebuffer&& other) noexcept :
		target_(other.target_),
		prev_read_(other.prev_read_),
		prev_draw_(other.prev_draw_),
		active_(std::exchange(other.active_, false)) {
	}

	ScopedGLFramebuffer& operator=(ScopedGLFramebuffer&& other) noexcept {
		if (this != &other) {
			restore();
			target_ = other.target_;
			prev_read_ = other.prev_read_;
			prev_draw_ = other.prev_draw_;
			active_ = std::exchange(other.active_, false);
		}
		return *this;
	}

private:
	void restore() const {
		if (active_) {
			if (target_ == GL_FRAMEBUFFER || target_ == GL_READ_FRAMEBUFFER) {
				glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_);
			}
			if (target_ == GL_FRAMEBUFFER || target_ == GL_DRAW_FRAMEBUFFER) {
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_);
			}
		}
	}

private:
	GLenum target_;
	GLint prev_read_ = 0;
	GLint prev_draw_ = 0;
	bool active_ = true;
};

/**
 * @brief RAII wrapper for glViewport
 *
 * Saves current viewport on construction and restores it on destruction.
 */
class ScopedGLViewport {
public:
	[[nodiscard]] ScopedGLViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
		glGetIntegerv(GL_VIEWPORT, prev_viewport_);
		glViewport(x, y, width, height);
	}

	~ScopedGLViewport() {
		restore();
	}

	ScopedGLViewport(const ScopedGLViewport&) = delete;
	ScopedGLViewport& operator=(const ScopedGLViewport&) = delete;

	ScopedGLViewport(ScopedGLViewport&& other) noexcept :
		active_(std::exchange(other.active_, false)) {
		for (int i = 0; i < 4; ++i) {
			prev_viewport_[i] = other.prev_viewport_[i];
		}
	}

	ScopedGLViewport& operator=(ScopedGLViewport&& other) noexcept {
		if (this != &other) {
			restore();
			for (int i = 0; i < 4; ++i) {
				prev_viewport_[i] = other.prev_viewport_[i];
			}
			active_ = std::exchange(other.active_, false);
		}
		return *this;
	}

private:
	void restore() const {
		if (active_) {
			glViewport(prev_viewport_[0], prev_viewport_[1], prev_viewport_[2], prev_viewport_[3]);
		}
	}

private:
	GLint prev_viewport_[4];
	bool active_ = true;
};

#endif
