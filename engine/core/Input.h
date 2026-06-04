#pragma once

#include <glm/glm.hpp>
#include <unordered_map>

namespace engine
{
	class Input
	{
	public:
		// Polling functions
		static bool isKeyDown(int key);
		static bool isKeyPressed(int key);
		static bool isKeyReleased(int key);
		static bool isMouseDown(int button);
		static bool isMouseTrapped() { return _mouseTrapped; }
		static glm::vec2 getMousePos();
		static glm::vec2 getMouseDelta();
		static glm::vec2 getScrollDelta();

		// Application functions
		static void update();
		static void onKey(int key, int action);
		static void onMouseMove(double x, double y);
		static void onMouseButton(int button, int action);
		static void onScroll(double x, double y);
		static void setMouseTrapped(bool trapped);
		static void flushMouseDelta();


	private:
		static inline std::unordered_map<int, bool> _keys;
		static inline std::unordered_map<int, bool> _prevKeys;
		static inline std::unordered_map<int, bool> _mouseButtons;
		static inline glm::vec2 _mousePos = glm::vec2(0.0f);
		static inline glm::vec2 _prevMousePos = glm::vec2(0.0f);
		static inline glm::vec2 _mouseDelta = glm::vec2(0.0f);
		static inline glm::vec2 _scroll = glm::vec2(0.0f);
		static inline glm::vec2 _prevScroll = glm::vec2(0.0f);
		static inline glm::vec2 _scrollDelta = glm::vec2(0.0f);
		static inline bool _mouseTrapped = false;
	};
}