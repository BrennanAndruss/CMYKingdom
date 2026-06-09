#pragma once

#include "scene/Scene.h"
#include "renderer/passes/RenderPass.h"
#include "renderer/resources/Shader.h"
#include "renderer/RenderContext.h"
#include "renderer/Framebuffer.h"
#include "resources/AssetManager.h"
#include "systems/Collectable.h"

struct PulseData
{
	glm::vec3 center;
	float radius = 0.0f;
	Collectable::Type type;
};

class ColorRestorationPass : public engine::RenderPass
{
public:
	ColorRestorationPass(int width, int height, Handle<engine::Shader> shader);

	void resize(int width, int height) override;
	void execute(const engine::Scene& scene, const engine::AssetManager& assets,
		engine::RenderContext& ctx) override;

	void setActivePulses(const std::vector<PulseData>& pulses) { _activePulses = pulses; }

	// Pulse variables
	float pulseThickness = 5.0f;
	float pulseSoftness = 2.0f;
	float pulseColorBoost = 0.2f;
	static constexpr int MAX_PULSES = 4;

	// Color restoration variables
	float cyan = 0.0f, magenta = 0.0f, yellow = 0.0f, key = 0.0f;

private:
	Handle<engine::Shader> _shader;
	engine::Framebuffer _framebuffer;
	std::vector<PulseData> _activePulses;
};