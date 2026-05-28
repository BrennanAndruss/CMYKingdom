#pragma once

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
		DeferredGeometryPass(int width, int height);
		~DeferredGeometryPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Framebuffer _gBuffer;

		void drawObject(Object* object, const Scene& scene,
			const AssetManager& assets);
		void drawObjectCulled(Object* object, const Scene& scene,
			const AssetManager& assets, const Frustum& frustum);
	};
}