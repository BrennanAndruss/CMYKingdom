#pragma once

struct GLFWwindow;

namespace engine
{
    class Scene;
    class AssetManager;
    struct AppConfig;
    class Object;
}

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include "../game/MyGame.h"

namespace engine
{
    class Editor
    {
    public:
        Editor() = default;
        ~Editor() = default;

        void initialize(GLFWwindow* windowHandle);
        void shutdown();

        void beginFrame();
        void draw(Scene& scene, AssetManager& assets, const AppConfig& config);
        void endFrame();
       static bool writeObjectsToFile(const std::vector<std::unique_ptr<Object>>& objects,
                    const std::string& filename,
                    std::size_t startIndex,
                    const AssetManager& assets);
        static bool readObjectsFromFile(const std::string& filename, Scene& scene, AssetManager& assets);
        bool hasSelectedObject() const { return _selectedObject != nullptr; }
        static void setCurrentSceneName(const std::string& name);      

    private:
        bool _initialized = false;
        bool _capturedInitialSceneState = false;
        static std::size_t _initialObjectCount;
        bool _toolActive = true;
        float _toolColor[4] = { 0.20f, 0.55f, 0.90f, 1.0f };
		Object* _selectedObject = nullptr;
        static std::string currentSceneName;
        bool _sceneNameUnlocked = false;
        MyGame* _currentGame;
    };
}
