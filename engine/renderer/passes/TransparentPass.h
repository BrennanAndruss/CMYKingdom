#pragma once

#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"

namespace engine
{
	class Scene;
	class AssetManager;
	class RenderContext;
}

namespace engine
{
	class TransparentPass : public RenderPass
	{
	public:
		TransparentPass(int width, int height);
		~TransparentPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Framebuffer _framebuffer;
	};
}