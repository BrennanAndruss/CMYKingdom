#include "ui/GameUI.h"

#include <algorithm>
#include <cstdint>

void GameUI::loadAssets(engine::AssetManager& assets)
{
    _cyanGemIconTex = assets.loadTexture("uiCyanGemIcon", "textures/ui/cyan.png", true);
    _magentaGemIconTex = assets.loadTexture("uiMagentaGemIcon", "textures/ui/magenta.png", true);
    _yellowGemIconTex = assets.loadTexture("uiYellowGemIcon", "textures/ui/yellow.png", true);

    if (auto* tex = assets.getTexture(_cyanGemIconTex))
    {
        _cyanGemIconId = tex->getID();
    }

    if (auto* tex = assets.getTexture(_magentaGemIconTex))
    {
        _magentaGemIconId = tex->getID();
    }

    if (auto* tex = assets.getTexture(_yellowGemIconTex))
    {
        _yellowGemIconId = tex->getID();
    }
}

GameUIAction GameUI::draw(
    GameUIState state,
    int cyanGemCount,
    int magentaGemCount,
    int yellowGemCount,
    const PowerUpPopup& powerUpPopup
)

{
    switch (state)
    {
    case GameUIState::Start:
        return drawStartScreen();

    case GameUIState::Playing:
    drawPowerUpPopup(powerUpPopup);
    drawHUD(cyanGemCount, magentaGemCount, yellowGemCount);
    return GameUIAction::None;

    case GameUIState::End:
        return drawEndScreen();
    }

    return GameUIAction::None;
}

void GameUI::drawHUD(int cyanGemCount, int magentaGemCount, int yellowGemCount)
{

    float cyanProgress = std::min(cyanGemCount / static_cast<float>(maxGems), 1.0f);
    float magentaProgress = std::min(magentaGemCount / static_cast<float>(maxGems), 1.0f);
    float yellowProgress = std::min(yellowGemCount / static_cast<float>(maxGems), 1.0f);

    ImGuiIO& io = ImGui::GetIO();

    const float hudHeight = 165.0f;
    const float barWidth = 300.0f;
    const float gap = 100.0f;
    const float totalWidth = barWidth * 3.0f + gap * 2.0f;
    const float startX = (io.DisplaySize.x - totalWidth) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - hudHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, hudHeight));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("GemHUD", nullptr, flags);

    ImGui::SetCursorPos(ImVec2(startX, 12.0f));

    drawGemBar("CYAN", cyanGemCount, maxGems, cyanProgress,
        ImVec4(0.0f, 0.9f, 1.0f, 1.0f), _cyanGemIconId);

    ImGui::SameLine(0.0f, gap);

    drawGemBar("MAGENTA", magentaGemCount, maxGems, magentaProgress,
        ImVec4(1.0f, 0.0f, 0.8f, 1.0f), _magentaGemIconId);

    ImGui::SameLine(0.0f, gap);

    drawGemBar("YELLOW", yellowGemCount, maxGems, yellowProgress,
        ImVec4(1.0f, 0.85f, 0.0f, 1.0f), _yellowGemIconId);

    ImGui::End();
}

void GameUI::drawGemBar(
    const char* label,
    int current,
    int max,
    float progress,
    const ImVec4& color,
    GLuint iconTextureId
)
{
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    ImGui::BeginChild(label, ImVec2(300.0f, 82.0f), true);

    if (iconTextureId != 0)
    {
        ImGui::Image((ImTextureID)(uintptr_t)iconTextureId, ImVec2(56.0f, 56.0f));
    }

    ImGui::SameLine();

    ImGui::BeginGroup();

    ImGui::TextColored(color, "%s", label);

    ImGui::SameLine();
    ImGui::Text("%d / %d", current, max);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.08f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.08f, 0.08f, 0.08f, 0.85f));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    ImGui::ProgressBar(progress, ImVec2(210.0f, 22.0f), "");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::EndGroup();
    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::EndGroup();
}

