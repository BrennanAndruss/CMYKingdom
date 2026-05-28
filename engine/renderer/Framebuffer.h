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

		// Depth formats
		Depth24,
		Depth24Stencil8
	};

	struct FramebufferAttachment
	{
		AttachmentFormat format;
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