#include "MyGame.h"
#include "core/Application.h"

int main() {
    engine::AppConfig config;
    config.title = "My Game";
    config.width = 640;
    config.height = 480;
    config.renderingPath = engine::RenderingPath::Deferred;

    engine::Application app(config);
    // Blue background (132, 217, 224)
    app.setClearColor(132.0f / 255.0f, 217.0f / 255.0f, 224.0f / 255.0f, 1.0f);
    app.run(std::make_unique<MyGame>());
    return 0;
}