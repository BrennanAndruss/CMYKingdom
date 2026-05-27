#include "passes/ColorRestorationPass.h"

#include "renderer/FullscreenQuad.h"

ColorRestorationPass::ColorRestorationPass(int width, int height, 
	Handle<engine::Shader> shader) : 
	_shader(shader),
	_framebuffer(width, height, {
		{ engine::AttachmentFormat::RGBA8 },
		{ engine::AttachmentFormat::Depth24Stencil8 }
	}) {}

void ColorRestorationPass::resize(int width, int height)
{
	_framebuffer.resize(width, height);
}

void ColorRestorationPass::execute(const engine::Scene& scene, 
	const engine::AssetManager& assets, engine::RenderContext& ctx)
{
	// Blit scene FBO into local FBO to use stencil values
	glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.sceneFramebuffer->getFboId());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _framebuffer.getFboId());
	glBlitFramebuffer(
		0, 0, ctx.width, ctx.height,
		0, 0, ctx.width, ctx.height,
		GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
		GL_NEAREST
	);

	_framebuffer.bind();
	// glClear(GL_COLOR_BUFFER_BIT);

	// Only process pixels where stencil != 1
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilMask(0x00);
	glDisable(GL_DEPTH_TEST);

	auto* shader = assets.getShader(_shader);
	
	// Set shader uniforms
	shader->bind();
	shader->setInt("sceneTex", 0);
	shader->setFloat("cyan", cyan);
	shader->setFloat("magenta", magenta);
	shader->setFloat("yellow", yellow);
	shader->setFloat("key", key);

	GLuint inputTex = ctx.getBuffer(engine::BufferNames::SceneColor);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, inputTex);

	engine::FullscreenQuad::getInstance().draw();

	shader->unbind();
	glEnable(GL_DEPTH_TEST);
	_framebuffer.unbind();

	// Register outputs with render context
	ctx.sceneFramebuffer = &_framebuffer;
	ctx.setBuffer(engine::BufferNames::SceneColor, 
		_framebuffer.getColorAttachment(0));
	ctx.setBuffer(engine::BufferNames::SceneDepth,
		_framebuffer.getDepthAttachment());
}