#pragma once

#include <glm/glm.hpp>
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
	struct ColorGradingSettings
	{
		float exposure = 0.0f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		glm::vec3 lift = glm::vec3(0.0f);
		glm::vec3 gamma = glm::vec3(1.0f);
		glm::vec3 gain = glm::vec3(1.0f);
	};

	struct TonemapSettings
	{
		enum class Mode { Linear, Reinhard, ACES };
		Mode mode = Mode::ACES;
	};

	struct PostProcessVolume
	{
		ColorGradingSettings colorGrading;
		TonemapSettings tonemap;
	};

	class PostProcessPass : public RenderPass
	{
	public:
		PostProcessPass(int width, int height, Handle<Shader> shader);

		void resize(int width, int height) override;
		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

		PostProcessVolume volume;

	private:
		Handle<Shader> _shader;
		Framebuffer _framebuffer;
	};
}