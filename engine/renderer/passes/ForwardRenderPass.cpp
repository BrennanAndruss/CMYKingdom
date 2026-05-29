#include "renderer/passes/ForwardRenderPass.h"

#include <glad/glad.h>
#include <algorithm>
#include <string>
#include <vector>
#include "core/Time.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "renderer/resources/Cubemap.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Animator.h"
#include "scene/components/MeshRenderer.h"
#include "scene/components/GrassRenderer.h"
#include "../game/MyGame.h"

namespace engine
{
	static constexpr std::size_t MAX_SHADER_BONES = 100;

	ForwardRenderPass::ForwardRenderPass(int width, int height) :
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24Stencil8 }
		}),
		_waterSceneCopy(width, height, {
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24 }
		}) {}

	ForwardRenderPass::~ForwardRenderPass() = default;

	void ForwardRenderPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
		_waterSceneCopy.resize(width, height);
	}

	void ForwardRenderPass::execute(const Scene& scene, const AssetManager& assets, 
		RenderContext& ctx)
	{
		_framebuffer.bind();
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		Camera* camera = scene.getMainCamera();
		if (!camera) return;
		Frustum frustum = Frustum::fromCamera(camera->getCameraData());

		// Cull and draw
		for (const auto& object : scene.getRootObjects())
		{
			drawObjectCulled(object, scene, assets, frustum, camera, false, nullptr);
		}

		// Copy opaque scene into a texture pair we can safely sample from when drawing water.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebuffer.getFboId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _waterSceneCopy.getFboId());
		glBlitFramebuffer(
			0, 0, ctx.width, ctx.height,
			0, 0, ctx.width, ctx.height,
			GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
			GL_NEAREST
		);
		glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer.getFboId());

		// Draw water after the scene has been captured.
		for (const auto& object : scene.getRootObjects())
		{
			drawObjectCulled(object, scene, assets, frustum, camera, true, &_waterSceneCopy);
		}

		// Clean up state for next pass
		glDisable(GL_STENCIL_TEST);
		glStencilMask(0x00);
		_framebuffer.unbind();

		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getColorAttachment(0));
		ctx.setBuffer(BufferNames::SceneDepth, _framebuffer.getDepthAttachment());
		ctx.sceneFramebuffer = &_framebuffer;
	}

	void ForwardRenderPass::drawObjectCulled(Object* object, const Scene& scene,
		const AssetManager& assets, const Frustum& frustum,
		const Camera* camera, bool waterOnly, const Framebuffer* sceneCopy)
	{
		const bool isWaterObject = (object->name == "TempWaterPlane");
		if (waterOnly != isWaterObject)
		{
			for (auto* childTransform : object->transform.getChildren())
			{
				if (Object* child = childTransform->owner)
				{
					drawObjectCulled(child, scene, assets, frustum, camera, waterOnly, sceneCopy);
				}
			}
			return;
		}

		// Test object's hierarchy bbox
		if (!object->transform.getChildren().empty())
		{
			// Test combined bbox to cull the subtree
			BBox hierarchyBBox = object->getHierarchyBBox(assets);
			if (!hierarchyBBox.intersectsFrustum(frustum))
			{
				// std::cout << "Culled " << object->name << " and its subtree\n";
				return;
			}
		}

		// Test object's own bbox
		if (auto* mr = object->getComponent<MeshRenderer>())
		{
			const BBox& worldBBox = object->getWorldBBox(assets);
			if (!worldBBox.intersectsFrustum(frustum))
			{
				return;
			}

			drawObject(object, scene, assets, camera, sceneCopy);
		}
		if (auto* grass = object->getComponent<GrassRenderer>())
		{
			grass->draw(assets, frustum, Time::time());
		}

		// Recurse into children
		for (auto* childTransform : object->transform.getChildren())
		{
			if (Object* child = childTransform->owner)
			{
				drawObjectCulled(child, scene, assets, frustum, camera, waterOnly, sceneCopy);
			}
		}
	}

	void ForwardRenderPass::drawObject(Object* object, const Scene& scene,
		const AssetManager& assets, const Camera* camera, const Framebuffer* sceneCopy)
	{
		auto* meshRenderer = object->getComponent<MeshRenderer>();
		if (!meshRenderer) return;

		auto* mesh = assets.getMesh(meshRenderer->mesh);
		auto* mat = assets.getMaterial(meshRenderer->material);
		if (!mesh || !mat) return;

		auto* shader = assets.getShader(mat->shader);
		if (!shader) return;

		// Write stencil
		if (meshRenderer->writeStencil)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 1, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0xFF);
		}
		else
		{
			glStencilMask(0x00);
		}

		shader->bind();

		const bool isWaterObject = (object->name == "TempWaterPlane");

		Cubemap* irradianceMap = nullptr;
		if (scene.hasIrradianceMap() && MyGame::getActiveGame()->terrainSkyLightingEnabled)
		{
			irradianceMap = assets.getCubemap(scene.getIrradianceMap());
			if (irradianceMap)
			{
				irradianceMap->bindToUnit(shader->getUniform("irradianceMap"), 10);
				shader->setInt("useIrradianceMap", 1);
				shader->setFloat("irradianceStrength", 1.2f);
				shader->setFloat("irradianceStrength", 1.2f);
			}
		}
		else
		{
			shader->setInt("useIrradianceMap", 0);
		}

		shader->setMat4("model", object->transform.getWorldMatrix());
		shader->setVec3("mat.ambient", mat->ambient);
		shader->setVec3("mat.diffuse", mat->diffuse);
		shader->setVec3("mat.specular", mat->specular);
		shader->setFloat("mat.shininess", mat->shininess);
		shader->setInt("numLights", scene.getLights().size());
		shader->setFloat("uTime", Time::time());

		if (isWaterObject)
		{
			const CameraData& camData = camera->getCameraData();
			shader->setMat4("u_model", object->transform.getWorldMatrix());
			shader->setMat4("u_view", camData.view);
			shader->setMat4("u_projection", camData.projection);
			shader->setFloat("u_time", Time::time());
			shader->setFloat("u_waveSpeed", 1.9f);
			shader->setFloat("u_waveLength", 3.0f);
			shader->setFloat("u_waveHeight", 0.25f);
			shader->setFloat("u_refractionStrength", 0.01f);
			shader->setFloat("u_depthScale", 0.50f);
			shader->setVec3("u_shallowColor", glm::vec3(0.3f, 0.66f, 0.9f));
			shader->setVec3("u_deepColor", glm::vec3(0.24f, 0.48f, 0.81f));
			shader->setFloat("u_terrainPlaneLen", mat->terrainPlaneLen);
			shader->setFloat("u_terrainHeightScale", mat->terrainHeightScale);

			if (sceneCopy)
			{
				const GLuint sceneColor = sceneCopy->getColorAttachment(0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sceneColor);
				glUniform1i(shader->getUniform("u_sceneColorTex"), 0);

				if (auto* noiseTex = assets.getTexture("waterNoiseTex"))
				{
					noiseTex->bindToUnit(shader->getUniform("u_noiseTex"), 1);
				}

				if (auto* terrainHeightTex = assets.getTexture(mat->terrainHeightTex))
				{
					terrainHeightTex->bindToUnit(shader->getUniform("u_terrainHeightTex"), 2);
				}
			}
		}

		// Bind textures
		Texture* terrainSplat0 = nullptr;
		Texture* terrainGrass = nullptr;
		Texture* terrainSand = nullptr;
		Texture* terrainRock = nullptr;
		Texture* terrainSnow = nullptr;

		Texture* difTex = nullptr;
		Texture* specTex = nullptr;

		if (mat->renderMode == RenderMode::Terrain)
		{
			terrainSplat0 = assets.getTexture(mat->splat0);
			terrainGrass = assets.getTexture(mat->terrainGrass);
			terrainSand = assets.getTexture(mat->terrainSand);
			terrainRock = assets.getTexture(mat->terrainRock);
			terrainSnow = assets.getTexture(mat->terrainSnow);

			if (terrainSplat0)
				terrainSplat0->bindToUnit(shader->getUniform("splat0"), 0);
			if (terrainGrass)
				terrainGrass->bindToUnit(shader->getUniform("terrainGrass"), 1);
			if (terrainSand)
				terrainSand->bindToUnit(shader->getUniform("terrainSand"), 2);
			if (terrainRock)
				terrainRock->bindToUnit(shader->getUniform("terrainRock"), 3);
			if (terrainSnow)
				terrainSnow->bindToUnit(shader->getUniform("terrainSnow"), 4);

			shader->setFloat("terrainTextureTiling", mat->terrainTextureTiling);
		}
		else
		{
			difTex = assets.getTexture(mat->difTex);
			specTex = assets.getTexture(mat->specTex);

			if (difTex)  difTex->bind(shader->getUniform("mat.difTex"));
			if (specTex) specTex->bind(shader->getUniform("mat.specTex"));
		}

		// Bone matrices for skinned meshes
		Animator* animator = nullptr;
		if (mesh->isSkinned() && (animator = object->getComponent<Animator>()))
		{
			const auto& boneMatrices = animator->getBoneMatrices();
			const int numBones = static_cast<int>(
				std::min(boneMatrices.size(), MAX_SHADER_BONES));
			shader->setInt("isSkinned", 1);
			shader->setInt("numBones", numBones);
			for (int i = 0; i < numBones; ++i)
			{
				shader->setMat4("bones[" + std::to_string(i) + "]",
					boneMatrices[static_cast<std::size_t>(i)]);
			}
		}

		mesh->draw();

		if (isWaterObject)
		{
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if (terrainSplat0)	terrainSplat0->unbindFromUnit(0);
		if (terrainGrass)	terrainGrass->unbindFromUnit(1);
		if (terrainSand)	terrainSand->unbindFromUnit(2);
		if (terrainRock)	terrainRock->unbindFromUnit(3);
		if (terrainSnow)	terrainSnow->unbindFromUnit(4);
		if (difTex)			difTex->unbind();
		if (specTex)		specTex->unbind();

		if (irradianceMap)
		{
			irradianceMap->unbindFromUnit(10);
		}

		shader->unbind();
	}
}