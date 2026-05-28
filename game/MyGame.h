#pragma once

#include <vector>

#include "core/Game.h"
#include "core/EventCallbacks.h"
#include "resources/AssetManager.h"
#include "renderer/Renderer.h"
#include "scene/Scene.h"
#include "scene/Object.h"

#include "passes/ColorRestorationPass.h"
#include "systems/FreeCameraController.h"
#include "systems/PlayerController.h"

class MyGame : public engine::Game, public engine::EventCallbacks
{
public:
	MyGame() = default;
	~MyGame() = default;

	static MyGame* getActiveGame();

	void init(engine::AssetManager& assets, 
			  engine::Renderer& renderer, 
			  engine::Scene& scene, 
			  const engine::AppConfig& config) override;
	void update(float deltaTime) override;
	void setEditorMode(bool editorActive, engine::Scene& scene) override;
	void setEditorSelectionLock(bool locked, engine::Scene& scene) override;
	void onCollectableCollected();
	void onCollectableCollected(int type);
	bool terrainSkyLightingEnabled = false;

private:
	engine::Object* gem = nullptr;
	engine::Object* cube = nullptr;
	engine::Object* gameplayCameraObject = nullptr;
	engine::Object* editorCameraObject = nullptr;
	engine::Object* pointLightCenter = nullptr;
	Handle<engine::Mesh> cubeMesh;
	Handle<engine::Mesh> platformMesh;
	Handle<engine::Material> platformMaterial;
	PlayerController* gameplayController = nullptr;
	FreeCameraController* editorController = nullptr;
	bool editorModeActive = false;
	bool editorCameraLocked = false;
	std::vector<engine::Object*> objects;

	ColorRestorationPass* _colorRestorePass = nullptr;
	float _collectedCyan = 0.0f, _collectedMagenta = 0.0f, _collectedYellow = 0.0f;
	float _teleportCooldown = 0.0f;
	static MyGame* _activeGame;

	void refreshEditorCameraState(engine::Scene& scene);
};