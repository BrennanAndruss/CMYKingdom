#pragma once

#include <glm/glm.hpp>
#include <array>
#include "renderer/passes/RenderPass.h"
#include "renderer/Framebuffer.h"
#include "renderer/UniformBuffer.h"
#include "resources/Handle.h"

namespace engine
{
	class Scene;
	class Object;
	class AssetManager;
	class RenderContext;
	class Shader;
	class Camera;
	struct Frustum;
	struct BBox;
}

namespace engine
{
	constexpr int NUM_CASCADES = 3;
	constexpr int SHADOW_RESOLUTION = 2048;

	struct ShadowUBO
	{
		glm::mat4 cascadeLightSpaces[NUM_CASCADES];
		glm::vec4 cascadeRadii;
		glm::vec4 cascadeSplits;
		int numCascades = NUM_CASCADES;
		float shadowBias = 0.0005f;
		float _pad[2];
	};

	class ShadowPass : public RenderPass
	{
	public:
		ShadowPass(int shadowResolution, Handle<Shader> depthShader,
			Handle<Shader> skinnedDepthShader);
		~ShadowPass();

		void execute(const Scene& scene, const AssetManager& assets,
			RenderContext& ctx) override;

		const ShadowUBO& getShadowUBO() const { return _shadowUBO; }

		// Cascade parameters
		float lambda = 0.15f;
		float shadowBias = 0.0015f;
		float maxShadowDistance = 500.0f;

	private:
		Handle<Shader> _depthShader, _skinnedDepthShader;

		Framebuffer _shadowFramebuffer;
		int _shadowResolution;

		ShadowUBO _shadowUBO;

		void computeCascadeSplits(const Camera& camera);
		void computeLightSpaceMatrices(const Camera& camera, 
			BBox sceneBBox, glm::vec3 lightDir);
		void renderCascade(int index, const Scene& scene,
			const AssetManager& assets);
		void drawObjectCulled(int index, Object* object, const Scene& scene,
			const AssetManager& assets, const Frustum& lightFrustum);
		void drawObject(int index, Object* object, const Scene& scene,
			const AssetManager& assets);
	};
}