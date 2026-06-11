#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

// Forward declarations
namespace engine
{
	class EventCallbacks;
}

namespace engine
{
	class Window
	{
	public:
		Window(int width, int height, const std::string& title);
		~Window();

		void setEventCallbacks(EventCallbacks* callbacks);
		GLFWwindow* getHandle() { return _handle; }

		bool shouldClose();
		void swapBuffers();
		void pollEvents();

		void setFullscreen();

	private:
		GLFWwindow* _handle;
		EventCallbacks* _callbacks;

		// Static forwarders with plain function pointers to register as callbacks with GLFW
		// These functions retrieve the Window instance via user pointer and forward associated callbacks
		static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		static void resizeCallback(GLFWwindow* window, int width, int height);
		static void mouseCallback(GLFWwindow* window, double xPos, double yPos);
		static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
	};
}
