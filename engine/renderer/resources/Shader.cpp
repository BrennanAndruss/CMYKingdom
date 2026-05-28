#include "renderer/resources/Shader.h"

#include <iostream>
#include <stdexcept>
#include <cassert>
#include "renderer/UniformBuffer.h"

namespace engine
{
	static GLuint compileShader(GLenum type, const std::string& src)
	{
		GLuint shader = glCreateShader(type);
		const char* srcPtr = src.data();
		glShaderSource(shader, 1, &srcPtr, nullptr);
		glCompileShader(shader);

		GLint ok;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, nullptr, infoLog);
			glDeleteShader(shader);
			throw std::runtime_error(std::string("Shader compilation failed: ") + infoLog);
		}

		return shader;
	}

	static GLuint linkProgram(GLuint vertShader, GLuint fragShader)
	{
		GLuint program = glCreateProgram();
		glAttachShader(program, vertShader);
		glAttachShader(program, fragShader);
		glLinkProgram(program);

		GLint ok;
		glGetProgramiv(program, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			char infoLog[512];
			glGetProgramInfoLog(program, 512, nullptr, infoLog);
			glDeleteProgram(program);
			glDeleteShader(vertShader);
			glDeleteShader(fragShader);
			throw std::runtime_error(std::string("Program linking failed: ") + infoLog);
		}

		glDeleteShader(vertShader);
		glDeleteShader(fragShader);
		return program;
	}

	Shader::Shader(const std::string& vertSrc, const std::string& fragSrc)
	{
		GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
		GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
		_pid = linkProgram(vertShader, fragShader);

		// Bind uniform blocks
		GLuint cameraIndex = glGetUniformBlockIndex(_pid, "CameraData");
		GLuint lightIndex = glGetUniformBlockIndex(_pid, "LightData");

		if (cameraIndex != GL_INVALID_INDEX)
		{
			glUniformBlockBinding(_pid, cameraIndex, static_cast<GLuint>(UBOBindings::Camera));
		}
		if (lightIndex != GL_INVALID_INDEX)
		{
			glUniformBlockBinding(_pid, lightIndex, static_cast<GLuint>(UBOBindings::Light));
		}
	}

	void Shader::bind() const
	{
		glUseProgram(_pid);
	}

	void Shader::unbind() const
	{
		glUseProgram(0);
	}

	GLint Shader::getUniform(const std::string& name) const
	{
		// Get previously cached uniform locations
		auto uniformIt = _uniforms.find(name);
		if (uniformIt != _uniforms.end())
		{
			return uniformIt->second;
		}

		// Get and cache new uniform location
		GLint loc = glGetUniformLocation(_pid, name.data());
		if (loc == -1)
		{
			// Uniform either doesn't exist or has been optimized away
			std::cerr << "Warning: Uniform " << name << " cannot be bound.\n";
		}
		_uniforms.emplace(name, loc);
		return loc;
	}

	void Shader::setInt(const std::string& name, int value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniform1i(loc, value);
	}

	void Shader::setFloat(const std::string& name, float value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniform1f(loc, value);
	}

	void Shader::setVec2(const std::string& name, const glm::vec2& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniform2fv(loc, 1, &value[0]);
	}

	void Shader::setVec3(const std::string& name, const glm::vec3& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniform3fv(loc, 1, &value[0]);
	}

	void Shader::setVec4(const std::string& name, const glm::vec4& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniform4fv(loc, 1, &value[0]);
	}

	void Shader::setMat2(const std::string& name, const glm::mat2& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniformMatrix2fv(loc, 1, GL_FALSE, &value[0][0]);
	}

	void Shader::setMat3(const std::string& name, const glm::mat3& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniformMatrix3fv(loc, 1, GL_FALSE, &value[0][0]);
	}

	void Shader::setMat4(const std::string& name, const glm::mat4& value) const
	{
		GLint loc = getUniform(name);
		if (loc == -1) return;
		glUniformMatrix4fv(loc, 1, GL_FALSE, &value[0][0]);
	}
}