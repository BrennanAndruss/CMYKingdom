#include "renderer/Renderer.h"

#include <glad/glad.h>
#include <cassert>
#include <iostream>
#include "renderer/RenderContext.h"
#include "renderer/passes/ForwardRenderPass.h"
#include "renderer/passes/SkyboxRenderPass.h"
#include "renderer/passes/DebugRenderPass.h"
#include "resources/AssetManager.h"

namespace engine
{
	Renderer::Renderer(int width, int height) : 
		_width(width), 
		_height(height), 
		_cameraUBO(sizeof(CameraData), static_cast<GLuint>(UBOBindings::Camera)),
		_lightsUBO(MAX_LIGHTS * sizeof(LightData), static_cast<GLuint>(UBOBindings::Light)) {}

	void Renderer::init(AssetManager& assets)
	{
		// Load engine shaders
		Handle<Shader> forwardShader = assets.loadEngineShader(
			"EngineForward", "shaders/forward.vert", "shaders/forward.frag"
		);
		Handle<Shader> skyboxShader = assets.loadEngineShader(
			"EngineSkybox", "shaders/skybox.vert", "shaders/skybox.frag"
		);
		assets.setDefaultShader(forwardShader);

		// Construct render passes
		addRenderPass(std::make_unique<ForwardRenderPass>(_width, _height));
		addRenderPass(std::make_unique<DebugRenderPass>());
		addRenderPass(std::make_unique<SkyboxRenderPass>(skyboxShader));
		_blitPass = std::make_unique<BlitPass>();
	}

	void Renderer::resize(int width, int height)
	{
		_width = width;
		_height = height;

		for (auto& pass : _renderPasses)
		{
			pass->resize(width, height);
		}

		for (auto& pass : _postProcessPasses)
		{
			pass->resize(width, height);
		}
	}

	void Renderer::render(const Scene& scene, const AssetManager& assets)
	{
		glViewport(0, 0, _width, _height);
		
		// Reset context each frame
		_ctx.buffers.clear();
		_ctx.sceneFramebuffer = nullptr;
		_ctx.width = _width;
		_ctx.height = _height;

		// Update uniform buffer objects
		auto* camera = scene.getMainCamera();
		assert(camera != nullptr && "Scene has no camera.");
		_cameraUBO.update(&camera->getCameraData(), sizeof(CameraData));

		const auto& lights = scene.getLights();
		int numLights = std::min((int)lights.size(), MAX_LIGHTS);
		for (int i = 0; i < lights.size(); i++)
		{
			LightData data = lights[i]->getLightData();
			_lightsUBO.update(&data, sizeof(LightData), i * sizeof(LightData));
		}

		// Run render pipeline
		for (const auto& pass : _renderPasses)
		{
			pass->execute(scene, assets, _ctx);
		}

		if (_postProcessEnabled)
		{
			for (const auto& pass : _postProcessPasses)
			{
				pass->execute(scene, assets, _ctx);
			}
		}

		// Blit processed frame to screen
		_blitPass->execute(scene, assets, _ctx);
	}

	void Renderer::addRenderPass(std::unique_ptr<RenderPass> pass)
	{
		assert(pass != nullptr && "Null render pass.");
		_renderPasses.push_back(std::move(pass));
	}

	RenderPass& Renderer::addPostProcessPass(std::unique_ptr<RenderPass> pass)
	{
		assert(pass != nullptr && "Null post process pass.");
		return *_postProcessPasses.emplace_back(std::move(pass));
	}
}
