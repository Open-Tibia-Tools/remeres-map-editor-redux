#include "rendering/core/shader_program.h"
#include "rendering/core/gl_resources.h"
#include <vector>
#include <spdlog/spdlog.h>

ShaderProgram::ShaderProgram() {
}

ShaderProgram::~ShaderProgram() {
}

bool ShaderProgram::Load(const std::string& vertexSource, const std::string& fragmentSource) {
	GLShader vertexShader(GL_VERTEX_SHADER);
	if (!CompileShader(vertexShader.GetID(), vertexSource)) {
		return false;
	}

	GLShader fragmentShader(GL_FRAGMENT_SHADER);
	if (!CompileShader(fragmentShader.GetID(), fragmentSource)) {
		return false;
	}

	program_ = std::make_unique<GLProgram>();
	GLuint program_id = program_->GetID();

	glAttachShader(program_id, vertexShader.GetID());
	glAttachShader(program_id, fragmentShader.GetID());
	glLinkProgram(program_id);

	GLint success;
	glGetProgramiv(program_id, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetProgramInfoLog(program_id, 1024, nullptr, infoLog);
		spdlog::error("SHADER PROGRAM LINKING ERROR: {}", infoLog);
		program_.reset(); // Cleanup on failure
		return false;
	}

	return true;
}

void ShaderProgram::Use() const {
	if (program_) {
		glUseProgram(program_->GetID());
	}
}

void ShaderProgram::Unuse() const {
	glUseProgram(0);
}

bool ShaderProgram::IsValid() const {
	return program_ && program_->GetID() != 0;
}

GLuint ShaderProgram::GetID() const {
	return program_ ? program_->GetID() : 0;
}

GLint ShaderProgram::GetUniformLocation(const std::string& name) const {
	if (!program_) {
		return -1;
	}

	if (auto it = uniform_cache.find(name); it != uniform_cache.end()) {
		return it->second;
	}

	GLint location = glGetUniformLocation(program_->GetID(), name.c_str());
	if (location == -1) {
	}

	uniform_cache[name] = location;
	return location;
}

void ShaderProgram::SetBool(const std::string& name, bool value) const {
	// OpenGL doesn't have a native bool uniform type, so we cast to int (0 or 1)
	glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void ShaderProgram::SetInt(const std::string& name, int value) const {
	glUniform1i(GetUniformLocation(name), value);
}

void ShaderProgram::SetUint(const std::string& name, unsigned int value) const {
	glUniform1ui(GetUniformLocation(name), value);
}

void ShaderProgram::SetFloat(const std::string& name, float value) const {
	glUniform1f(GetUniformLocation(name), value);
}

void ShaderProgram::SetVec2(const std::string& name, const glm::vec2& value) const {
	glUniform2fv(GetUniformLocation(name), 1, &value[0]);
}

void ShaderProgram::SetVec3(const std::string& name, const glm::vec3& value) const {
	glUniform3fv(GetUniformLocation(name), 1, &value[0]);
}

void ShaderProgram::SetVec4(const std::string& name, const glm::vec4& value) const {
	glUniform4fv(GetUniformLocation(name), 1, &value[0]);
}

void ShaderProgram::SetMat4(const std::string& name, const glm::mat4& value) const {
	glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &value[0][0]);
}

bool ShaderProgram::CompileShader(GLuint shader, const std::string& source) {
	const char* src = source.c_str();
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetShaderInfoLog(shader, 1024, nullptr, infoLog);

		GLint type;
		glGetShaderiv(shader, GL_SHADER_TYPE, &type);
		spdlog::error("SHADER COMPILATION ERROR ({}):\n{}", (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"), infoLog);
		return false;
	}
	return true;
}
