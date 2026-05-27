#include "core/Window.h"

#include <iostream>
#include <stdexcept>
#include <cassert>
#include "core/EventCallbacks.h"

namespace engine
{
	static void errorCallback(int error, const char* description)
	{
		std::cerr << description << std::endl;
	}

	Window::Window(int width, int height, const std::string& title) : _callbacks(nullptr)
	{
		glfwSetErrorCallback(errorCallback);

		// Initialize GLFW library
		if (!glfwInit())
		{
			throw std::runtime_error("Failed to initialize GLFW");
		}

		// Request the highest possible version of OpenGL
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

		// Create a windowed mode window and its OpenGL context
		_handle = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
		if (!_handle)
		{
			glfwTerminate();
			throw std::runtime_error("Failed to create window");
		}

		glfwMakeContextCurrent(_handle);

		// Initialize GLAD
		if (!gladLoadGL())
		{
			glfwTerminate();
			throw std::runtime_error("Failed to initialize GLAD");
		}

		std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
		std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

		// Set vsync
		glfwSwapInterval(0);

		// Store this window instance so static callbacks can retrieve it
		glfwSetWindowUserPointer(_handle, this);

		// Register callbacks with GLFW
		glfwSetKeyCallback(_handle, keyCallback);
		glfwSetMouseButtonCallback(_handle, mouseButtonCallback);
		glfwSetFramebufferSizeCallback(_handle, resizeCallback);
		glfwSetCursorPosCallback(_handle, mouseCallback);
		glfwSetScrollCallback(_handle, scrollCallback);
	}

	Window::~Window()
	{
		if (_handle)
		{
			glfwDestroyWindow(_handle);
			_handle = nullptr;
		}
		glfwTerminate();
	}

	void Window::setEventCallbacks(EventCallbacks* callbacks)
	{
		_callbacks = callbacks;
	}

	bool Window::shouldClose()
	{
		return glfwWindowShouldClose(_handle);
	}

	void Window::swapBuffers()
	{
		glfwSwapBuffers(_handle);
	}

	void Window::pollEvents()
	{
		glfwPollEvents();
	}

	static Window* getWindow(GLFWwindow* w)
	{
		auto* win = static_cast<Window*>(glfwGetWindowUserPointer(w));
		assert(win != nullptr && "Window user pointer is null");
		return win;
	}

	void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto* win = getWindow(window);
		if (win->_callbacks)
		{
			win->_callbacks->keyCallback(window, key, scancode, action, mods);
		}
	}

	void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		auto* win = getWindow(window);
		if (win->_callbacks)
		{
			win->_callbacks->mouseButtonCallback(window, button, action, mods);
		}
	}

	void Window::resizeCallback(GLFWwindow* window, int width, int height)
	{
		auto* win = getWindow(window);
		if (win->_callbacks)
		{
			win->_callbacks->resizeCallback(window, width, height);
		}
	}

	void Window::mouseCallback(GLFWwindow* window, double xPos, double yPos)
	{
		auto* win = getWindow(window);
		if (win->_callbacks)
		{
			win->_callbacks->mouseCallback(window, xPos, yPos);
		}
	}

	void Window::scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
	{
		auto* win = getWindow(window);
		if (win->_callbacks)
		{
			win->_callbacks->scrollCallback(window, xOffset, yOffset);
		}
	}
}