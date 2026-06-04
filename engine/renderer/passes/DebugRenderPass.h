#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "renderer/passes/RenderPass.h"

namespace engine
{
	class Shader;
}

namespace engine
{
	class DebugRenderPass : public RenderPass
	{
	public:
		DebugRenderPass();
		~DebugRenderPass();

		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

		bool showSkeletons = false;
		bool showColliders = false;

		// Global/static controls so the editor UI can toggle debug drawing
		static bool s_showSkeletons;
		static bool s_showColliders;

		static void setShowSkeletons(bool v) { s_showSkeletons = v; }
		static bool getShowSkeletons() { return s_showSkeletons; }

		static void setShowColliders(bool v) { s_showColliders = v; }
		static bool getShowColliders() { return s_showColliders; }

	private:
		std::unique_ptr<Shader> _lineShader;
		GLuint _vao = 0, _vbo = 0;

		// Track allocated size to avoid realloc every frame
		size_t _vboCapacity = 0;

		void drawSkeletons(const Scene& scene, const AssetManager& assets);
		void drawColliders(const Scene& scene);
		void uploadLineVerts(const std::vector<glm::vec3>& verts);
	};
}