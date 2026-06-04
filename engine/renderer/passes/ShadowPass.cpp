#include "renderer/passes/ShadowPass.h"

#include <GLFW/glfw3.h>
#include <core/Input.h>
#include "renderer/BoundingVolume.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/MeshRenderer.h"
#include "scene/components/Animator.h"
#include "scene/components/Light.h"

namespace engine
{
	static constexpr std::size_t MAX_SHADER_BONES = 100;

	ShadowPass::ShadowPass(int shadowResolution, Handle<Shader> depthShader,
		Handle<Shader> skinnedDepthShader) :
		_shadowResolution(shadowResolution),
		_depthShader(depthShader),
		_skinnedDepthShader(skinnedDepthShader),
		_shadowFramebuffer(shadowResolution, shadowResolution, {
			{ AttachmentFormat::Depth32FShadowArray, GL_CLAMP_TO_BORDER, true, NUM_CASCADES }
		}) {}
	
	ShadowPass::~ShadowPass() = default;

	void ShadowPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		// Debug input to adjust cascades
		if (Input::isKeyPressed(GLFW_KEY_LEFT_BRACKET))
		{
			shadowBias -= 0.0001f;
			std::cout << "shadow bias: " << shadowBias << "\n";
			//lambda -= 0.05f;
			//std::cout << "lambda: " << lambda << "\n";
			//for (int i = 0; i < NUM_CASCADES; i++)
			//{
			//	std::cout << "cascade " << i << " split depth: " << _shadowUBO.cascadeSplits[i] << "\n";
			//}
		}
		if (Input::isKeyPressed(GLFW_KEY_RIGHT_BRACKET))
		{
			shadowBias += 0.0001f;
			std::cout << "shadow bias: " << shadowBias << "\n";
			//lambda += 0.05f;
			//std::cout << "lambda: " << lambda << "\n";
			//for (int i = 0; i < NUM_CASCADES; i++)
			//{
			//	std::cout << "cascade " << i << " split depth: " << _shadowUBO.cascadeSplits[i] << "\n";
			//}
		}

		Camera* camera = scene.getMainCamera();
		assert(camera && "No main camera assigned.");

		// Find the first directional light
		const DirectionalLight* dirLight = nullptr;
		for (const auto& light : scene.getLights())
		{
			dirLight = dynamic_cast<const DirectionalLight*>(light);
			if (dirLight) break;
		}
		if (!dirLight) return;

		// Compute cascade split depths and light space matrices for each cascade
		computeCascadeSplits(*camera);

		BBox sceneBBox = scene.getSceneBBox(assets);
		glm::vec3 lightDir = -dirLight->getDirection();
		computeLightSpaceMatrices(*camera, sceneBBox, lightDir);

