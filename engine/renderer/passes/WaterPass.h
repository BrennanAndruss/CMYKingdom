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
	class WaterPass : public RenderPass
	{
	public:
		WaterPass(int width, int height);
		~WaterPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Framebuffer _framebuffer;
	};
}