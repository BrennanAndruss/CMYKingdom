#include "MyGame.h"
#include "core/Application.h"

#if defined(_WIN32) || defined(_WIN64)
// Force high-performance GPU usage on Windows laptops with switchable graphics
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main() {
    engine::AppConfig config;
    config.title = "My Game";
    config.width = 640;
    config.height = 480;
    config.fullscreen = true;
    config.renderingPath = engine::RenderingPath::Deferred;
    config.defaultScene = "scene_objects";

    engine::Application app(config);
    // Blue background (132, 217, 224)
    //app.setClearColor(132.0f / 255.0f, 217.0f / 255.0f, 224.0f / 255.0f, 1.0f);
    app.run(std::make_unique<MyGame>());
    return 0;
}