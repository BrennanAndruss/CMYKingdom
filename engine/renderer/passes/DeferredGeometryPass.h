#pragma once

#include <vector>
#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"
#include "resources/Handle.h"

namespace engine
{
	class Scene;
	class Object;
	class AssetManager;
	class RenderContext;
	class Shader;
	struct Frustum;
}

namespace engine
{
	class DeferredGeometryPass : public RenderPass
	{
	public:
		DeferredGeometryPass(int width, int height, Handle<Shader> baseShader,
			Handle<Shader> terrainShader, Handle<Shader> skinnedShader);
		~DeferredGeometryPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Handle<Shader> _baseShader, _terrainShader, _skinnedShader;
		Framebuffer _gBuffer;

		std::vector<Object*> baseQueue;
		std::vector<Object*> stencilQueue;
		std::vector<Object*> terrainQueue;
		std::vector<Object*> skinnedQueue;

		void collectVisibleObjects(Object* object, const Scene& scene,
			const AssetManager& assets, const Frustum& frustum);
	};
}