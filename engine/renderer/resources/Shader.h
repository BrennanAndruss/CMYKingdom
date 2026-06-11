#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <string>
#include <unordered_map>

namespace engine
{
	class Shader
	{
	public:
		Shader(const std::string& vertSrc, const std::string& fragSrc);

		GLuint getPid() const { return _pid; }
		GLint getUniform(const std::string& name) const;

		void bind() const;
		void unbind() const;

		void setInt(const std::string& name, int value) const;
		void setFloat(const std::string& name, float value) const;
		void setVec2(const std::string& name, const glm::vec2& value) const;
		void setVec3(const std::string& name, const glm::vec3& value) const;
		void setVec4(const std::string& name, const glm::vec4& value) const;
		void setMat2(const std::string& name, const glm::mat2& value) const;
		void setMat3(const std::string& name, const glm::mat3& value) const;
		void setMat4(const std::string& name, const glm::mat4& value) const;
		void setMat4Array(const std::string& name, const glm::mat4* values, 
			GLsizei count) const;

	private:
		GLuint _pid = 0;

		// Mutable uniform cache to enable caching in const methods
		mutable std::unordered_map<std::string, GLint> _uniforms;
	};
}