#include "renderer/Framebuffer.h"

#include <cassert>

namespace engine
{
	static float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };

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
		case AttachmentFormat::Depth32F:
		case AttachmentFormat::Depth32FShadow: 
		case AttachmentFormat::Depth32FShadowArray: return GL_DEPTH_COMPONENT32F;
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
		case AttachmentFormat::Depth24: 
		case AttachmentFormat::Depth32F:
		case AttachmentFormat::Depth32FShadow: 
		case AttachmentFormat::Depth32FShadowArray: return GL_DEPTH_COMPONENT;
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
		case AttachmentFormat::RGB16F:
		case AttachmentFormat::Depth32F:
		case AttachmentFormat::Depth32FShadow: 
		case AttachmentFormat::Depth32FShadowArray: return GL_FLOAT;
		case AttachmentFormat::Depth24: return GL_UNSIGNED_INT;
		case AttachmentFormat::Depth24Stencil8: return GL_UNSIGNED_INT_24_8;
		}

		assert(false && "Unknown attachment format");
		return 0;
	}

	static bool isDepthFormat(AttachmentFormat format)
	{
		return format == AttachmentFormat::Depth24 || 
			format == AttachmentFormat::Depth24Stencil8 ||
			format == AttachmentFormat::Depth32F ||
			format == AttachmentFormat::Depth32FShadow ||
			format == AttachmentFormat::Depth32FShadowArray;
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

	void Framebuffer::attachLayer(int index) const
	{
		for (const auto& attachment : _attachments)
		{
			if (attachment.format == AttachmentFormat::Depth32FShadowArray)
			{
				glFramebufferTextureLayer(
					GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					attachment.textureId, 0, index
				);
				return;
			}
		}

		assert(false && "No layered depth attachment found");
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
			GLenum format = toInternalFormat(attachment.format);
			GLenum dataFormat = toDataFormat(attachment.format);
			GLenum dataType = toDataType(attachment.format);

			glGenTextures(1, &attachment.textureId);

			if (attachment.format == AttachmentFormat::Depth32FShadowArray)
			{
				glBindTexture(GL_TEXTURE_2D_ARRAY, attachment.textureId);
				glTexImage3D(
					GL_TEXTURE_2D_ARRAY, 0, format,
					_width, _height, attachment.layerCount, 0,
					dataFormat, dataType, nullptr
				);

				// Set filter to linear for PCF shadows
				GLint filter = (attachment.format == AttachmentFormat::Depth32FShadowArray)
					? GL_LINEAR : GL_NEAREST;

				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, attachment.wrapMode);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, attachment.wrapMode);

				// Set border color to white for shadow map sampling
				if (attachment.borderWhite)
				{
					glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
				}

				// Set shadow compare mode for sampler2DShadow
				if (attachment.format == AttachmentFormat::Depth32FShadowArray)
				{
					glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE,
						GL_COMPARE_REF_TO_TEXTURE);
					glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
				}

				// Attach the entire texture array allocation to the framebuffer
				GLenum attachPoint = toAttachmentPoint(attachment.format, colorIndex);
				glFramebufferTexture(GL_FRAMEBUFFER, attachPoint, attachment.textureId, 0);

				glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, attachment.textureId);
				glTexImage2D(
					GL_TEXTURE_2D, 0, format,
					_width, _height, 0, dataFormat,
					dataType, nullptr
				);

				// Set filter to linear for PCF shadows
				GLint filter = (attachment.format == AttachmentFormat::Depth32FShadow)
					? GL_LINEAR : GL_NEAREST;

				// Set filter to linear for smooth color textures
				if (attachment.format == AttachmentFormat::RGBA8 ||
					attachment.format == AttachmentFormat::RGBA16F ||
					attachment.format == AttachmentFormat::RGB16F)
				{
					filter = GL_LINEAR;
				}

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, attachment.wrapMode);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, attachment.wrapMode);

				// Set border color to white for shadow map sampling
				if (attachment.borderWhite)
				{
					glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
				}

				// Set shadow compare mode for sampler2DShadow
				if (attachment.format == AttachmentFormat::Depth32FShadow)
				{
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
						GL_COMPARE_REF_TO_TEXTURE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
				}

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
		}

		if (!colorAttachmentPoints.empty())
		{
			// Set up color attachments
			glDrawBuffers(colorAttachmentPoints.size(), colorAttachmentPoints.data());
		}
		else
		{
			// Explicitly set no color attachments for shadow FBOs
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
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