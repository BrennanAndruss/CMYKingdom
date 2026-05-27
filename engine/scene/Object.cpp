#include "scene/Object.h"

#include "resources/AssetManager.h"
#include "renderer/resources/Mesh.h"
#include "scene/components/MeshRenderer.h"

namespace engine
{
	Object::Object()
	{
		transform.owner = this;
	}
	
	Object::Object(const std::string& name) : name(name) 
	{
		transform.owner = this;
	}

	void Object::start()
	{
		_started = true;
		for (auto& component : _components)
		{
			component->start();
		}
	}

	void Object::update(float deltaTime)
	{
		for (auto& component : _components)
		{
			component->update(deltaTime);
		}
	}

	void Object::update(float deltaTime, AssetManager& assets)
	{
		for (auto& component : _components)
		{
			component->update(deltaTime, assets);
		}
	}

	const BBox& Object::getWorldBBox(const AssetManager& assets) const
	{
		if (!_worldBBoxDirty)
		{
			return _worldBBox;
		}

		_worldBBox = BBox{};
		_worldBBoxDirty = false;

		// Union of all mesh renderer bboxes in local space, transformed to world space
		if (auto* meshRenderer = getComponent<MeshRenderer>())
		{
			Mesh* mesh = assets.getMesh(meshRenderer->mesh);
			if (mesh)
			{
				_worldBBox = mesh->getBBox().transformed(transform.getWorldMatrix());
			}
		}

		return _worldBBox;
	}

	void Object::markWorldBBoxDirty()
	{
		_worldBBoxDirty = true;
		markHierarchyBBoxDirty();
	}

	BBox Object::getHierarchyBBox(const AssetManager& assets) const
	{
		if (!_hierarchyBBoxDirty)
		{
			return _hierarchyBBox;
		}

		_hierarchyBBox = getWorldBBox(assets);
		_hierarchyBBoxDirty = false;

		for (auto* childTransform : transform.getChildren())
		{
			if (Object* child = childTransform->owner)
			{
				BBox childBBox = child->getHierarchyBBox(assets);
				_hierarchyBBox.expand(childBBox);
			}
		}

		return _hierarchyBBox;
	}

	void Object::markHierarchyBBoxDirty()
	{
		_hierarchyBBoxDirty = true;

		// Cascade up to parent objects
		if (transform.getParent())
		{
			if (Object* parent = transform.getParent()->owner)
			{
				parent->markHierarchyBBoxDirty();
			}
		}
	}
}