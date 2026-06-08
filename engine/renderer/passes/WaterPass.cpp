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
			{ AttachmentFormat::RGBA8 }
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

		// Copy opaque scene from lighting pass into scratch buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _framebuffer.getFboId());
		glBlitFramebuffer(
			0, 0, ctx.width, ctx.height,
			0, 0, ctx.width, ctx.height,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST
		);

		// Bind framebuffer from deferred lighting pass
		Framebuffer* framebuffer = ctx.sceneFramebuffer;
		framebuffer->bind();

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
				shader->setVec3("u_shallowColor", glm::vec3(0.3f, 0.66f, 0.9f));
				shader->setVec3("u_deepColor", glm::vec3(0.24f, 0.48f, 0.81f));
				shader->setFloat("u_terrainPlaneLen", mat->terrainPlaneLen);
				shader->setFloat("u_terrainHeightScale", mat->terrainHeightScale);

				// Sample the lit scene from deferred rendering for refraction effects
				GLuint sceneColor = _framebuffer.getColorAttachment(0);
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
			else
			{
				assert(false && "todo: implement generic transparent object rendering");
			}

			mesh->draw();
			shader->unbind();
		}

		glDepthMask(GL_TRUE);

		framebuffer->unbind();
	}
}