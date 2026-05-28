#include "renderer/passes/DeferredLightingPass.h"

#include <GLFW/glfw3.h>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/FullscreenQuad.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Texture.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"

namespace engine
{
	DeferredLightingPass::DeferredLightingPass(int width, int height, Handle<Shader> shader) :
		_shader(shader),
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA8 },
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
		// Debug views for G-buffer textures
		static int debugView = -1;
		if (Input::isKeyDown(GLFW_KEY_F4))
		{
			if (Input::isKeyPressed(GLFW_KEY_1)) debugView = 0;
			else if (Input::isKeyPressed(GLFW_KEY_2)) debugView = 1;
			else if (Input::isKeyPressed(GLFW_KEY_3)) debugView = 2;
		}
		else
			debugView = -1;

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

		Shader* shader = assets.getShader(_shader);
		if (!shader) return;

		// todo: handle water in TransparentPass
		static Shader* waterShader = assets.getShader("waterShader");
		if (shader == waterShader)
			return;

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
		shader->setInt("debugView", debugView);

		FullscreenQuad::getInstance().draw();

		shader->unbind();
		glEnable(GL_DEPTH_TEST);
		_framebuffer.unbind();

		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getColorAttachment(0));
		ctx.setBuffer(BufferNames::SceneDepth, _framebuffer.getDepthAttachment());
		ctx.sceneFramebuffer = &_framebuffer;
	}
}