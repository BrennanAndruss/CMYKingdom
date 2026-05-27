#pragma once

#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"
#include "resources/Handle.h"

// Forward declarations
namespace engine
{
	class Scene;
	class Object;
	class Camera;
	class AssetManager;
	class RenderContext;
	class Shader;
	struct Frustum;
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
		Framebuffer _waterSceneCopy;

		void drawObject(Object* object, const Scene& scene,
			const AssetManager& assets, const Camera* camera,
			const Framebuffer* sceneCopy = nullptr);
		void drawObjectCulled(Object* object, const Scene& scene,
			const AssetManager& assets, const Frustum& frustum,
			const Camera* camera, bool waterOnly = false,
			const Framebuffer* sceneCopy = nullptr);
	};
}