#pragma once

#include <array>
#include <string>
#include <glad/glad.h>

namespace engine
{
	class Cubemap
	{
	public:
		Cubemap(const std::array<std::string, 6>& facePaths);
		~Cubemap();

		void setUnit(GLint unit) { _unit = unit; }
		GLint getUnit() const { return _unit; }

		void bind(GLint uniformHandle) const;
		void unbind() const;

	private:
		GLuint _tid = 0;
		GLint _unit = 0;
	};
}