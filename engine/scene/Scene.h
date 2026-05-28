#pragma once

#include <string>
#include <vector>
#include <memory>
#include <type_traits>
#include <cassert>
#include <glm/glm.hpp>
#include "physics/PhysicsSystem.h"
#include "scene/Object.h"
#include "scene/components/Camera.h"
#include "scene/components/Light.h"
#include "resources/Handle.h"

namespace engine
{
	class Cubemap;
	class AssetManager;

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void start();
		void update(float deltaTime);
		void update(float deltaTime, AssetManager& assets);

		Object& createObject(const std::string& name);

		std::vector<std::unique_ptr<Object>>& getObjects() { return _objects; }
		
		const std::vector<std::unique_ptr<Object>>& getObjects() const { return _objects; }
		std::vector<Object*> getRootObjects() const;

		void setPhysicsSystem(PhysicsSystem* physics) { _physics = physics; }
		PhysicsSystem* getPhysicsSystem() const {
			assert(_physics && "PhysicsSystem not set in scene");
			return _physics; 
		}

		void addCamera(Camera* camera) { _cameras.push_back(camera); }
		const std::vector<Camera*>& getCameras() const { return _cameras; }

		void setMainCamera(Camera* camera) { _mainCamera = camera; }
		Camera* getMainCamera() const {
			assert(_mainCamera && "Main camera not set in scene");
			return _mainCamera;
		}

		void addLight(Light* light) { _lights.push_back(light); }
		const std::vector<Light*>& getLights() const { return _lights; }

		void setSkybox(Handle<Cubemap> cubemap)
		{
			_skybox = cubemap;
		}

		Handle<Cubemap> getSkybox() const { return _skybox; }
		bool hasSkybox() const { return _skybox.valid(); }

		void setIrradianceMap(Handle<Cubemap> cubemap)
		{
			_irradianceMap = cubemap;
		}

		Handle<Cubemap> getIrradianceMap() const { return _irradianceMap; }
		bool hasIrradianceMap() const { return _irradianceMap.valid(); }

	private:
		std::vector<std::unique_ptr<Object>> _objects;
		bool _started = false;

		PhysicsSystem* _physics = nullptr;
		Camera* _mainCamera = nullptr;
		std::vector<Camera*> _cameras;
		std::vector<Light*> _lights;

		Handle<Cubemap> _skybox;
		Handle<Cubemap> _irradianceMap;

		void cleanupDeleted();
		void resolveTransforms(Transform& t, const glm::mat4& parentWorld);
	};
}