		// _shadowUBO.shadowBias = shadowBias;
		_shadowUBO.numCascades = NUM_CASCADES;

		
		// Shift depth values to reduce shadow acne
		_shadowFramebuffer.bind();
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 2.0f);

		// Render each cascade
		for (int i = 0; i < NUM_CASCADES; i++)
		{
			// Write depth data into layer index of shadow map array
			_shadowFramebuffer.attachLayer(i);
			glClear(GL_DEPTH_BUFFER_BIT);

			renderCascade(i, scene, assets);
		}

		glDisable(GL_POLYGON_OFFSET_FILL);
		_shadowFramebuffer.unbind();

		// Register shadow maps to render context
		ctx.setBuffer(BufferNames::Shadow, _shadowFramebuffer.getDepthAttachment());
		ctx.shadowUBO = &_shadowUBO;
	}

	void ShadowPass::computeCascadeSplits(const Camera& camera)
	{
		float camNear = camera.getNearPlane();
		float camFar = camera.getFarPlane();

		// Limit shadow distance to maxShadowDistance
		camFar = std::min(camFar, maxShadowDistance);

		float range = camFar - camNear;
		float ratio = camFar / camNear;

		for (int i = 0; i < NUM_CASCADES; i++)
		{
			float p = static_cast<float>(i + 1) / NUM_CASCADES;
			float logSplit = camNear * std::pow(ratio, p);
			float uniformSplit = glm::mix(camNear, camFar, p);

			// Blend uniform and logarithmic split
			float d = glm::mix(logSplit, uniformSplit, lambda);
			_shadowUBO.cascadeSplits[i] = d;
		}
	}

	void ShadowPass::computeLightSpaceMatrices(const Camera& camera, 
		BBox sceneBBox, glm::vec3 lightDir)
	{
		float nearSplit = camera.getNearPlane();
		float farSplit;

		lightDir = glm::normalize(lightDir);
		const glm::vec3 up = (std::abs(lightDir.y) > 0.99f)
			? glm::vec3(0, 0, 1)
			: glm::vec3(0, 1, 0);

		for (int i = 0; i < NUM_CASCADES; i++)
		{
			farSplit = _shadowUBO.cascadeSplits[i];			

			const CameraData& camData = camera.getCameraData();
			float aspect = camera.getAspect();
			float fov = camera.getFov();

			// Build perspective matrix for this cascade slice
			glm::mat4 sliceP = glm::perspective(glm::radians(fov), aspect,
				nearSplit, farSplit);
			glm::mat4 invPV = glm::inverse(sliceP * camData.view);

			// Transform unit cube corners to world space
			glm::vec3 corners[8];
			for (int i = 0; i < 8; i++)
			{
				glm::vec4 p;
				p.x = (i & 1) ? -1.0f : 1.0f;
				p.y = (i & 2) ? -1.0f : 1.0f;
				p.z = (i & 4) ? -1.0f : 1.0f;
				p.w = 1.0f;
				glm::vec4 w = invPV * p;
				corners[i] = glm::vec3(w) / w.w;
			}

			// Create subfrusta bounding spheres for shadow to prevent shadow crawling
			// Min at corner[0], max at corner[7]
			glm::vec3 center = (corners[0] + corners[7]) * 0.5f;

			float radius = 0.0f;
			for (int k = 0; k < 8; k++)
			{
				float d = glm::length(corners[k] - center);
				if (d > radius)
					radius = d;
			}

			// Construct stable view frame so all shadow casters are in front of light
			float sceneZExtent = glm::length(sceneBBox.max - sceneBBox.min);
			glm::vec3 lightPos = center + lightDir * (radius + sceneZExtent);
			glm::mat4 lightView = glm::lookAt(lightPos, center, up);

			// Transform scene bbox corners to light space
			glm::vec3 sceneCorners[8] = {
				{ sceneBBox.min.x, sceneBBox.min.y, sceneBBox.min.z },
				{ sceneBBox.max.x, sceneBBox.min.y, sceneBBox.min.z },
				{ sceneBBox.min.x, sceneBBox.max.y, sceneBBox.min.z },
				{ sceneBBox.max.x, sceneBBox.max.y, sceneBBox.min.z },
				{ sceneBBox.min.x, sceneBBox.min.y, sceneBBox.max.z },
				{ sceneBBox.max.x, sceneBBox.min.y, sceneBBox.max.z },
				{ sceneBBox.min.x, sceneBBox.max.y, sceneBBox.max.z },
				{ sceneBBox.max.x, sceneBBox.max.y, sceneBBox.max.z }
			};

			// Find z-range of shadow casters in light space
			float minZ = (std::numeric_limits<float>::max)();
			float maxZ = -(std::numeric_limits<float>::max)();
			for (int i = 0; i < 8; i++)
			{
				glm::vec4 lightSpaceCorner = lightView * glm::vec4(sceneCorners[i], 1.0f);
				if (lightSpaceCorner.z < minZ) minZ = lightSpaceCorner.z;
				if (lightSpaceCorner.z > maxZ) maxZ = lightSpaceCorner.z;
			}

			// Create orthographic projection bounding shadow casters in light space
			float padding = 5.0f;
			float orthoNear = -maxZ - padding;
			float orthoFar = -minZ + padding;

			glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius,
				orthoNear, orthoFar);

			// Transform world origin to light clip space and scale to texel units
			glm::mat4 lsNoSnap = lightProj * lightView;
			glm::vec4 originLS = lsNoSnap * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			originLS *= static_cast<float>(_shadowResolution) * 0.5f;

			// Snap to nearest texel to prevent shadow shimmer as camera moves
			glm::vec4 rounded = glm::round(originLS);

			// Compute sub-texel offset and convert back to NDC
			glm::vec4 snapOffset = (rounded - originLS) *
				(2.0f / static_cast<float>(_shadowResolution));

			// Apply translation to projection matrix
			glm::mat4 snap = glm::mat4(1.0f);
			snap[3][0] = snapOffset.x;
			snap[3][1] = snapOffset.y;
			lightProj = snap * lightProj;

			_shadowUBO.cascadeLightSpaces[i] = lightProj * lightView;
			_shadowUBO.cascadeRadii[i] = radius;

			// Update near split for next cascade
			nearSplit = _shadowUBO.cascadeSplits[i];
		}
	}

	void ShadowPass::renderCascade(int index, const Scene& scene,
		const AssetManager& assets)
	{
		// todo: implement shadow frustum culling
		for (const auto& object : scene.getObjects())
		{
			auto* meshRenderer = object->getComponent<MeshRenderer>();
			if (!meshRenderer) continue;

			auto* mesh = assets.getMesh(meshRenderer->mesh);
			auto* mat = assets.getMaterial(meshRenderer->material);
			if (!mesh || !mat || mat->renderMode == RenderMode::Transparent
				|| mat->renderMode == RenderMode::Water) continue;

			auto* animator = object->getComponent<Animator>();
			bool skinned = mesh->isSkinned() && animator;

			Shader* shader = assets.getShader(
				skinned ? _skinnedDepthShader : _depthShader);
			assert(shader && "Shader not found.");

			shader->bind();
			shader->setMat4("lightSpace", _shadowUBO.cascadeLightSpaces[index]);
			shader->setMat4("model", object->transform.getWorldMatrix());

			if (skinned)
			{
				const auto& boneMatrices = animator->getBoneMatrices();
				const int numBones = static_cast<int>(
					std::min(boneMatrices.size(), MAX_SHADER_BONES));
				shader->setInt("isSkinned", 1);
				shader->setInt("numBones", numBones);
				for (int i = 0; i < numBones; ++i)
				{
					shader->setMat4("bones[" + std::to_string(i) + "]",
						boneMatrices[i]);
				}
			}

			mesh->draw();
			shader->unbind();
		}
	}
}