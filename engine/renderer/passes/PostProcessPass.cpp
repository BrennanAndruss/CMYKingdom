#include "renderer/passes/PostProcessPass.h"

#include <GLFW/glfw3.h>
#include "core/Input.h"
#include "renderer/RenderContext.h"
#include "renderer/FullscreenQuad.h"
#include "renderer/resources/Shader.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"

namespace engine
{
	PostProcessPass::PostProcessPass(int width, int height, Handle<Shader> shader) :
		_shader(shader),
		_framebuffer(width, height, {
			{ AttachmentFormat::RGBA8 },
			{ AttachmentFormat::Depth24Stencil8 }
		}) {}

	void PostProcessPass::resize(int width, int height)
	{
		_framebuffer.resize(width, height);
	}

	void PostProcessPass::execute(const Scene& scene, const AssetManager& assets,
		RenderContext& ctx)
	{
		// Debugging
		static bool disable = false;
		if (Input::isKeyPressed(GLFW_KEY_F8))
			disable = !disable;

		if (disable) return;

		// Get scene color and shared depth/stencil textures
		GLuint hdrSceneTex = ctx.getBuffer(BufferNames::SceneColor);
		GLuint depthStencilTex = ctx.getBuffer(BufferNames::SceneDepth);

		_framebuffer.bind();

		// Link depth/stencil slot to the shared buffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
			GL_TEXTURE_2D, depthStencilTex, 0);
		
		glDisable(GL_DEPTH_TEST);

		auto* shader = assets.getShader(_shader);
		shader->bind();

		// Bind HDR scene
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, hdrSceneTex);
		shader->setInt("hdrSceneTex", 0);

		shader->setFloat("exposure", volume.colorGrading.exposure);
		shader->setFloat("contrast", volume.colorGrading.contrast);
		shader->setFloat("saturation", volume.colorGrading.saturation);
		shader->setVec3("lift", volume.colorGrading.lift);
		shader->setVec3("gamma", volume.colorGrading.gamma);
		shader->setVec3("gain", volume.colorGrading.gain);

		FullscreenQuad::getInstance().draw();

		shader->unbind();

		glEnable(GL_DEPTH_TEST);

		_framebuffer.unbind();

		// Pass updated buffer through the pipeline
		// Leave scene depth pointed to the shared buffer
		ctx.setBuffer(BufferNames::SceneColor, _framebuffer.getColorAttachment(0));
		ctx.sceneFramebuffer = &_framebuffer;
	}
}