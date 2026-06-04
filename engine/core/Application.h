#pragma once

#include <glad/glad.h>
#include "core/AppConfig.h"
#include "editor/Editor.h"
#include "core/Window.h"
#include "core/EventCallbacks.h"
#include "physics/PhysicsSystem.h"
#include "renderer/Renderer.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Audio.h"

// Forward declarations
namespace engine
{
	class Game;
}

namespace engine
{
	class Application : public EventCallbacks
	{
	public:
		Application(const AppConfig& config);
		~Application();

		void run(std::unique_ptr<Game> game);

		void setClearColor(float r, float g, float b, float a);

		void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		void resizeCallback(GLFWwindow* window, int width, int height);
		void mouseCallback(GLFWwindow* window, double xPos, double yPos);
		void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

	private:
		AppConfig _config;
		Window _window;
		Editor _editor;
		PhysicsSystem _physics;
		Renderer _renderer;
		AssetManager _assets;
		Scene _scene;
		AudioEngine _audio;
		Game* _game = nullptr;

		bool _editorActive = false;
	};
}