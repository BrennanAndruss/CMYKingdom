#include "core/Application.h"

#include "core/Game.h"
#include "core/Input.h"
#include "core/Time.h"

namespace engine 
{
    Application::Application(const AppConfig& config) : 
        _config(config),
        _window(config.width, config.height, config.title),
        _renderer(config.width, config.height, config.renderingPath),
        _assets(config.assetsDir)        
    {
        if (config.depthTest) glEnable(GL_DEPTH_TEST);
        if (config.cullFace)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }

        _window.setEventCallbacks(this);
        _scene.setPhysicsSystem(&_physics);
        _renderer.init(_assets);

        int framebufferWidth = 0, framebufferHeight = 0;
        glfwGetFramebufferSize(_window.getHandle(), &framebufferWidth, &framebufferHeight);
        if (framebufferWidth > 0 && framebufferHeight > 0)
        {
            _config.width = framebufferWidth;
            _config.height = framebufferHeight;
        }
        _renderer.resize(framebufferWidth, framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        _editor.initialize(_window.getHandle());
    }

    Application::~Application()
    {
        _editor.shutdown();
    }

    void Application::run(std::unique_ptr<Game> game)
    {
		_game = game.get();
        game->init(_assets, _renderer, _scene, _config);
		_game->setEditorMode(_editorActive, _scene);
        _scene.start();

        // Initialize camera aspect ratio on startup
        if (auto* camera = _scene.getMainCamera())
            camera->setAspect(static_cast<float>(_config.width) / static_cast<float>(_config.height));

        while (!_window.shouldClose())
        {
            Time::update();
            Input::update();
            _window.pollEvents();
            
			if (_game)
			{
				_game->setEditorSelectionLock(_editorActive && _editor.hasSelectedObject(), _scene);
			}

            const bool editorEnabledThisFrame = _editorActive;
            if (editorEnabledThisFrame)
            {
                _editor.beginFrame();
            }

            // Update gameplay logic
            game->update(Time::deltaTime());
            _scene.update(Time::deltaTime(), _assets);

            // Render
            _renderer.render(_scene, _assets);

            if (editorEnabledThisFrame)
            {
                _editor.draw(_scene, _assets, _config);
                _editor.endFrame();
            }
            
            _window.swapBuffers();
        }
    }

    void Application::setClearColor(float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
    }

    #pragma region EventCallbacks

    void Application::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Input::onKey(key, action);

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        {
            _editorActive = !_editorActive;
            Input::setMouseTrapped(!_editorActive);
			if (_game)
			{
				_game->setEditorMode(_editorActive, _scene);
                _game->setEditorSelectionLock(_editorActive && _editor.hasSelectedObject(), _scene);
			}
        }

        if (key == GLFW_KEY_Z && action == GLFW_PRESS)
        {
            // Hide fullscreen quad geometry
            _renderer.enablePostProcessing(false);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        if (key == GLFW_KEY_Z && action == GLFW_RELEASE)
        {
            _renderer.enablePostProcessing(true);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if (key == GLFW_KEY_F4 && action == GLFW_PRESS)
        {
            _renderer.enablePostProcessing(!_renderer.isPostProcessingEnabled());
        }
    }

    void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        Input::onMouseButton(button, action);
    }

    void Application::resizeCallback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
        _config.width = width;
        _config.height = height;

        _renderer.resize(width, height);
        for (auto* camera : _scene.getCameras())
            camera->setAspect(static_cast<float>(width) / static_cast<float>(height));
    }

    void Application::mouseCallback(GLFWwindow* window, double xPos, double yPos)
    {
        Input::onMouseMove(xPos, yPos);
    }

    void Application::scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
    {
        // wip
        // Input::onScroll(xOffset, yOffset);
    }

    #pragma endregion
}