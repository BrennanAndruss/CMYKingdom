#pragma once

#include <glad/glad.h>
#include <vector>

namespace engine
{
	class Texture;
}

namespace engine
{
	enum class AttachmentFormat
	{
		// Color formats
		RGBA8,
		RGBA16F,
		RGB16F,
		RGBA32F,
		RGB32F,

		// Depth formats
		Depth24,
		Depth24Stencil8,
		Depth32F,

		// Shadow formats
		// Enables shadow compare mode
		Depth32FShadow,
		Depth32FShadowArray
	};

	struct FramebufferAttachment
	{
		AttachmentFormat format;
		GLint filterMode = GL_NEAREST;
		GLint wrapMode = GL_CLAMP_TO_EDGE;
		bool borderWhite = false;
		int layerCount = 1;
		GLuint textureId = 0;
	};

	class Framebuffer
	{
	public:
		Framebuffer(int width, int height, std::vector<FramebufferAttachment> attachments);
		~Framebuffer();

		void resize(int width, int height);

		void bind() const;
		void unbind() const;

		void attachLayer(int index) const;

		GLuint getColorAttachment(int index) const;
		GLuint getDepthAttachment() const;

		int getWidth() const { return _width; }
		int getHeight() const { return _height; }
		GLuint getFboId() const { return _fboId; }

	private:
		GLuint _fboId;
		int _width, _height;
		std::vector<FramebufferAttachment> _attachments;

		void create();
		void destroy();
	};
}