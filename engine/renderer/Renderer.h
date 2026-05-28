#pragma once

#include <vector>
#include <memory>
#include "renderer/passes/RenderPass.h"
#include "renderer/passes/BlitPass.h"
#include "renderer/RenderingPath.h"
#include "renderer/RenderContext.h"
#include "renderer/UniformBuffer.h"
#include "scene/Scene.h"

// Forward declarations
namespace engine
{
	class AssetManager;
}

namespace engine
{
	constexpr int MAX_LIGHTS = 16;

	class Renderer
	{
	public:
		Renderer(int width, int height, RenderingPath renderingPath);
		~Renderer() = default;

		void init(AssetManager& assets);
		void resize(int width, int height);
		void render(const Scene& scene, const AssetManager& assets);

		void addRenderPass(std::unique_ptr<RenderPass> pass);
		RenderPass& addPostProcessPass(std::unique_ptr<RenderPass> pass);

		Handle<Shader> getBaseShader() const { return _baseShader; }
		Handle<Shader> getSkinnedShader() const { return _skinnedShader; }
		Handle<Shader> getTerrainShader() const { return _terrainShader; }

		void enablePostProcessing(bool enable) { _postProcessEnabled = enable; }

	private:
		std::vector<std::unique_ptr<RenderPass>> _renderPasses;
		std::vector<std::unique_ptr<RenderPass>> _postProcessPasses;
		std::unique_ptr<BlitPass> _blitPass;

		RenderingPath _renderingPath;
		Handle<Shader> _baseShader, _skinnedShader, _terrainShader;
		
		int _width, _height;
		RenderContext _ctx;
		UniformBuffer _cameraUBO, _lightsUBO;

		bool _postProcessEnabled = true;
	};
}