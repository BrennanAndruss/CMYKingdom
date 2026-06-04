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

class GameUI
{
public:
    void loadAssets(engine::AssetManager& assets);

    GameUIAction draw(
        GameUIState state,
        int cyanGemCount,
        int magentaGemCount,
        int yellowGemCount
    );
    const int maxGems = 1;

private:
    void drawHUD(int cyanGemCount, int magentaGemCount, int yellowGemCount);
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