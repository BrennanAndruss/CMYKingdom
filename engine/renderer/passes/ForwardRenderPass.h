#pragma once

#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"
#include "resources/Handle.h"

// Forward declarations
namespace engine
{
	class Scene;
	class AssetManager;
	class RenderContext;
	class Shader;
}

namespace engine
{
	class ForwardRenderPass : public RenderPass
	{
	public:
		ForwardRenderPass(int width, int height, Handle<Shader> shader);
		~ForwardRenderPass();

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

	private:
		Handle<Shader> _shader;
		Framebuffer _framebuffer;
	};
}