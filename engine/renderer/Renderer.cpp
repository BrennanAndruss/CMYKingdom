#include "renderer/Renderer.h"

#include <glad/glad.h>
#include <cassert>
#include <iostream>
#include "core/Time.h"
#include "renderer/RenderContext.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/ForwardRenderPass.h"
#include "renderer/passes/DeferredGeometryPass.h"
#include "renderer/passes/DeferredLightingPass.h"
#include "renderer/passes/WaterPass.h"
#include "renderer/passes/SkyboxRenderPass.h"
#include "renderer/passes/PostProcessPass.h"
#include "renderer/passes/DebugRenderPass.h"
#include "renderer/passes/BlitPass.h"
#include "resources/AssetManager.h"

namespace engine
{
	// Debug profiling
	static int frameCount = 0;

	Renderer::Renderer(int width, int height, RenderingPath renderingPath) : 
		_width(width), 
		_height(height),
		_renderingPath(renderingPath),
		_cameraUBO(sizeof(CameraData), static_cast<GLuint>(UBOBindings::Camera)),
		_lightsUBO(MAX_LIGHTS * sizeof(LightData), static_cast<GLuint>(UBOBindings::Light)),
		_shadowUBO(sizeof(ShadowUBO), static_cast<GLuint>(UBOBindings::Shadow))
	{}

	Renderer::~Renderer() = default;

	void Renderer::init(AssetManager& assets)
	{
		// Load engine shaders and construct render passes
		Handle<Shader> depthShader = assets.loadEngineShader(
			"EngineShadowDepth", "shaders/shadowDepth.vert", "shaders/shadowDepth.frag");
		Handle<Shader> skinnedDepthShader = assets.loadEngineShader(
			"EngineShadowDepthSkinned", "shaders/shadowDepthSkinned.vert", "shaders/shadowDepth.frag");

		_shadowPass = std::make_unique<ShadowPass>(
			SHADOW_RESOLUTION, depthShader, skinnedDepthShader);

		Handle<Shader> skyboxShader = assets.loadEngineShader(
			"EngineSkybox", "shaders/skybox.vert", "shaders/skybox.frag");

		if (_renderingPath == RenderingPath::Forward)
		{
			_baseShader = assets.loadEngineShader(
				"EngineForward", "shaders/forward.vert", "shaders/forward.frag");
			_skinnedShader = assets.loadEngineShader(
				"EngineSkinnedForward", "shaders/skinned.vert", "shaders/forward.frag");
			_terrainShader = assets.loadEngineShader(
				"EngineTerrainForward", "shaders/forward.vert", "shaders/terrain.frag");

			assets.setDefaultShader(_baseShader);
			addRenderPass(std::make_unique<ForwardRenderPass>(_width, _height));
			addRenderPass(std::make_unique<SkyboxRenderPass>(skyboxShader));
		}
		else if (_renderingPath == RenderingPath::Deferred)
		{
			_baseShader = assets.loadEngineShader(
				"EngineGeometry", "shaders/geometry.vert", "shaders/geometry.frag");
			_skinnedShader = assets.loadEngineShader(
				"EngineSkinnedGeometry", "shaders/skinned.vert", "shaders/geometry.frag"
			);
			_terrainShader = assets.loadEngineShader(
				"EngineTerrainGeometry", "shaders/geometry.vert", "shaders/terrainGeom.frag");

			Handle<Shader> lightingShader = assets.loadEngineShader(
				"EngineLighting", "shaders/passthrough.vert", "shaders/lighting.frag");

			assets.setDefaultShader(_baseShader);
			addRenderPass(std::make_unique<DeferredGeometryPass>(_width, _height, _baseShader,
				_terrainShader, _skinnedShader));
			addRenderPass(std::make_unique<DeferredLightingPass>(_width, _height, lightingShader));
			addRenderPass(std::make_unique<SkyboxRenderPass>(skyboxShader));
			addRenderPass(std::make_unique<WaterPass>(_width, _height));
		}

		Handle<Shader> postProcessShader = assets.loadEngineShader(
			"EnginePostProcess", "shaders/passthrough.vert", "shaders/postProcess.frag");

		auto postProcessPass = std::make_unique<PostProcessPass>(_width, _height, postProcessShader);
		volume = &postProcessPass->volume;

		addRenderPass(std::move(postProcessPass));
		addRenderPass(std::make_unique<DebugRenderPass>());
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

		// Run shadow pass and upload shadow UBO
		// double startTime = glfwGetTime();
		_shadowPass->execute(scene, assets, _ctx);
		// glFinish();
		// double endTime = glfwGetTime();
		/*if (frameCount == 0)
			std::cout << "Shadow pass: " << (endTime - startTime) * 1000.0 << "ms\n";*/

		ShadowUBO shadowData = _shadowPass->getShadowUBO();
		_shadowUBO.update(&shadowData, sizeof(ShadowUBO));

		// Run render pipeline
		for (const auto& pass : _renderPasses)
		{
			// startTime = glfwGetTime();
			pass->execute(scene, assets, _ctx);
			// glFinish();
			// endTime = glfwGetTime();
			//if (frameCount == 0)
			//	std::cout << "Render pass: " << (endTime - startTime) * 1000.0 << "ms\n";
		}

		if (_postProcessEnabled)
		{
			for (const auto& pass : _postProcessPasses)
			{
				// startTime = glfwGetTime();
				pass->execute(scene, assets, _ctx);
				// glFinish();
				// endTime = glfwGetTime();
				//if (frameCount == 0)
				//	std::cout << "Post pass: " << (endTime - startTime) * 1000.0 << "ms\n";
			}
		}

		// Blit processed frame to screen
		//startTime = glfwGetTime();
		_blitPass->execute(scene, assets, _ctx);
		//glFinish();
		//endTime = glfwGetTime();
		//if (frameCount == 0)
			//std::cout << "Blit pass: " << (endTime - startTime) * 1000.0 << "ms\n";

		frameCount = (frameCount + 1) % 1000;
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
