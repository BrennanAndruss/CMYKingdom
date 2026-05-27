#include "renderer/Framebuffer.h"

#include <cassert>

namespace engine
{
	// Format helpers
	static GLenum toInternalFormat(AttachmentFormat format)
	{
		switch (format)
		{
		case AttachmentFormat::RGBA8: return GL_RGBA8;
		case AttachmentFormat::RGBA16F: return GL_RGBA16F;
		case AttachmentFormat::RGB16F: return GL_RGB16F;
		case AttachmentFormat::Depth24: return GL_DEPTH_COMPONENT24;
		case AttachmentFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
		}
		assert(false && "Unknown attachment format");
		return 0;
	}

	static GLenum toDataFormat(AttachmentFormat format)
	{
		switch (format)
		{
		case AttachmentFormat::RGBA8:
		case AttachmentFormat::RGBA16F: return GL_RGBA;
		case AttachmentFormat::RGB16F: return GL_RGB;
		case AttachmentFormat::Depth24: return GL_DEPTH_COMPONENT;
		case AttachmentFormat::Depth24Stencil8: return GL_DEPTH_STENCIL;
		}
		assert(false && "Unknown attachment format");
		return 0;
	}

	static GLenum toDataType(AttachmentFormat format)
	{
		switch (format)
		{
		case AttachmentFormat::RGBA8: return GL_UNSIGNED_BYTE;
		case AttachmentFormat::RGBA16F:
		case AttachmentFormat::RGB16F: return GL_FLOAT;
		case AttachmentFormat::Depth24: return GL_UNSIGNED_INT;
		case AttachmentFormat::Depth24Stencil8: return GL_UNSIGNED_INT_24_8;
		}
		assert(false && "Unknown attachment format");
		return 0;
	}

	static bool isDepthFormat(AttachmentFormat format)
	{
		return format == AttachmentFormat::Depth24 || 
			format == AttachmentFormat::Depth24Stencil8;
	}

	static GLenum toAttachmentPoint(AttachmentFormat format, int colorIndex)
	{
		if (isDepthFormat(format))
		{
			if (format == AttachmentFormat::Depth24Stencil8)
			{
				return GL_DEPTH_STENCIL_ATTACHMENT;
			}
			return GL_DEPTH_ATTACHMENT;
		}
		return GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(colorIndex);
	}

	Framebuffer::Framebuffer(int width, int height, 
		std::vector<FramebufferAttachment> attachments) :
		_width(width), _height(height), _attachments(attachments) 
	{
		create();
	}

	Framebuffer::~Framebuffer()
	{
		destroy();
	}

	void Framebuffer::bind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _fboId);
		glViewport(0, 0, _width, _height);
	}

	void Framebuffer::unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::resize(int width, int height)
	{
		if (_width == width && _height == height) return;

		// Rebuild framebuffer object
		_width = width;
		_height = height;
		create();
	}

	GLuint Framebuffer::getColorAttachment(int index) const
	{
		int colorIdx = 0;
		for (const auto& attachment : _attachments)
		{
			if (!isDepthFormat(attachment.format))
			{
				if (colorIdx == index)
					return attachment.textureId;
				colorIdx++;
			}
		}

		assert(false && "Color attachment index out of range");
		return 0;
	}

	GLuint Framebuffer::getDepthAttachment() const
	{
		for (const auto& attachment : _attachments)
		{
			if (isDepthFormat(attachment.format))
			{
				return attachment.textureId;
			}
		}

		assert(false && "Depth attachment not found");
		return 0;
	}

	void Framebuffer::create()
	{
		destroy();

		glGenFramebuffers(1, &_fboId);
		glBindFramebuffer(GL_FRAMEBUFFER, _fboId);

		std::vector<GLenum> colorAttachmentPoints;
		int colorIndex = 0;

		// Create attachments
		for (auto& attachment : _attachments)
		{
			GLuint texId;
			glGenTextures(1, &attachment.textureId);
			glBindTexture(GL_TEXTURE_2D, attachment.textureId);
			glTexImage2D(
				GL_TEXTURE_2D, 0, toInternalFormat(attachment.format),
				_width, _height, 0, toDataFormat(attachment.format),
				toDataType(attachment.format), nullptr
			);

			// Texture parameter defaults
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			GLenum attachPoint = toAttachmentPoint(attachment.format, colorIndex);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER, attachPoint, GL_TEXTURE_2D, attachment.textureId, 0
			);

			if (!isDepthFormat(attachment.format))
			{
				colorAttachmentPoints.push_back(attachPoint);
				colorIndex++;
			}

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// Tell GL which color attachments to draw to
		if (!colorAttachmentPoints.empty())
		{
			glDrawBuffers(colorAttachmentPoints.size(), colorAttachmentPoints.data());
		}

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
			"Framebuffer is incomplete");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::destroy()
	{
		for (auto& attachment : _attachments)
		{
			if (attachment.textureId != 0)
			{
				glDeleteTextures(1, &attachment.textureId);
			}
		}

		if (_fboId != 0)
		{
			glDeleteFramebuffers(1, &_fboId);
			_fboId = 0;
		}
	}
}