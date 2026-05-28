#include "renderer/resources/Texture.h"

#include <stdexcept>

namespace engine
{
	Texture::Texture(int width, int height, GLint format, const unsigned char* textureData)
		: _width(width), _height(height), _internalFormat(format), _imageFormat(format), _tid(0), _unit(0)
	{
		_channels = (format == GL_RGBA) ? 4 : 3;

		if (textureData)
		{
			const std::size_t pixelCount = static_cast<std::size_t>(_width * _height * _channels);
			_pixels.assign(textureData, textureData + pixelCount);
		}

		glGenTextures(1, &_tid);
		glBindTexture(GL_TEXTURE_2D, _tid);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexImage2D(GL_TEXTURE_2D, 0, _internalFormat, _width, _height, 0, _imageFormat, GL_UNSIGNED_BYTE, textureData);

		GLint w = 0, h = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		if (w == 0 || h == 0)
		{
			throw std::runtime_error("Texture initialization failed");
		}

		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture::~Texture()
	{
		if (_tid != 0)
		{
			glDeleteTextures(1, &_tid);
		}
	}

	void Texture::bind(GLint handle) const
	{
		glActiveTexture(GL_TEXTURE0 + _unit);
		glBindTexture(GL_TEXTURE_2D, _tid);
		glUniform1i(handle, _unit);
	}

	void Texture::bindToUnit(GLint handle, GLint unit) const
	{
		if (handle < 0)
		{
			return;
		}

		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, _tid);
		glUniform1i(handle, unit);
	}

	void Texture::unbind() const
	{
		glActiveTexture(GL_TEXTURE0 + _unit);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	
	void Texture::unbindFromUnit(GLint unit) const
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	
}