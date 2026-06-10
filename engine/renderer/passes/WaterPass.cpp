#include "renderer/passes/WaterPass.h"

#include <iostream>
#include "core/Time.h"
#include "renderer/RenderContext.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "scene/components/MeshRenderer.h"

namespace engine
{
	WaterPass::WaterPass(int width, int height) :
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA16F }
		}) {}

	WaterPass::~WaterPass() = default;

	void WaterPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
	}

	void WaterPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		// Collect water objects
		std::vector<Object*> waterObjects;
		for (const auto& object : scene.getObjects())
		{
			auto* mr = object->getComponent<MeshRenderer>();
			if (!mr) continue;

			auto* mat = assets.getMaterial(mr->material);
			if (mat && mat->renderMode == RenderMode::Water)
			{
				waterObjects.push_back(object.get());
			}
		}

		// Get scene color and shared depth/stencil textures
		GLuint hdrSceneTex = ctx.getBuffer(BufferNames::SceneColor);
		GLuint depthStencilTex = ctx.getBuffer(BufferNames::SceneDepth);

		// Copy opaque scene from lighting pass into local buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _framebuffer.getFboId());
		glBlitFramebuffer(
			0, 0, ctx.width, ctx.height,
			0, 0, ctx.width, ctx.height,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST
		);

		_framebuffer.bind();

		// Link depth/stencil slot to the shared buffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
			GL_TEXTURE_2D, depthStencilTex, 0);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_FALSE);

		for (const auto& object : waterObjects)
		{
			auto* mr = object->getComponent<MeshRenderer>();
			auto* mesh = assets.getMesh(mr->mesh);
			auto* mat = assets.getMaterial(mr->material);
			auto* shader = assets.getShader(mat->shader);
			if (!mesh || !shader) continue;

			const CameraData& camData = scene.getMainCamera()->getCameraData();

			shader->bind();

			if (mat->renderMode == RenderMode::Water)
			{
				shader->setMat4("u_model", object->transform.getWorldMatrix());
				shader->setMat4("u_view", camData.view);
				shader->setMat4("u_projection", camData.projection);
				shader->setFloat("u_time", Time::time());
				shader->setFloat("u_waveSpeed", 1.9f);
				shader->setFloat("u_waveLength", 3.0f);
				shader->setFloat("u_waveHeight", 0.25f);
				shader->setFloat("u_refractionStrength", 0.01f);
				shader->setFloat("u_depthScale", 0.50f);
				shader->setVec3("u_shallowColor", glm::vec3(0.1f, 0.75f, 0.85f));
				shader->setVec3("u_deepColor", glm::vec3(0.05f, 0.15f, 0.45f));
				shader->setFloat("u_terrainPlaneLen", mat->terrainPlaneLen);
				shader->setFloat("u_terrainHeightScale", mat->terrainHeightScale);

				// Sample the opaque scene texture from lighting pass for refraction effects
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, hdrSceneTex);
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
			else
			{
				assert(false && "todo: implement generic transparent object rendering");
			}

			mesh->draw();
			shader->unbind();
		}

		glDepthMask(GL_TRUE);

		_framebuffer.unbind();

		// Pass updated buffer through the pipeline
		// Leave scene depth pointed to the shared buffer
		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getColorAttachment(0));
		ctx.sceneFramebuffer = &_framebuffer;
	}
}