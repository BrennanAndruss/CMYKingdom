#pragma once

#include <glad/glad.h>
#include <vector>

namespace engine
{
	class Texture
	{
	public:
		Texture(int width, int height, GLint format, const unsigned char* textureData);
		~Texture();

		void setUnit(GLint unit) { _unit = unit; }
		GLint getUnit() const { return _unit; }
		GLuint getID() const { return _tid; }

		int getWidth() const { return _width; }
		int getHeight() const { return _height; }
		int getChannels() const { return _channels; }
		const std::vector<unsigned char>& getPixels() const { return _pixels; }

		void bindToUnit(GLint handle, GLint unit) const;
		void unbindFromUnit(GLint unit) const;
		
		void bind(GLint handle) const;
		void unbind() const;
		

	private:
		int _width = 0;
		int _height = 0;
		int _channels = 4;

		GLint _internalFormat;
		GLint _imageFormat;

		GLuint _tid;
		GLuint _unit;

		std::vector<unsigned char> _pixels;
	};
}