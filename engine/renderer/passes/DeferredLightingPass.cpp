#include "renderer/passes/DeferredLightingPass.h"

#include <GLFW/glfw3.h>
#include "core/Input.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/RenderContext.h"
#include "renderer/FullscreenQuad.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Texture.h"
#include "renderer/resources/Cubemap.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"

namespace engine
{
	DeferredLightingPass::DeferredLightingPass(int width, int height, Handle<Shader> shader) :
		_shader(shader),
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA16F },
			{ AttachmentFormat::Depth24Stencil8 }
		}) {}

	DeferredLightingPass::~DeferredLightingPass() = default;

	void DeferredLightingPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
	}

	void DeferredLightingPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		// Debug views for G-buffer textures and shadow cascades
		static bool debugMode = false;
		static int selectedDebugView = -1;         // 0/1/2 = views selected while debugMode is enabled
		static bool showCascadesToggle = false;

		// Toggle debug mode
		if (Input::isKeyPressed(GLFW_KEY_F4))
		{
			debugMode = !debugMode;
		}

		// When debugMode is active, pressing 1/2/3 will toggle the corresponding view selection
		if (debugMode)
		{
			if (Input::isKeyPressed(GLFW_KEY_1))
			{
				selectedDebugView = (selectedDebugView == 0) ? -1 : 0;
			}
			else if (Input::isKeyPressed(GLFW_KEY_2))
			{
				selectedDebugView = (selectedDebugView == 1) ? -1 : 1;
			}
			else if (Input::isKeyPressed(GLFW_KEY_3))
			{
				selectedDebugView = (selectedDebugView == 2) ? -1 : 2;
			}
		}

		// Toggle cascade visualization
		if (Input::isKeyPressed(GLFW_KEY_F2))
		{
			showCascadesToggle = !showCascadesToggle;
		}

		// Only show the selected view when debugMode is enabled
		int debugView = debugMode ? selectedDebugView : -1;
		int showCascades = showCascadesToggle ? 1 : 0;

		// Blit stencil from gBuffer to lighting framebuffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _framebuffer.getFboId());
		glBlitFramebuffer(
			0, 0, ctx.width, ctx.height,
			0, 0, ctx.width, ctx.height,
			GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
			GL_NEAREST
		);

		_framebuffer.bind();
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);

		Shader* shader = assets.getShader(_shader);
		if (!shader) return;

		shader->bind();

		// Bind G-buffer textures
		GLuint positionTex = ctx.getBuffer(BufferNames::GPosition);
		GLuint normalTex = ctx.getBuffer(BufferNames::GNormal);
		GLuint albedoTex = ctx.getBuffer(BufferNames::GAlbedo);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, positionTex);
		shader->setInt("gPosition", 0);

		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, normalTex);
		shader->setInt("gNormalShine", 1);

		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, albedoTex);
		shader->setInt("gAlbedoSpec", 2);

		shader->setInt("numLights", static_cast<int>(scene.getLights().size()));
		
		shader->setInt("shadowSampleCount", shadowSampleCount);
		shader->setFloat("shadowPenumbraSize", shadowPenumbraSize);

		shader->setInt("debugView", debugView);
		shader->setInt("showCascades", showCascades);

		// Bind shadow maps
		GLuint shadowMap = ctx.getBuffer(BufferNames::Shadow);
		glActiveTexture(GL_TEXTURE0 + 11);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMap);
		shader->setInt("shadowMaps", 11);

		if (scene.hasIrradianceMap())
		{
			auto* irradianceMap = assets.getCubemap(scene.getIrradianceMap());
			assert(irradianceMap && "No irradiance map found.");

			irradianceMap->bindToUnit(shader->getUniform("irradianceMap"), 10);
			shader->setInt("useIrradianceMap", 1);
			shader->setFloat("irradianceStrength", 1.2f);
		}
		else
		{
			shader->setInt("useIrradianceMap", 0);
		}

		FullscreenQuad::getInstance().draw();

		shader->unbind();

		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);

		_framebuffer.unbind();

		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getColorAttachment(0));
		ctx.setBuffer(BufferNames::SceneDepth, _framebuffer.getDepthAttachment());
		ctx.sceneFramebuffer = &_framebuffer;
	}
}