#ifndef RME_RENDERING_CORE_SHADER_PROGRAM_H_
#define RME_RENDERING_CORE_SHADER_PROGRAM_H_

#include <glad/glad.h>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>

class GLProgram;

class ShaderProgram {
public:
	ShaderProgram();
	~ShaderProgram();

	bool Load(const std::string& vertexSource, const std::string& fragmentSource);
	void Use() const;
	void Unuse() const;

	// Uniform setters
	void SetBool(std::string_view name, bool value) const;
	void SetInt(std::string_view name, int value) const;
	void SetUint(std::string_view name, unsigned int value) const;
	void SetFloat(std::string_view name, float value) const;
	void SetVec2(std::string_view name, const glm::vec2& value) const;
	void SetVec3(std::string_view name, const glm::vec3& value) const;
	void SetVec4(std::string_view name, const glm::vec4& value) const;
	void SetMat4(std::string_view name, const glm::mat4& value) const;

	bool IsValid() const;

	GLuint GetID() const;

private:
	struct UniformNameHash {
		using is_transparent = void;
		size_t operator()(std::string_view name) const noexcept {
			return std::hash<std::string_view> {}(name);
		}
	};

	std::unique_ptr<GLProgram> program_;
	mutable std::unordered_map<std::string, GLint, UniformNameHash, std::equal_to<>> uniform_cache;

	GLint GetUniformLocation(std::string_view name) const;
	bool CompileShader(GLuint shader, const std::string& source);
};

#endif
