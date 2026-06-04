#pragma once

#include "core/AppConfig.h"

// Forward declarations
namespace engine
{
	class AssetManager;
	class Renderer;
	class Scene;
	class AudioEngine;
}

namespace engine
{
	class Game
	{
	public:
		virtual ~Game() = default;
		virtual void init(AssetManager& assets, Renderer& renderer, Scene& scene, AudioEngine& audio, const AppConfig& config) = 0;
		virtual void update(float deltaTime) = 0;
		virtual void setEditorMode(bool editorActive, Scene& scene) {}
		virtual void setEditorSelectionLock(bool locked, Scene& scene) {}
		virtual void drawUI() {}
	};
}