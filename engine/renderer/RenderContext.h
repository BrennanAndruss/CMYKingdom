#pragma once

#include <glad/glad.h>
#include <unordered_map>
#include <string>
#include <cassert>
#include "renderer/Framebuffer.h"

namespace engine
{
	class Framebuffer;
}

namespace engine
{
	// Named texture slots
	namespace BufferNames
	{
		// Scene outputs
		inline constexpr const char* SceneColor = "SceneColor";
		inline constexpr const char* SceneDepth = "SceneDepth";

		// G-buffer
		inline constexpr const char* GPosition = "GPosition";
		inline constexpr const char* GNormal = "GNormal";
		inline constexpr const char* GAlbedo = "GAlbedo";
		inline constexpr const char* GSpecular = "GSpecular";
	}

	struct RenderContext
	{
		int width = 0, height = 0;
		
		Framebuffer* sceneFramebuffer;
		std::unordered_map<std::string, GLuint> buffers;

		void setBuffer(const std::string& name, GLuint textureId)
		{
			buffers[name] = textureId;
		}

		GLuint getBuffer(const std::string& name) const
		{
			auto it = buffers.find(name);
			assert(it != buffers.end() && "Buffer not found");

			return it->second;
		}
	};
}