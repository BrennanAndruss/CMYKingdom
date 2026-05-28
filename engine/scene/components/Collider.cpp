#include "scene/components/Collider.h"

#include <btBulletDynamicsCommon.h>
#include "physics/PhysicsSystem.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "scene/components/RigidBody.h"
#include "scene/components/AnimatedVelocity.h"

namespace engine
{
	namespace
	{
		bool usesAnimatedVelocity(const Object* owner)
		{
			return owner && owner->getComponent<AnimatedVelocity>();
		}

		void configureCollisionFlags(btCollisionObject* object, bool isTrigger, bool isKinematic)
		{
			if (!object) return;

			int flags = object->getCollisionFlags();
			flags &= ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
			if (isKinematic)
			{
				flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
			}
			else
			{
				flags |= btCollisionObject::CF_STATIC_OBJECT;
			}

			if (isTrigger)
			{
				flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
			}

			object->setCollisionFlags(flags);
			if (isTrigger || isKinematic)
			{
				object->setActivationState(DISABLE_DEACTIVATION);
			}
		}
	}

	void Collider::update(float deltaTime)
	{
		if (!_object)
		{
			return;
		}

		btTransform t;
		t.setIdentity();
		t.setOrigin(PhysicsSystem::toBullet(owner->transform.getWorldPosition()));
		t.setRotation(PhysicsSystem::toBullet(owner->transform.getWorldRotation()));
		_object->setWorldTransform(t);
	}

	void Collider::destroyCollisionObject()
	{
		if (!_object) return;
		
		if (auto* physics = owner->getScene()->getPhysicsSystem())
		{
			// Unregister callback and remove from physics world
			physics->removeCollisionObject(_object);
		}

		delete _object;
		_object = nullptr;
	}

#pragma region BoxCollider

	// Constructors defined in implementation where classes are included
	BoxCollider::BoxCollider() = default;
	BoxCollider::~BoxCollider()
	{
		destroyCollisionObject();
	}

	btCollisionShape* BoxCollider::getShape() const
	{
		return _shape.get();
	}

	void BoxCollider::start()
	{
		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();
		_shape = std::make_unique<btBoxShape>(PhysicsSystem::toBullet(size));

		glm::vec3 worldScale = owner->transform.getWorldScale();
		_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
		const glm::vec3 worldCenter = center * worldScale;

		// If object doesn't have a RigidBody, register as static collision object
		if (!owner->getComponent<RigidBody>())
		{
			_object = physics->createCollisionObject(
				_shape.get(), owner->transform.getWorldPosition() + worldCenter, isTrigger);
			_object->setUserPointer(owner);

			configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
		}
	}

	void BoxCollider::update(float deltaTime)
	{
		if (_shape)
		{
			const glm::vec3 worldScale = owner->transform.getWorldScale();
			_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
			if (_object)
			{
				configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
				btTransform t;
				t.setIdentity();
				t.setOrigin(PhysicsSystem::toBullet(owner->transform.getWorldPosition() + (center * worldScale)));
				t.setRotation(PhysicsSystem::toBullet(owner->transform.getWorldRotation()));
				_object->setWorldTransform(t);
			}
		}
	}

	void BoxCollider::rebuild()
	{
		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();
		
		// Recreate the shape with the new size
		_shape = std::make_unique<btBoxShape>(PhysicsSystem::toBullet(size));
		
		glm::vec3 worldScale = owner->transform.getWorldScale();
		_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
		const glm::vec3 worldCenter = center * worldScale;
		
		// Remove the old collision object from physics system
		if (_object)
		{
			physics->removeCollisionObject(_object);
			_object = nullptr;
		}
		
		// Create new collision object if no RigidBody
		if (!owner->getComponent<RigidBody>())
		{
			_object = physics->createCollisionObject(
				_shape.get(), owner->transform.getWorldPosition() + worldCenter, isTrigger);
			_object->setUserPointer(owner);
			
			configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
		}
	}

#pragma endregion

#pragma region SphereCollider

	SphereCollider::SphereCollider() = default;
	SphereCollider::~SphereCollider()
	{
		destroyCollisionObject();
	}

	btCollisionShape* SphereCollider::getShape() const
	{
		return _shape.get();
	}

	void SphereCollider::start()
	{
		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();
		_shape = std::make_unique<btSphereShape>(radius);

		glm::vec3 worldScale = owner->transform.getWorldScale();
		_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
		const glm::vec3 worldCenter = center * worldScale;

		// If object doesn't have a RigidBody, register as static collision object
		if (!owner->getComponent<RigidBody>())
		{
			_object = physics->createCollisionObject(
				_shape.get(), owner->transform.getWorldPosition() + worldCenter, isTrigger);
			_object->setUserPointer(owner);

			configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
		}
	}

	void SphereCollider::update(float deltaTime)
	{
		if (_shape)
		{
			const glm::vec3 worldScale = owner->transform.getWorldScale();
			_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
			if (_object)
			{
				configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
				btTransform t;
				t.setIdentity();
				t.setOrigin(PhysicsSystem::toBullet(owner->transform.getWorldPosition() + (center * worldScale)));
				t.setRotation(PhysicsSystem::toBullet(owner->transform.getWorldRotation()));
				_object->setWorldTransform(t);
			}
		}
	}

#pragma endregion

#pragma region CapsuleCollider

	CapsuleCollider::CapsuleCollider() = default;
	CapsuleCollider::~CapsuleCollider()
	{
		destroyCollisionObject();
	}

	btCollisionShape* CapsuleCollider::getShape() const
	{
		return _shape.get();
	}

	void CapsuleCollider::start()
	{
		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();

		// Convert to bullet's internal height (sphere center-to-center)
		float bulletHeight = height - (2.0f * radius);
		if (bulletHeight < 0) bulletHeight = 0.0f;

		switch (direction)
		{
		case Axis::X:
			_shape = std::make_unique<btCapsuleShapeX>(radius, bulletHeight);
			break;
		case Axis::Y:
			_shape = std::make_unique<btCapsuleShape>(radius, bulletHeight);
			break;
		case Axis::Z:
			_shape = std::make_unique<btCapsuleShapeZ>(radius, bulletHeight);
			break;
		}

		glm::vec3 worldScale = owner->transform.getWorldScale();
		_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
		const glm::vec3 worldCenter = center * worldScale;

		// If object doesn't have a RigidBody, register as static collision object
		if (!owner->getComponent<RigidBody>())
		{
			_object = physics->createCollisionObject(
				_shape.get(), owner->transform.getWorldPosition() + worldCenter, isTrigger);
			_object->setUserPointer(owner);

			configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
		}
	}

	void CapsuleCollider::update(float deltaTime)
	{
		if (_shape)
		{
			const glm::vec3 worldScale = owner->transform.getWorldScale();
			_shape->setLocalScaling(PhysicsSystem::toBullet(worldScale));
			if (_object)
			{
				configureCollisionFlags(_object, isTrigger, usesAnimatedVelocity(owner));
				btTransform t;
				t.setIdentity();
				t.setOrigin(PhysicsSystem::toBullet(owner->transform.getWorldPosition() + (center * worldScale)));
				t.setRotation(PhysicsSystem::toBullet(owner->transform.getWorldRotation()));
				_object->setWorldTransform(t);
			}
		}
	}

#pragma endregion
}