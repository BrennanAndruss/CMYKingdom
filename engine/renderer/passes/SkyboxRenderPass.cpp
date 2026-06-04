#include "renderer/passes/SkyboxRenderPass.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "resources/AssetManager.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Cubemap.h"
#include "renderer/RenderContext.h"
#include "renderer/Framebuffer.h"
#include "scene/Scene.h"
#include "scene/components/Camera.h"

namespace engine
{
	static const float SKYBOX_VERTICES[] =
	{
		-1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
	};

	SkyboxRenderPass::SkyboxRenderPass(Handle<Shader> shader) : _shader(shader)
	{
		glGenVertexArrays(1, &_vao);
		glGenBuffers(1, &_vbo);

		glBindVertexArray(_vao);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(SKYBOX_VERTICES), SKYBOX_VERTICES, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		glBindVertexArray(0);
	}

	SkyboxRenderPass::~SkyboxRenderPass()
	{
		if (_vbo != 0) glDeleteBuffers(1, &_vbo);
		if (_vao != 0) glDeleteVertexArrays(1, &_vao);
	}

	void SkyboxRenderPass::execute(const Scene& scene, 
		const AssetManager &assets, RenderContext& ctx)
	{
		if (!scene.hasSkybox()) return;

		auto* shader = assets.getShader(_shader);
		auto* cubemap = assets.getCubemap(scene.getSkybox());
		auto* camera = scene.getMainCamera();
		if (!shader || !cubemap || !camera) return;

		Framebuffer* framebuffer = ctx.sceneFramebuffer;
		framebuffer->bind();

		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);

		shader->bind();

		const CameraData& camData = camera->getCameraData();
		glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(camData.view));

		shader->setMat4("view", viewNoTranslation);
		shader->setMat4("projection", camData.projection);

		cubemap->bind(shader->getUniform("skybox"));

		glBindVertexArray(_vao);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);

		cubemap->unbind();
		shader->unbind();

		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);

		framebuffer->unbind();
	}
}