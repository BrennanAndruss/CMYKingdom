#include "MyGame.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "core/Input.h"
#include "resources/AssetManager.h"
#include "resources/Heightmap.h"
#include "renderer/resources/Shader.h"
#include "renderer/resources/Mesh.h"
#include "renderer/resources/Material.h"
#include "renderer/resources/Texture.h"
#include "renderer/resources/Cubemap.h"
#include "scene/components/Components.h"
#include "systems/PlayerController.h"
#include "systems/Collectable.h"
#include "ui/GameUI.h"
#include <imgui.h>
#include <stdio.h>
#include "editor/Editor.h"

namespace
{
    PowerUpPopupState g_powerUpPopupsInstance;
}

#define _powerUpPopups g_powerUpPopupsInstance

MyGame* MyGame::_activeGame = nullptr;

MyGame* MyGame::getActiveGame()
{
	return _activeGame;
}

namespace
{
	Handle<engine::AudioClip> loadOptionalAudioClip(engine::AssetManager& assets,
		const std::string& name,
		const std::string& path)
	{
		try
		{
			return assets.loadAudioClip(name, path);
		}
		catch (const std::exception& e)
		{
			std::cerr << "Optional audio clip not loaded: " << e.what() << "\n";
			return {};
		}
	}
}

void MyGame::onPowerUpCollected(Collectable::Type type, float duration)
{
    if (type == Collectable::Type::speedBoost)
    {
        _powerUpPopups.speedBoost.active = true;
        _powerUpPopups.speedBoost.label = "Speed boost";
        _powerUpPopups.speedBoost.remainingTime = duration;
    }
    else if (type == Collectable::Type::JumpBoost)
    {
        _powerUpPopups.jumpBoost.active = true;
        _powerUpPopups.jumpBoost.label = "Jump boost";
        _powerUpPopups.jumpBoost.remainingTime = duration;
    }
}

void MyGame::onCollectableCollected()
{
	_collectedCyan = std::min(_collectedCyan + 0.2f, 1.0f);
	_collectedMagenta = std::min(_collectedMagenta + 0.2f, 1.0f);
	_collectedYellow = std::min(_collectedYellow + 0.2f, 1.0f);
}

bool MyGame::allGemsCollected() const
{
    return _cyanGemCount >= _gameUI.maxGems &&
           _magentaGemCount >= _gameUI.maxGems &&
           _yellowGemCount >= _gameUI.maxGems;
}

void MyGame::onCollectableCollected(Collectable::Type type, glm::vec3 worldPos)
{
	// Increment the gem count of the corresponding collectable type
	if (type == Collectable::Type::Cyan)
		_cyanGemCount++;
	else if (type == Collectable::Type::Magenta)
		_magentaGemCount++;
	else if (type == Collectable::Type::Yellow)
		_yellowGemCount++;

	if (_audio && _assets && gemCollectSoundClip.valid())
	{
		if (auto* clip = _assets->getAudioClip(gemCollectSoundClip))
		{
			_audio->playOneShot(*clip);
		}
	}

	// Add color pulse event when there is room; audio should still play if this visual queue is full.
	if (_activePulses.size() < ColorRestorationPass::MAX_PULSES)
	{
		_activePulses.emplace_back(PulseData{ worldPos, 0.0f, type });
	}
}

void MyGame::startGame()
{
    _gameUIState = GameUIState::Playing;
    editorModeActive = false;
    editorCameraLocked = false;

    if (gameplayController) gameplayController->enabled = true;
    if (editorController)   editorController->enabled = false;

    if (gameplayCameraObject)
    {
        auto* camera = gameplayCameraObject->getComponent<engine::Camera>();
        if (camera) gameplayCameraObject->getScene()->setMainCamera(camera);
    }

    engine::Input::setMouseTrapped(true);

    // Explicitly flush any warp delta GLFW fires when cursor mode changes
    engine::Input::flushMouseDelta();   // see below

	engine::Editor::setCurrentSceneName(_config.defaultScene);

	engine::Editor::readObjectsFromFile(
		_config.assetsDir + "scenes/" + _config.defaultScene + ".txt",
		*_scene,
		*_assets);
	
	
}

void MyGame::continueGame()
{
    _gameUIState = GameUIState::Playing;

    if (gameplayController)
    {
        gameplayController->enabled = true;
    }

    if (editorController)
    {
        editorController->enabled = false;
    }

    engine::Input::setMouseTrapped(true);
}

void MyGame::restartGame()
{
    resetGameProgress();
    resetPlayerToStart();

    _endScreenShown = false;
    _gameUIState = GameUIState::Playing;

    if (gameplayController)
    {
        gameplayController->enabled = true;
    }

    if (editorController)
    {
        editorController->enabled = false;
    }

    engine::Input::setMouseTrapped(true);
    engine::Input::flushMouseDelta();
}

void MyGame::resetPlayerToStart()
{
    if (!player)
    {
        return;
    }

    if (auto* controller = player->getComponent<engine::CharacterController>())
    {
        controller->teleport(_playerStartPosition);
    }
    else
    {
        player->transform.setPosition(_playerStartPosition);
    }

    if (gameplayController)
    {
        gameplayController->resetGameplayState();
    }

    if (gameplayCameraObject)
    {
        const glm::vec3 focus = player->transform.getPosition() + glm::vec3(0.0f, gameplayController ? gameplayController->eyeHeight : 0.3f, 0.0f);
        gameplayCameraObject->transform.setPosition(focus + glm::vec3(0.0f, 0.0f, -4.0f));
        gameplayCameraObject->transform.lookAt(focus);
    }

    _teleportCooldown = 0.0f;
}

void MyGame::resetGameProgress()
{
	_powerUpPopups.speedBoost = PowerUpPopup{};
	_powerUpPopups.jumpBoost = PowerUpPopup{};
	
    _cyanGemCount = 0;
    _magentaGemCount = 0;
    _yellowGemCount = 0;

    _collectedCyan = 0.0f;
    _collectedMagenta = 0.0f;
    _collectedYellow = 0.0f;

    _activePulses.clear();

    if (_colorRestorePass)
    {
        _colorRestorePass->cyan = 0.0f;
        _colorRestorePass->magenta = 0.0f;
        _colorRestorePass->yellow = 0.0f;
        _colorRestorePass->setActivePulses(_activePulses);
    }
}