GameUIAction GameUI::drawStartScreen()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    GameUIAction action = GameUIAction::None;

    ImGui::Begin("StartScreen", nullptr, flags);

    float centerX = io.DisplaySize.x * 0.5f;
    const float verticalOffset = 0;
    const float titleScale = 10.0f;
    const float descriptionScale = 2.0f;
    const float buttonScale = 1.5f;

    //----------------------------------------
    // Title
    //----------------------------------------

    ImGui::SetCursorPosY(io.DisplaySize.y * 0.18f + verticalOffset);

    ImGui::SetWindowFontScale(titleScale);

    ImVec2 titleSize = ImGui::CalcTextSize("CMY Kingdom");
    ImGui::SetCursorPosX((io.DisplaySize.x - titleSize.x) * 0.5f);

    const ImVec4 cyanColor    = ImVec4(0.0f, 0.9f, 1.0f, 1.0f);
    const ImVec4 magentaColor = ImVec4(1.0f, 0.0f, 0.8f, 1.0f);
    const ImVec4 yellowColor  = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);

    ImGui::TextColored(cyanColor, "C ");
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::TextColored(magentaColor, "M ");
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::TextColored(yellowColor, "Y");
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::Text(" Kingdom");

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    //----------------------------------------
    // Description Box
    //----------------------------------------

    const float boxWidth = 800.0f;
    const float boxHeight = 300.0f;

    ImGui::SetCursorPosX(centerX - boxWidth * 0.5f);

    ImGui::BeginChild(
        "DescriptionBox",
        ImVec2(boxWidth, boxHeight),
        false
    );

    ImGui::SetWindowFontScale(descriptionScale);

    const float textRegionWidth = 720.0f;

    ImGui::SetCursorPosX((boxWidth - textRegionWidth) * 0.5f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textRegionWidth);

    ImGui::Text(
        "Welcome to CMY Kingdom!\n\n"
        "The kingdom's magical color crystals have been scattered "
        "throughout the land. Explore the world, collect Cyan, "
        "Magenta, and Yellow gems, and restore color to the kingdom!"
    );

    ImGui::PopTextWrapPos();

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::EndChild();

    //----------------------------------------
    // Start Button
    //----------------------------------------

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    

    const float buttonWidth = 220.0f;
    const float buttonHeight = 60.0f;

    ImGui::SetCursorPosX(centerX - buttonWidth * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::Button("Start Game", ImVec2(buttonWidth, buttonHeight)))
    {
        action = GameUIAction::Start;
    }

    ImGui::PopStyleColor(4);

    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();

    return action;
}

GameUIAction GameUI::drawEndScreen()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    GameUIAction action = GameUIAction::None;

    ImGui::Begin("EndScreen", nullptr, flags);

    float centerX = io.DisplaySize.x * 0.5f;
    const float verticalOffset = 0;
    const float titleScale = 10.0f;
    const float descriptionScale = 2.0f;
    const float buttonScale = 1.5f;

    //----------------------------------------
    // Title
    //----------------------------------------

    ImGui::SetCursorPosY(io.DisplaySize.y * 0.18f + verticalOffset);

    ImGui::SetWindowFontScale(titleScale);

    ImVec2 titleSize = ImGui::CalcTextSize("Mission Complete!");
    ImGui::SetCursorPosX((io.DisplaySize.x - titleSize.x) * 0.5f);

    ImGui::Text("Mission Complete!");

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    //----------------------------------------
    // Description Box
    //----------------------------------------

    const float boxWidth = 800.0f;
    const float boxHeight = 300.0f;

    ImGui::SetCursorPosX(centerX - boxWidth * 0.5f);

    ImGui::BeginChild(
        "DescriptionBox",
        ImVec2(boxWidth, boxHeight),
        false
    );

    ImGui::SetWindowFontScale(descriptionScale);

    const float textRegionWidth = 720.0f;

    ImGui::SetCursorPosX((boxWidth - textRegionWidth) * 0.5f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textRegionWidth);

    ImGui::Text(
        "Congratulations!\n\n"
        "You have collected all the gems and restored color to the kingdom! "
        "Go forth and explore the new vibrant world. "
    );

    ImGui::PopTextWrapPos();

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::EndChild();

    //----------------------------------------
    // Continue Button
    //----------------------------------------

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    

    const float buttonWidth = 220.0f;
    const float buttonHeight = 60.0f;

    ImGui::SetCursorPosX(centerX - buttonWidth * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::Button("Continue", ImVec2(buttonWidth, buttonHeight)))
    {
        action = GameUIAction::Continue;

    }

    ImGui::PopStyleColor(4);

    //----------------------------------------
    // Restart Button
    //----------------------------------------

    ImGui::SetWindowFontScale(buttonScale);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(centerX - buttonWidth * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::Button("Restart", ImVec2(buttonWidth, buttonHeight)))
    {
        action = GameUIAction::Restart;

    }

    ImGui::PopStyleColor(4);

    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();

    return action;
}

void GameUI::drawPowerUpPopup(const PowerUpPopup& popup)
{
    if (!popup.active || popup.remainingTime <= 0.0f)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    const float width = 420.0f;
    const float height = 70.0f;

    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - width) * 0.5f, 30.0f)
    );
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.02f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);

    ImGui::Begin("PowerUpPopup", nullptr, flags);

    ImGui::SetWindowFontScale(1.4f);

    char text[128];
    snprintf(
        text,
        sizeof(text),
        "%s collected! Time: %.1f",
        popup.label,
        popup.remainingTime
    );

    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImGui::SetCursorPosX((width - textSize.x) * 0.5f);
    ImGui::SetCursorPosY((height - textSize.y) * 0.5f);

    ImGui::Text("%s", text);

    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}