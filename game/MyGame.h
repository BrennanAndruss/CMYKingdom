#pragma once

#include <string>
#include <vector>

#include "core/Game.h"
#include "core/EventCallbacks.h"
#include "resources/AssetManager.h"
#include "renderer/Renderer.h"
#include "scene/Scene.h"
#include "scene/components/Audio.h"
#include "scene/Object.h"
//#include <imgui.h>
#include "ui/GameUI.h"
#include <glad/glad.h>

#include "passes/ColorRestorationPass.h"
#include "systems/Collectable.h"
#include "systems/FreeCameraController.h"
#include "systems/PlayerController.h"

namespace engine
{
	class GrassRenderer;
}

class MyGame : public engine::Game, public engine::EventCallbacks
{
public:
	MyGame() = default;
	~MyGame() = default;

	static MyGame* getActiveGame();
	void setBackgroundMusicPath(const std::string& path);

	void init(engine::AssetManager& assets, 
			  engine::Renderer& renderer, 
			  engine::Scene& scene, 
			  engine::AudioEngine& audio,
			  const engine::AppConfig& config) override;
	void update(float deltaTime) override;
	void setEditorMode(bool editorActive, engine::Scene& scene) override;
	void setEditorSelectionLock(bool locked, engine::Scene& scene) override;
	void onCollectableCollected();
	void onCollectableCollected(Collectable::Type type, glm::vec3 worldPos);
	void drawUI() override;
	bool terrainSkyLightingEnabled = true;
	bool _startRequested = false;
	bool _endScreenShown = false;
	bool allGemsCollected() const;
	void continueGame();
	void restartGame();
	void resetGameProgress();
	

private:
	engine::Object* gem = nullptr;
	engine::Object* cube = nullptr;
	engine::Object* gameplayCameraObject = nullptr;
	engine::Object* editorCameraObject = nullptr;
	engine::Object* pointLightCenter = nullptr;
	engine::GrassRenderer* grassRenderer = nullptr;

	Handle<engine::Mesh> cubeMesh;
	Handle<engine::Mesh> platformMesh;
	Handle<engine::Material> platformMaterial;

	PlayerController* gameplayController = nullptr;
	FreeCameraController* editorController = nullptr;
	GameUI _gameUI;
	GameUIState _gameUIState = GameUIState::Start;

	bool editorModeActive = false;
	bool editorCameraLocked = false;
	std::vector<engine::Object*> objects;

	// Color restoration and pulse tracking
	std::vector<PulseData> _activePulses;
	ColorRestorationPass* _colorRestorePass = nullptr;

	float _collectedCyan = 0.0f, _collectedMagenta = 0.0f, _collectedYellow = 0.0f;
	int _cyanGemCount = 0, _magentaGemCount = 0, _yellowGemCount = 0;
	float _colorIncrement;

	engine::AudioEngine* _audio = nullptr;
	// Set these to the audio files you want to use.
	// Background music loops until the game closes.
	std::string backgroundMusicPath = PROJECT_ROOT"assets/sounds/background.mp3";
	// These are used by the player controller for locomotion and jump SFX.
	std::string runningSoundPath = PROJECT_ROOT "assets/sounds/walkaudio.mp3";
	std::string runningFastSoundPath = PROJECT_ROOT "assets/sounds/runaudio.mp3";
	std::string jumpingSoundPath = PROJECT_ROOT "assets/sounds/jumpaudio.mp3";
	
	float _teleportCooldown = 0.0f;
	static MyGame* _activeGame;

	void refreshEditorCameraState(engine::Scene& scene);
	void startGame();
};