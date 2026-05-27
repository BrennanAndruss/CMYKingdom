#pragma once

#include <string>
#include <vector>
#include <memory>
#include <type_traits>
#include <cassert>
#include "renderer/BoundingVolume.h"
#include "scene/Transform.h"
#include "scene/components/Component.h"

namespace engine
{
	class Scene;
	class AssetManager;
}

namespace engine
{
	class Object
	{
	public:
		std::string name;
		Transform transform;
		bool markedForDeletion = false;

		Object();
		Object(const std::string& name);
		~Object() = default;

		void start();
		void update(float deltaTime);
		void update(float deltaTime, AssetManager& assets);

		template<typename T, typename... Args>
		T& addComponent(Args&&... args)
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			auto& ptr = _components.emplace_back(
				std::make_unique<T>(std::forward<Args>(args)...)
			);
			ptr->owner = this;
			if (_started)
			{
				ptr->start();
			}
			return static_cast<T&>(*ptr);
		}

		template<typename T>
		T* getComponent() const
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			for (auto& component : _components)
			{
				if (auto* c = dynamic_cast<T*>(component.get()))
					return c;
			}
			return nullptr;
		}

		void setScene(Scene* scene) { _scene = scene; }
		Scene* getScene() const {
			assert(_scene && "Scene not set in object");
			return _scene;
		}

		// Bounding volume hierarchy
		const BBox& getWorldBBox(const AssetManager& assets) const;
		BBox getHierarchyBBox(const AssetManager& assets) const;

		bool isWorldBBoxDirty() const { return _worldBBoxDirty; }
		void markWorldBBoxDirty();
		bool isHierarchyBBoxDirty() const { return _hierarchyBBoxDirty; }
		void markHierarchyBBoxDirty();

	private:
		Scene* _scene = nullptr;
		bool _started = false;

		// Mutable bounding volume cache
		mutable BBox _worldBBox;
		mutable bool _worldBBoxDirty = true;
		mutable BBox _hierarchyBBox;
		mutable bool _hierarchyBBoxDirty = true;

		std::vector<std::unique_ptr<Component>> _components;
	};
}