#include "renderer/resources/Cubemap.h"

#include <stdexcept>
#include <stb_image.h>

namespace engine
{
	Cubemap::Cubemap(const std::array<std::string, 6>& facePaths)
	{
		glGenTextures(1, &_tid);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _tid);

		// Cubemaps usually should not be flipped like normal 2D textures
		stbi_set_flip_vertically_on_load(false);

		for (GLuint i = 0; i < 6; ++i)
		{
			int width = 0, height = 0, channels = 0;
			unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);
			if (!data)
			{
				glDeleteTextures(1, &_tid);
				throw std::runtime_error("Failed to load cubemap face: " + facePaths[i]);
			}

			GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0,
				format,
				width,
				height,
				0,
				format,
				GL_UNSIGNED_BYTE,
				data
			);

			stbi_image_free(data);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	Cubemap::~Cubemap()
	{
		if (_tid != 0)
		{
			glDeleteTextures(1, &_tid);
		}
	}

	void Cubemap::bind(GLint uniformHandle) const
	{
		glActiveTexture(GL_TEXTURE0 + _unit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _tid);
		glUniform1i(uniformHandle, _unit);
	}

	void Cubemap::unbind() const
	{
		glActiveTexture(GL_TEXTURE0 + _unit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
}