#pragma once

#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"
#include "resources/Handle.h"

namespace engine
{
	class Scene;
	class AssetManager;
	class RenderContext;
	class Shader;
}

namespace engine
{
	class DeferredLightingPass : public RenderPass
	{
	public:
		DeferredLightingPass(int width, int height, Handle<Shader> shader);
		~DeferredLightingPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Handle<Shader> _shader;
		Framebuffer _framebuffer;
	};
}