void MyGame::drawUI()
{
    GameUIAction action = _gameUI.draw(
    _gameUIState,
    _cyanGemCount,
    _magentaGemCount,
    _yellowGemCount,
    _powerUpPopups);

    if (_gameUIState == GameUIState::Start && action == GameUIAction::Start)
    {
        _startRequested = true;
    }
    else if (_gameUIState == GameUIState::End && action == GameUIAction::Continue)
    {
        continueGame();
    }
    else if (_gameUIState == GameUIState::End && action == GameUIAction::Restart)
    {
        restartGame();
    }
}


void MyGame::init(engine::AssetManager& assets, 
				  engine::Renderer& renderer, 
				  engine::Scene& scene,
				  engine::AudioEngine& audio,
				  const engine::AppConfig& config)
{
	_activeGame = this;
	_audio = &audio;

	_scene = &scene;
	_assets = &assets;
	_config = config;

	// Initialize resources
	std::cout << "Loading shaders...\n";
	Handle<engine::Shader> colorRestoreShader = assets.loadShader(
		"colorRestoreShader", "shaders/colorRestore.vert", "shaders/colorRestore.frag");
	Handle<engine::Shader> waterShader = assets.loadShader(
		"waterShader", "shaders/water.vert", "shaders/water.frag");
	Handle<engine::Shader> grassShader = assets.loadShader(
		// "grassShader", "shaders/grass.vert", "shaders/grass.frag");
		"grassShader", "shaders/grassGeom.vert", "shaders/grassGeom.frag");
	Handle<engine::Shader> grassShadowShader = assets.loadShader(
		"grassShadowShader", "shaders/grassDepth.vert", "shaders/grassDepth.frag");

	//loading textures
	std::cout << "Loading textures...\n";

	// Heighmap Texture
	Handle<engine::Heightmap> terrainHeightmap = assets.loadHeightmap("terrainHM", "textures/heightmaps/unityterrain04.png", 400.0f);
	auto* heightmap = assets.getHeightmap(terrainHeightmap);

	// Build a grayscale texture from the same terrain height data so the water shader
	// can query actual terrain contact instead of reacting to foreground silhouettes.
	Handle<engine::Texture> terrainHeightTex;
	if (heightmap)
	{
		const auto& srcPixels = heightmap->getPixels();
		const int hWidth = heightmap->getWidth();
		const int hLength = heightmap->getLength();
		const int hChannels = heightmap->getChannels();
		std::vector<unsigned char> heightRgba(static_cast<std::size_t>(hWidth * hLength * 4));
		for (int y = 0; y < hLength; ++y)
		{
			for (int x = 0; x < hWidth; ++x)
			{
				const std::size_t srcIdx = static_cast<std::size_t>((y * hWidth + x) * hChannels);
				const unsigned char value = srcPixels[srcIdx];
				const std::size_t dstIdx = static_cast<std::size_t>((y * hWidth + x) * 4);
				heightRgba[dstIdx + 0] = value;
				heightRgba[dstIdx + 1] = value;
				heightRgba[dstIdx + 2] = value;
				heightRgba[dstIdx + 3] = 255;
			}
		}
		terrainHeightTex = assets.loadTexture("terrainHeightTex", heightRgba.data(), hWidth, hLength);
	}
	// Splatmap Textures
	Handle<engine::Texture> terrainSplat0 =
		assets.loadTexture("terrainSplat0", "textures/splatmaps/splatmaps0.png", true);

	Handle<engine::Texture> terrainSplat1 =
		assets.loadTexture("terrainSplat1", "textures/splatmaps/splatmaps1.png", true);

	Handle<engine::Texture> terrainSplat2 =
		assets.loadTexture("terrainSplat2", "textures/splatmaps/splatmaps2.png", true);




	// Terrain Textures
	/*
		assets/textures/terrain/BeachGrass.png 
		assets/textures/terrain/BeachSang.png 
		assets/textures/terrain/BeachStone.png 
		assets/textures/terrain/DesertGrass.png 
		assets/textures/terrain/DesertSand.png 
		assets/textures/terrain/DesertStone.png 
		assets/textures/terrain/ForestFlowers.png 
		assets/textures/terrain/ForestGrass.png 
		assets/textures/terrain/ForestStone.png
		assets/textures/terrain/StylizedSnow.png
	*/
	// =====================================================
	// Terrain Textures
	// =====================================================

	// Beach
	Handle<engine::Texture> beachGrass =
		assets.loadTexture("beachGrass", "textures/terrain/BeachGrass.png", true);

	Handle<engine::Texture> beachSand =
		assets.loadTexture("beachSand", "textures/terrain/BeachSand.png", true);

	Handle<engine::Texture> beachStone =
		assets.loadTexture("beachStone", "textures/terrain/BeachStone.png", true);


	// Desert
	Handle<engine::Texture> desertGrass =
		assets.loadTexture("desertGrass", "textures/terrain/DesertGrass.png", true);

	Handle<engine::Texture> desertSand =
		assets.loadTexture("desertSand", "textures/terrain/DesertSand.png", true);

	Handle<engine::Texture> desertStone =
		assets.loadTexture("desertStone", "textures/terrain/DesertStone.png", true);


	// Forest
	Handle<engine::Texture> forestFlowers =
		assets.loadTexture("forestFlowers", "textures/terrain/ForestFlowers.png", true);

	Handle<engine::Texture> forestGrass =
		assets.loadTexture("forestGrass", "textures/terrain/ForestGrass.png", true);

	Handle<engine::Texture> forestStone =
		assets.loadTexture("forestStone", "textures/terrain/ForestStone.png", true);


	// Snow
	Handle<engine::Texture> stylizedSnow =
		assets.loadTexture("stylizedSnow", "textures/terrain/StylizedSnow.png", true);

	// Texture array slots used by terrain shader: 10 textures used right now 

	Handle<engine::Texture> terrainTex0 = beachSand;
	Handle<engine::Texture> terrainTex1 = beachGrass;
	Handle<engine::Texture> terrainTex2 = beachStone;

	Handle<engine::Texture> terrainTex3 = desertSand;
	Handle<engine::Texture> terrainTex4 = desertGrass;
	Handle<engine::Texture> terrainTex5 = desertStone;

	Handle<engine::Texture> terrainTex6 = forestGrass;
	Handle<engine::Texture> terrainTex7 = forestFlowers;
	Handle<engine::Texture> terrainTex8 = forestStone;

	Handle<engine::Texture> terrainTex9 = stylizedSnow;

	Handle<engine::Texture> terrainTex10 = forestGrass;


	// Remaining slots available for future use
	// Handle<engine::Texture> terrainTex10 = beachSand;
	// Handle<engine::Texture> terrainTex11 = beachGrass;
	// Handle<engine::Texture> terrainTex12 = desertSand;
	// Handle<engine::Texture> terrainTex13 = desertGrass;
	// Handle<engine::Texture> terrainTex14 = forestGrass;
	// Handle<engine::Texture> terrainTex15 = stylizedSnow;


	// Using Terrain Grass Texture for Grass Instances
	Handle<engine::Texture> grassBladeTex = forestGrass;
	
	Handle<engine::Cubemap> skyboxCubemap = assets.loadCubemap("daySkybox", {
		"textures/skybox/px.png",
		"textures/skybox/nx.png",
		"textures/skybox/py.png",
		"textures/skybox/ny.png",
		"textures/skybox/pz.png",
		"textures/skybox/nz.png"
	});
	scene.setSkybox(skyboxCubemap);
	Handle<engine::Cubemap> irradianceCubemap = assets.loadCubemap("dayIrradiance", {
		"textures/irradiance/ipx.png",
		"textures/irradiance/inx.png",
		"textures/irradiance/ipy.png",
		"textures/irradiance/iny.png",
		"textures/irradiance/ipz.png",
		"textures/irradiance/inz.png"
	});
	scene.setIrradianceMap(irradianceCubemap);

	backgroundMusicClip = assets.loadAudioClip("backgroundMusic", "sounds/background.mp3");
	runSoundClip = assets.loadAudioClip("playerWalkSound", "sounds/walkaudio.mp3");
	runFastSoundClip = assets.loadAudioClip("playerRunSound", "sounds/runaudio.mp3");
	jumpSoundClip = assets.loadAudioClip("playerJumpSound", "sounds/jumpaudio.mp3");
	landingSoundClip = assets.loadAudioClip("playerLandSound", "sounds/landaudio.mp3");
	gemCollectSoundClip = loadOptionalAudioClip(assets, "gemCollectSound", "sounds/gemcollect.mp3");
	allGemsCollectedSoundClip = loadOptionalAudioClip(assets, "allGemsCollectedSound", "sounds/allgemscollected.mp3");

	if (_audio)
	{
		if (auto* clip = assets.getAudioClip(backgroundMusicClip))
		{
			_audio->playMusic(*clip, true);
		}
	}	//load in game UI
	_gameUI.loadAssets(assets);

	Handle<engine::Texture> defaultGrayTex = assets.createSolidTexture("defaultGrayTex", { 128, 128, 128, 255 });
	Handle<engine::Texture> charBaseTex = assets.loadTexture("charBaseTex", "textures/char_Base_color.png", true);

	// Procedural noise texture used by the water shader for stylized distortion/foam breakup.
	{
		const int noiseSize = 64;
		std::vector<unsigned char> noiseData(static_cast<std::size_t>(noiseSize * noiseSize * 4));
		for (int y = 0; y < noiseSize; ++y)
		{
			for (int x = 0; x < noiseSize; ++x)
			{
				const std::size_t idx = static_cast<std::size_t>((y * noiseSize + x) * 4);
				unsigned int n = static_cast<unsigned int>(x * 1973 + y * 9277 + x * y * 26699 + 911);
				n ^= (n << 13);
				n ^= (n >> 17);
				n ^= (n << 5);
				unsigned char value = static_cast<unsigned char>((n & 0xFFu));
				noiseData[idx + 0] = value;
				noiseData[idx + 1] = static_cast<unsigned char>((value * 167u) & 0xFFu);
				noiseData[idx + 2] = static_cast<unsigned char>((value * 37u) & 0xFFu);
				noiseData[idx + 3] = 255;
			}
		}
		assets.loadTexture("waterNoiseTex", noiseData.data(), noiseSize, noiseSize);
	}
	
	// Load CMYK platform texture
	Handle<engine::Texture> platformCMYKTex = assets.loadTexture("platformCMYKTex", "textures/heightmaps/cmyk_platform_openPBR_shader1_BaseMap.png", true);
	
	// Create CMYK platform material
	platformMaterial = assets.loadMaterial("platformCMYKMaterial");
	if (auto* mat = assets.getMaterial(platformMaterial))
	{
		mat->difTex = platformCMYKTex;
	}

	std::cout << "Loading models...\n";
	Handle<engine::Mesh> gemMesh;
	try
	{
		gemMesh = assets.loadMeshAssimp("gem", "models/gem_model.fbx");

		Handle<engine::Texture> cyanGemTex =
			assets.loadTexture("cyanGemTex", "textures/cyan_gem_texture.png", true);

		Handle<engine::Texture> magentaGemTex =
			assets.loadTexture("magentaGemTex", "textures/magenta_gem_texture.png", true);

		Handle<engine::Texture> yellowGemTex =
			assets.loadTexture("yellowGemTex", "textures/yellow_gem_texture.png", true);
				
		Handle<engine::Material> cyanGemMat = assets.loadMaterial("cyanGemMat");
		if (auto* mat = assets.getMaterial(cyanGemMat))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(1.0f);
			mat->shininess = 64.0f;
			mat->difTex = cyanGemTex;
			mat->specTex = cyanGemTex;
		}

		Handle<engine::Material> magentaGemMat = assets.loadMaterial("magentaGemMat");
		if (auto* mat = assets.getMaterial(magentaGemMat))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(1.0f);
			mat->shininess = 64.0f;
			mat->difTex = magentaGemTex;
			mat->specTex = magentaGemTex;
		}

		Handle<engine::Material> yellowGemMat = assets.loadMaterial("yellowGemMat");
		if (auto* mat = assets.getMaterial(yellowGemMat))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(1.0f);
			mat->shininess = 64.0f;
			mat->difTex = yellowGemTex;
			mat->specTex = yellowGemTex;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Assimp load failed, falling back to OBJ: " << e.what() << "\n";
		gemMesh = assets.loadMesh("gem", "models/cube.obj");
	}

	cubeMesh = assets.loadMesh("cube", "models/cube.obj");
	//load in a powerup meshes
	assets.loadMeshAssimp("speedBoost", "models/speedBoost.fbx");
	assets.loadMeshAssimp("JumpBoost", "models/jumpBoost.fbx");

	platformMesh = assets.loadMeshAssimp("square-platform", "models/square-platform.fbx");

	// Load tree models from assets/models/trees
	Handle<engine::Mesh> tree_1;
	Handle<engine::Mesh> tree_2;
	std::vector<Handle<engine::Mesh>> tree_3Meshes;
	try {
		Handle<engine::Texture> tree1_texture = assets.loadTexture("tree1_diffuse", "textures/trees/tree-maple-D.jpeg", true);
		tree_1 = assets.loadMeshAssimp("tree1", "models/trees/tree-maple-low-poly-Anim.fbx");
		if (auto* mat = assets.getMaterial(assets.loadMaterial("tree1Mat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = tree1_texture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for tree1\n";
		}
		

	} catch (const std::exception& e) {
		std::cerr << "Failed to load tree_1, falling back to cube: " << e.what() << "\n";
		tree_1 = cubeMesh;
		
	}

	// tree 2
	try {
		Handle<engine::Texture> tree2_texture = assets.loadTexture("tree2_diffuse", "textures/trees/PP_Texture_256.png", true);
		tree_2 = assets.loadMeshAssimp("tree2", "models/trees/PP_Tree_winding_01.fbx");
		if (auto* mat = assets.getMaterial(assets.loadMaterial("tree2Mat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = tree2_texture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for tree2\n";
		}
		

	} catch (const std::exception& e) {
		std::cerr << "Failed to load tree_2, falling back to cube: " << e.what() << "\n";
		tree_2 = cubeMesh;
		
	}

	// tree 3
	try
	{
		Handle<engine::Texture> tree3_texture = assets.loadTexture("tree3_diffuse", "textures/trees/Evergreen_Geometry_0801193826_texture.png", true);
		tree_3Meshes = assets.loadModelMeshesAssimp("tree3", "models/trees/Evergreen_Geometry_0801193826_texture.fbx");
		for (std::size_t i = 0; i < tree_3Meshes.size(); ++i)
		{
			if (auto* mat = assets.getMaterial(assets.loadMaterial("tree3Mat" + std::to_string(i))))
			{
				mat->shader = assets.getDefaultShader();
				mat->ambient = glm::vec3(0.2f);
				mat->diffuse = glm::vec3(0.8f);
				mat->specular = glm::vec3(0.5f);
				mat->shininess = 16.0f;
				mat->difTex = tree3_texture;
				mat->specTex = defaultGrayTex;
			}
			else
			{
				std::cerr << "Failed to get material for tree3 mesh " << i << "\n";
			}
		}

	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load tree_3, falling back to cube: " << e.what() << "\n";
		tree_3Meshes.clear();
	}

	Handle<engine::Mesh> palmTree;
	try {
		Handle<engine::Texture> palmTree_texture = assets.loadTexture("palmTree_diffuse", "textures/trees/palm_tree.png", true);
		palmTree = assets.loadMeshAssimp("palmTree", "models/trees/palm_tree.fbx");
		if (auto* mat = assets.getMaterial(assets.loadMaterial("palmTreeMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = palmTree_texture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for palm tree\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "Failed to load palm tree mesh, falling back to cube: " << e.what() << "\n";
		palmTree = cubeMesh;
	}


	// cactus
	Handle<engine::Mesh> cactusMesh;
	try
	{
		// cactus is obj
		cactusMesh = assets.loadMesh("cactus", "models/plants/cactus_1.obj");
		Handle<engine::Texture> cactusTexture = assets.loadTexture("cactus_diffuse", "textures/plants/cactus_Albedo.png", true);
		if (auto* mat = assets.getMaterial(assets.loadMaterial("cactusMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = cactusTexture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for cactus\n";
		}

	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load cactus mesh, falling back to cube: " << e.what() << "\n";
		cactusMesh = cubeMesh;
	}

	Handle<engine::Mesh> desertHouse;
	try {
		desertHouse = assets.loadMeshAssimp("desert_house", "models/buildings/desert_house.fbx");
		Handle<engine::Texture> houseTexture = assets.loadTexture("house_diffuse", "textures/buildings/desert_house_texture.png", true);
		if (auto* mat = assets.getMaterial(assets.loadMaterial("desertHouseMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = houseTexture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for desert house\n";
		}
		
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load desert house mesh, falling back to cube: " << e.what() << "\n";
		desertHouse = cubeMesh;
	}

	Handle<engine::Mesh> rock_1;
	try {
		rock_1 = assets.loadMeshAssimp("rock", "models/rocks/rocks.obj");
		Handle<engine::Texture> rockTexture = assets.loadTexture("rock_diffuse", "textures/rocks/rocks.jpg", true);
		if (auto* mat = assets.getMaterial(assets.loadMaterial("rockMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = rockTexture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for rock\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "Failed to load rock mesh, falling back to cube: " << e.what() << "\n";
		rock_1 = cubeMesh;
	}

	Handle<engine::Mesh> desert_rock_1;
	try {
		desert_rock_1 = assets.loadMeshAssimp("desert_rock", "models/rocks/desert_rocks_1.fbx");
		Handle<engine::Texture> desertRock1Texture = assets.loadTexture("desertRock1_diffuse", "textures/rocks/desert_rocks_1_albedo.jpg", true);
		if (auto* mat = assets.getMaterial(assets.loadMaterial("desertRockMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = desertRock1Texture;
			mat->specTex = defaultGrayTex;
		}
		else
		{
			std::cerr << "Failed to get material for desert rock 1\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "Failed to load desert rock 1 mesh, falling back to cube: " << e.what() << "\n";
		desert_rock_1 = cubeMesh;
	}
	Handle<engine::Mesh> mushroom_1;
	try {
		mushroom_1 = assets.loadMeshAssimp("mushroom", "models/plants/mushroom_1.obj");
		Handle<engine::Texture> mushroomTexture = assets.loadTexture("mushroom1_diffuse", "textures/plants/mushroom_1.png", true);
		if (auto* mat = assets.getMaterial(assets.loadMaterial("mushroomMat")))
		{
			mat->shader = assets.getDefaultShader();
			mat->ambient = glm::vec3(0.2f);
			mat->diffuse = glm::vec3(0.8f);
			mat->specular = glm::vec3(0.5f);
			mat->shininess = 16.0f;
			mat->difTex = mushroomTexture;
			mat->specTex = defaultGrayTex;
		}
		else		{
			std::cerr << "Failed to get material for mushroom\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "Failed to load mushroom mesh, falling back to cube: " << e.what() << "\n";
		mushroom_1 = cubeMesh;
	}


	// character mesh and animations
	Handle<engine::Mesh> sprintMesh;
	Handle<engine::Skeleton> sprintSkeleton;
	Handle<engine::AnimationClip> idleClip;
	Handle<engine::AnimationClip> sprintClip;
	Handle<engine::AnimationClip> jumpClip;
	Handle<engine::AnimationClip> celebrateClip;

	try
	{
		sprintMesh = assets.loadMeshAssimp("playerMesh", "models/Run.fbx");
		sprintSkeleton = assets.loadSkeletonAssimp("playerSkeleton", "models/Run.fbx");

		idleClip = assets.loadAnimationClipAssimp("playerIdleAnimation", "models/Idle.fbx");
		sprintClip = assets.loadAnimationClipAssimp("playerSprintAnimation", "models/walking.fbx");
		celebrateClip = assets.loadAnimationClipAssimp("playerCelebrateAnimation","models/Spinning.fbx");
		// load jump animation if available in assets/models/jump.fbx
		try {
			jumpClip = assets.loadAnimationClipAssimp("playerJumpAnimation", "models/jump.fbx");
		} catch (...) {
			// keep jumpClip empty if not found
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Player skinned import failed, falling back to cube: " << e.what() << "\n";
		sprintMesh = cubeMesh;
	}

	int planeRes = heightmap->getWidth() - 1;
	float planeLen = 500.0f;
	Handle<engine::Mesh> terrainMesh = assets.createHeightmapMesh("terrain", terrainHeightmap, planeRes, planeLen);

	//planeRes = 1;
	//planeLen = 3000.0f;
	//Handle<engine::Mesh> floorMesh = assets.createPlaneMesh("floor", planeRes, planeLen);

	int waterPlaneRes = 256;
	float waterPlaneLen = 1000.0f;
	Handle<engine::Mesh> waterMesh = assets.createPlaneMesh("water", waterPlaneRes, waterPlaneLen);

	std::cout << "Loading materials...\n";
	Handle<engine::Material> defaultMat = assets.getDefaultMaterial();
	auto* mat = assets.getMaterial(defaultMat);
	mat->shader = assets.getDefaultShader();
	mat->ambient = glm::vec3(0.2f);
	mat->diffuse = glm::vec3(1.0f);
	mat->specular = glm::vec3(0.25f);
	mat->shininess = 32.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;
	
	Handle<engine::Material> grassMat = assets.loadMaterial("grassMat");
	mat = assets.getMaterial(grassMat);
	mat->ambient = glm::vec3(0.113, 0.152, 0.081);
	mat->diffuse = glm::vec3(0.565, 0.761, 0.404);
	mat->specular = glm::vec3(0.5, 0.5, 0.5);
	mat->shininess = 32.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> cyanMat = assets.loadMaterial("cyanMat");
	mat = assets.getMaterial(cyanMat);
	mat->ambient = glm::vec3(0.2f, 0.0f, 0.2f);
	mat->diffuse = glm::vec3(0.0f, 0.8f, 0.8f);
	mat->specular = glm::vec3(1.0f, 1.0f, 1.0f);
	mat->shininess = 32.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> magentaMat = assets.loadMaterial("magentaMat");
	mat = assets.getMaterial(magentaMat);
	mat->ambient = glm::vec3(0.2f, 0.0f, 0.2f);
	mat->diffuse = glm::vec3(0.8f, 0.0f, 0.8f);
	mat->specular = glm::vec3(1.0f, 1.0f, 1.0f);
	mat->shininess = 32.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> yellowMat = assets.loadMaterial("yellowMat");
	mat = assets.getMaterial(yellowMat);
	mat->ambient = glm::vec3(0.2f, 0.2f, 0.0f);
	mat->diffuse = glm::vec3(0.8f, 0.8f, 0.0f);
	mat->specular = glm::vec3(1.0f, 1.0f, 1.0f);
	mat->shininess = 32.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> redMat = assets.loadMaterial("redMat");
	mat = assets.getMaterial(redMat);
	mat->shader = assets.getDefaultShader();
	mat->ambient  = glm::vec3(0.25f, 0.10f, 0.02f);
	mat->diffuse  = glm::vec3(1.00f, 0.45f, 0.05f);
	mat->specular = glm::vec3(0.90f, 0.65f, 0.30f);
	mat->shininess = 24.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> blueMat = assets.loadMaterial("blueMat");
	mat = assets.getMaterial(blueMat);
	mat->shader = assets.getDefaultShader();
	mat->ambient  = glm::vec3(0.03f, 0.06f, 0.20f);
	mat->diffuse  = glm::vec3(0.10f, 0.35f, 1.00f);
	mat->specular = glm::vec3(0.70f, 0.85f, 1.00f);
	mat->shininess = 40.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> gemMat = assets.loadMaterial("gemMat");
	mat = assets.getMaterial(gemMat);
	mat->ambient = glm::vec3(0.2f);
	mat->diffuse = glm::vec3(0.8f);
	mat->specular = glm::vec3(1.0f);
	mat->shininess = 64.0f;
	mat->difTex = defaultGrayTex;
	mat->specTex = defaultGrayTex;

	Handle<engine::Material> charTex = assets.loadMaterial("charBaseTex");
	mat = assets.getMaterial(charTex);
	mat->shader = renderer.getSkinnedShader();
	mat->ambient = glm::vec3(0.2f);
	mat->diffuse = glm::vec3(0.8f);
	mat->specular = glm::vec3(1.0f);
	mat->shininess = 20.0f;
	mat->difTex = charBaseTex;
	mat->specTex = charBaseTex;

	// Terrain Material
	// Terrain Material
	Handle<engine::Material> terrainMat = assets.loadMaterial("terrainMat");
	mat = assets.getMaterial(terrainMat);

	mat->shader = renderer.getTerrainShader();
	mat->renderMode = engine::RenderMode::Terrain;

	// Splatmaps
	mat->splatMaps[0] = terrainSplat0;
	mat->splatMaps[1] = terrainSplat1;
	mat->splatMaps[2] = terrainSplat2;

	mat->splatMapCount = 3;

	// Terrain textures.
	// Splatmap 0 RGBA controls textures 0-3.
	// Splatmap 1 RGBA controls textures 4-7.
	// Splatmap 2 RGBA controls textures 8-11.


	mat->terrainTextures[0] = desertSand;
	mat->terrainTextures[1] = desertStone;
	mat->terrainTextures[2] = desertGrass;

	mat->terrainTextures[3] = beachSand;
	mat->terrainTextures[4] = beachStone;
	mat->terrainTextures[5] = beachGrass;

	mat->terrainTextures[6] = forestGrass;
	mat->terrainTextures[7] = forestFlowers;
	mat->terrainTextures[8] = forestStone;

	mat->terrainTextures[9] = stylizedSnow;
	mat->terrainTextures[10] = terrainTex10;

	mat->terrainTextureCount = 11;

	mat->terrainHeightTex = terrainHeightTex;

	mat->ambient = glm::vec3(0.25f);
	mat->diffuse = glm::vec3(1.0f);
	mat->specular = glm::vec3(0.05f);
	mat->shininess = 8.0f;

	mat->terrainTextureTiling = 32.0f;



	// Initialize scene
	{
		auto& obj = scene.createObject("DirLight");
		auto& dirLight = obj.addComponent<engine::DirectionalLight>();
		obj.transform.lookAt(glm::vec3(-0.8f, -0.5f, 0.0f));
		// obj.transform.lookAt(glm::vec3(0.0f, 0.0f, -1.0f));
		dirLight.setColor(glm::vec3(1.0f));
		dirLight.setIntensity(1.0f);
	}

	{
		player = &scene.createObject("Player");
		player->transform.setPosition(_playerStartPosition);
		player->transform.setScale(glm::vec3(0.5f));

		auto& visual = scene.createObject("PlayerVisual");
		visual.transform.setParent(&player->transform);

		auto& meshRenderer = visual.addComponent<engine::MeshRenderer>();
		meshRenderer.mesh = sprintMesh;
		meshRenderer.material = charTex;
		meshRenderer.writeStencil = true;

		auto& animator = visual.addComponent<engine::Animator>();
		animator.skeleton = sprintSkeleton;
		animator.clip = idleClip;
		animator.debugClipLogging = true;
		animator.debugClipFilter = "jump";

		auto& characterController = player->addComponent<engine::CharacterController>();
		characterController.gravity = 9.81f;
		characterController.mass = 8.0f;
		characterController.targetHeight = 1.0f;
		characterController.radiusScale = 0.95f;

		characterController.visualTransform = &visual.transform;

		if (auto* mesh = assets.getMesh(sprintMesh))
		{
			characterController.fitToMesh(*mesh);
		}

		auto& playerController = player->addComponent<PlayerController>();
		playerController.eyeHeight = 0.3f;
		playerController.cameraDistance = 4.0f;
		playerController.jumpForce = 48.0f;
		playerController.baseJumpForce = playerController.jumpForce;
		playerController.locomotionCrossfade = 0.14f;
		playerController.jumpCrossfade = 0.10f;

		playerController.animator = &animator;
		playerController.idleClip = idleClip;
		playerController.sprintClip = sprintClip;
		playerController.jumpClip = jumpClip;
		playerController.celebrateClip = celebrateClip;
		playerController.setAudioEngine(_audio, &assets);
		playerController.runSoundClip = runSoundClip;
		playerController.runFastSoundClip = runFastSoundClip;
		playerController.jumpSoundClip = jumpSoundClip;
		playerController.landingSoundClip = landingSoundClip;

		if (_audio)
		{
			if (auto* clip = assets.getAudioClip(landingSoundClip))
			{
				_audio->preloadOneShot(*clip);
			}
		}
	}

	// pointLightCenter = &scene.createObject("PointLightCenter");
	// pointLightCenter->transform.setPosition(glm::vec3(0.0f, 3.5f, -5.0f));
	// ---- Terrain Object ----
	{
		auto& terrain = scene.createObject("Terrain");

		auto& collider = terrain.addComponent<engine::HeightmapCollider>();
		collider.heightmap = heightmap;
		collider.planeLen = planeLen;
		// TERRAIN MATERIAL
		auto& meshRenderer = terrain.addComponent<engine::MeshRenderer>();
		meshRenderer.mesh = terrainMesh;
		meshRenderer.material = terrainMat;
	}

	//{
	//	auto& floor = scene.createObject("Floor");

	//	auto& mr = floor.addComponent<engine::MeshRenderer>();
	//	mr.mesh = floorMesh;
	//}

	// ---- Grass Object ----
	{
		auto& grassObj = scene.createObject("GrassField");

		grassRenderer = &grassObj.addComponent<engine::GrassRenderer>();
		grassRenderer->shader = grassShader;
		grassRenderer->shadowShader = grassShadowShader;
		grassRenderer->texture = grassBladeTex;
		grassRenderer->heightmap = heightmap;
		grassRenderer->terrainPlaneLen = planeLen;
		// Keeping track of player position --> Chunking State Variables 
		grassRenderer->centerPosition = player->transform.getPosition();
		grassRenderer->spawnRadius = 400.0f;
		grassRenderer->usePatchStreaming = true;
		// Grass Patches
		grassRenderer->patchSize = 25.0f;
		grassRenderer->patchRadius = 2;
		grassRenderer->bladesPerPatch = 2500;
		grassRenderer->reloadDistance = 25.0f; // not needed anymore 
		// Grass Mesh Data
		grassRenderer->bladeCount = 100000;
		grassRenderer->minHeight = 0.3f;
		grassRenderer->maxHeight = 0.8f;
		grassRenderer->minWidth = 0.05f;
		grassRenderer->maxWidth = 0.1f;
		grassRenderer->maxBend = 0.84f;

		// Wind Simulation Variables
		grassRenderer->windDirection = glm::normalize(glm::vec2(1.0f, 0.3f));
		grassRenderer->windStrength = 0.95f;
		grassRenderer->windSpeed = 1.2f;

		// Splatmapping Filtering 
		grassRenderer->splatmap = terrainSplat1;
		grassRenderer->useSplatmapPlacement = true;
		grassRenderer->minGrassSplatWeight = 0.15f;
		grassRenderer->maxPlacementAttemptsMultiplier = 3;

		// changed because grass is now on terrainsplat1
		grassRenderer->setSplatTextureCpu(assets.getTexture(terrainSplat1));

		// Clumping Noise
		grassRenderer->useDensityNoise = true;
		grassRenderer->densityNoiseScale = 0.025f; // higher value small noisy clumps
		grassRenderer->densityNoiseStrength = 0.95f;

	}
	// ---- Water Plane Object ----
	{
		auto& tempWaterPlane = scene.createObject("TempWaterPlane");
		tempWaterPlane.transform.setPosition(glm::vec3(0.0f, 8.5f, 0.0f));
		// tempWaterPlane.transform.setScale(glm::vec3(5000.0f, 1.0f, 5000.0f));

		auto& mr = tempWaterPlane.addComponent<engine::MeshRenderer>();
		mr.mesh = waterMesh;
		// mr.mesh = assets.loadMesh("waterPlane", "models/cube.obj");
		
		Handle<engine::Material> waterMat = assets.loadMaterial("waterMat");
		auto* matPtr = assets.getMaterial(waterMat);
		matPtr->renderMode = engine::RenderMode::Water;
		matPtr->shader = waterShader;
		matPtr->ambient = glm::vec3(0.15f, 0.25f, 0.35f);
		matPtr->diffuse = glm::vec3(0.45f, 0.70f, 0.90f);
		matPtr->specular = glm::vec3(0.85f, 0.90f, 1.0f);
		matPtr->shininess = 64.0f;
		matPtr->difTex = defaultGrayTex;
		matPtr->specTex = defaultGrayTex;
		matPtr->terrainHeightTex = terrainHeightTex;
		matPtr->terrainPlaneLen = planeLen;
		matPtr->terrainHeightScale = 400.0f;
		mr.material = waterMat;


	}

	// Initialize player and main camera
	{
		auto& camObj = scene.createObject("MainCamera");
		gameplayCameraObject = &camObj;

		float aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
		auto& camera = camObj.addComponent<engine::Camera>(45.0f, aspect, 0.1f, 1000.0f);
		scene.setMainCamera(&camera);
		
		gameplayController = player->getComponent<PlayerController>();
		gameplayController->cameraTransform = &camObj.transform;
		gameplayController->enabled = false;
		engine::Input::setMouseTrapped(false);
	}

	{
		auto& camObj = scene.createObject("EditorCamera");
		editorCameraObject = &camObj;
		const glm::vec3 playerPos = player->transform.getPosition();
		camObj.transform.setPosition(playerPos + glm::vec3(0.0f, 3.0f, 8.0f));
		camObj.transform.lookAt(playerPos + glm::vec3(0.0f, 1.0f, 0.0f));

		float aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
		auto& camera = camObj.addComponent<engine::Camera>(45.0f, aspect, 0.1f, 1000.0f);
		(void)camera;

		editorController = &camObj.addComponent<FreeCameraController>();
		editorController->enabled = false;
	}

	// Get post processing volume
	volume = renderer.getPostProcessVolume();
	volume->colorGrading.contrast = 1.0f;
	volume->colorGrading.exposure = -0.5f;
	volume->colorGrading.lift = glm::vec3(-0.05f);
	volume->colorGrading.gamma = glm::vec3(1.0f, 1.0f, 1.05f);

	// Add custom post-processing render passes
	_colorRestorePass = static_cast<ColorRestorationPass*>(
		&renderer.addPostProcessPass(std::make_unique<ColorRestorationPass>(
			config.width, config.height, colorRestoreShader)));

	// Compute color restoration increments
	_colorIncrement = 1.0f / static_cast<float>(_gameUI.maxGems);
	_colorRestorePass->pulseColorBoost = _colorIncrement;
	_colorRestorePass->pulseThickness = 5.0f;
	_colorRestorePass->pulseSoftness = 2.0f;

	engine::Input::setMouseTrapped(false);
	engine::Editor::setCurrentSceneName(config.defaultScene);

	std::cout << "Game initialized!\n";
}

void MyGame::update(float deltaTime)
{
	if (_startRequested)
    {
        _startRequested = false;
        startGame();
    }

	if (_powerUpPopups.speedBoost.active)
	{
    	_powerUpPopups.speedBoost.remainingTime -= deltaTime;

    	if (_powerUpPopups.speedBoost.remainingTime <= 0.0f)
    	{
        	_powerUpPopups.speedBoost.remainingTime = 0.0f;
        	_powerUpPopups.speedBoost.active = false;
        	_powerUpPopups.speedBoost.label = "";
    	}
	}

	if (_powerUpPopups.jumpBoost.active)
	{
    	_powerUpPopups.jumpBoost.remainingTime -= deltaTime;

    	if (_powerUpPopups.jumpBoost.remainingTime <= 0.0f)
    	{
       		_powerUpPopups.jumpBoost.remainingTime = 0.0f;
        	_powerUpPopups.jumpBoost.active = false;
        	_powerUpPopups.jumpBoost.label = "";
    	}
	}

    if (_gameUIState == GameUIState::Start)
    {
        return;
    }	
	
	if (_gameUIState == GameUIState::Playing && !_endScreenShown && allGemsCollected())
	{
    	_endScreenShown = true;
    	_gameUIState = GameUIState::End;

		if (_audio && _assets && allGemsCollectedSoundClip.valid())
		{
			if (auto* clip = _assets->getAudioClip(allGemsCollectedSoundClip))
			{
				_audio->playOneShot(*clip);
			}
		}

    	if (gameplayController)
    	{
       		gameplayController->enabled = false;
    	}

    	engine::Input::setMouseTrapped(false);

    	return;
	}

	// Update teleport cooldown
	_teleportCooldown -= deltaTime;

	if (player)
	{
		glm::vec3 playerPos = player->transform.getPosition();
		// std::cout << "Player Y Position: " << playerPos.y << "\n";

		if (playerPos.y < 9.0f && _teleportCooldown <= 0.0f)
		{
			glm::vec3 respawnPos(-200.0f, 17.0f, -8.0f);
			// Teleport back to spawn
			if (auto* controller = player->getComponent<engine::CharacterController>())
			{
				controller->teleport(respawnPos);
				_teleportCooldown = 0.5f; // 0.5 second cooldown before next teleport
			}
			
		}
	}
	if (player && grassRenderer)
	{
		const glm::vec3 playerPos = player->transform.getPosition();
		grassRenderer->updateStreaming(playerPos);
	}

	// manual color restore debugs
	if (engine::Input::isKeyDown(GLFW_KEY_C)) _collectedCyan += 0.002f;
	if (engine::Input::isKeyDown(GLFW_KEY_M)) _collectedMagenta += 0.002f;
	if (engine::Input::isKeyDown(GLFW_KEY_Y)) _collectedYellow += 0.002f;
	if (engine::Input::isKeyPressed(GLFW_KEY_K))
	{
		_collectedCyan = _collectedMagenta = _collectedYellow = 0.0f;
	}

	// Update active pulses
	static float PULSE_MAX_RADIUS = 500.0f;
	static float PULSE_SPEED = 100.0f;

	for (auto it = _activePulses.begin(); it != _activePulses.end(); )
	{
		it->radius += PULSE_SPEED * deltaTime;
		if (it->radius > PULSE_MAX_RADIUS)
		{
			// Commit color restoration progress to the baseline background
			if (it->type == Collectable::Type::Cyan)
				_collectedCyan = std::min(_collectedCyan + _colorIncrement, 1.0f);
			else if (it->type == Collectable::Type::Magenta)
				_collectedMagenta = std::min(_collectedMagenta + _colorIncrement, 1.0f);
			else if (it->type == Collectable::Type::Yellow)
				_collectedYellow = std::min(_collectedYellow + _colorIncrement, 1.0f);

			it = _activePulses.erase(it);
		}
		else
		{
			it++;
		}
	}

	if (_colorRestorePass)
	{
		// Persistent base values for restored color
		_colorRestorePass->cyan = std::min(_collectedCyan, 1.0f);
		_colorRestorePass->magenta = std::min(_collectedMagenta, 1.0f);
		_colorRestorePass->yellow = std::min(_collectedYellow, 1.0f);
		_colorRestorePass->setActivePulses(_activePulses);
	}

	if (engine::Input::isKeyPressed(GLFW_KEY_L))
	{
   		terrainSkyLightingEnabled = !terrainSkyLightingEnabled;
    	std::cout << "Terrain sky lighting: "<< (terrainSkyLightingEnabled ? "ON" : "OFF") << "\n";
	}
}

void MyGame::setEditorMode(bool editorActive, engine::Scene& scene)
{
	const bool wasEditorMode = editorModeActive;
	editorModeActive = editorActive;

	if (!wasEditorMode && editorModeActive && gameplayCameraObject && editorCameraObject)
	{
		editorCameraObject->transform.setPosition(gameplayCameraObject->transform.getPosition());
		editorCameraObject->transform.setRotation(gameplayCameraObject->transform.getRotation());
		editorController->syncFromTransform(gameplayCameraObject->transform);
	}
	else if (wasEditorMode && !editorModeActive && gameplayCameraObject && editorCameraObject)
	{
		gameplayCameraObject->transform.setPosition(editorCameraObject->transform.getPosition());
		gameplayCameraObject->transform.setRotation(editorCameraObject->transform.getRotation());
	}

	refreshEditorCameraState(scene);
}

void MyGame::setEditorSelectionLock(bool locked, engine::Scene& scene)
{
	(void)locked;
	editorCameraLocked = false;
	refreshEditorCameraState(scene);
}

void MyGame::refreshEditorCameraState(engine::Scene& scene)
{
    if (!gameplayCameraObject || !editorCameraObject || !gameplayController || !editorController)
        return;

    if (_gameUIState == GameUIState::Start || _gameUIState == GameUIState::End)
    {
        gameplayController->enabled = false;
        editorController->enabled = false;
        if (engine::Input::isMouseTrapped())
            engine::Input::setMouseTrapped(false);
        scene.setMainCamera(gameplayCameraObject->getComponent<engine::Camera>());
        return;
    }

    gameplayController->enabled = !editorModeActive;
    editorController->enabled = editorModeActive;

    if (editorModeActive)
    {
        scene.setMainCamera(editorCameraObject->getComponent<engine::Camera>());
        if (engine::Input::isMouseTrapped())
            engine::Input::setMouseTrapped(false);
    }
    else
    {
        scene.setMainCamera(gameplayCameraObject->getComponent<engine::Camera>());
        // Only call setMouseTrapped if not already trapped — avoids zeroing delta every frame
        if (!engine::Input::isMouseTrapped())
            engine::Input::setMouseTrapped(true);
    }

	
}
