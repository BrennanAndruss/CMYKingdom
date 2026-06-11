#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include "resources/AssetManager.h"
#include "resources/Handle.h"
#include "renderer/resources/Texture.h"

enum class GameUIState
{
    Start,
    Playing,
    End
};


enum class GameUIAction
{
    None,
    Start,
    Continue,
    Restart
};

struct PowerUpPopup
{
    bool active = false;
    const char* label = "";
    float remainingTime = 0.0f;
};

class GameUI
{
public:
    void loadAssets(engine::AssetManager& assets);

    GameUIAction draw(
    GameUIState state,
    int cyanGemCount,
    int magentaGemCount,
    int yellowGemCount,
    const PowerUpPopup& powerUpPopup
    );

    const int maxGems = 1;

private:
    void drawHUD(int cyanGemCount, int magentaGemCount, int yellowGemCount);
    void drawPowerUpPopup(const PowerUpPopup& popup);
    GameUIAction drawStartScreen();
    GameUIAction drawEndScreen();

    void drawGemBar(
        const char* label,
        int current,
        int max,
        float progress,
        const ImVec4& color,
        GLuint iconTextureId
    );

private:
    Handle<engine::Texture> _cyanGemIconTex;
    Handle<engine::Texture> _magentaGemIconTex;
    Handle<engine::Texture> _yellowGemIconTex;

    GLuint _cyanGemIconId = 0;
    GLuint _magentaGemIconId = 0;
    GLuint _yellowGemIconId = 0;
};