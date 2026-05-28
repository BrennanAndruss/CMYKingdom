#include "scene/components/RigidBody.h"

#include <btBulletDynamicsCommon.h>
#include <cassert>
#include <glm/gtc/quaternion.hpp>
#include "physics/PhysicsSystem.h"
#include "scene/Scene.h"
#include "scene/Object.h"

namespace engine
{
	namespace
	{
		glm::vec3 getColliderWorldCenter(const Collider* collider, const Object* owner)
		{
			if (!collider || !owner)
			{
				return glm::vec3(0.0f);
			}

			const glm::vec3 worldScale = glm::abs(owner->transform.getWorldScale());
			const glm::quat worldRotation = owner->transform.getWorldRotation();

			if (const auto* boxCollider = dynamic_cast<const BoxCollider*>(collider))
			{
				return glm::mat3_cast(worldRotation) * (boxCollider->center * worldScale);
			}

			if (const auto* sphereCollider = dynamic_cast<const SphereCollider*>(collider))
			{
				return glm::mat3_cast(worldRotation) * (sphereCollider->center * worldScale);
			}

			if (const auto* capsuleCollider = dynamic_cast<const CapsuleCollider*>(collider))
			{
				return glm::mat3_cast(worldRotation) * (capsuleCollider->center * worldScale);
			}

			return glm::vec3(0.0f);
		}

		glm::vec3 getColliderWorldPosition(const Collider* collider, const Object* owner)
		{
			return owner ? owner->transform.getWorldPosition() + getColliderWorldCenter(collider, owner)
				: glm::vec3(0.0f);
		}
	}

	RigidBody::~RigidBody()
	{
		destroyBody();
	}

	void RigidBody::destroyBody()
	{
		if (!_body) return;

		if (auto* physics = owner->getScene()->getPhysicsSystem())
		{
			// Unregister callback and remove from physics world
			physics->removeBody(_body);
		}

		delete _body;
		_body = nullptr;
	}

	void RigidBody::disablePhysics()
	{
		_physicsDisabled = true;
		destroyBody();
	}

	bool RigidBody::initializeBody()
	{
		if (_physicsDisabled) return false;
		if (_body && !_bodyDirty) return true;

		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();
		if (!physics) return false;

		if (_body)
		{
			destroyBody();
		}

		if (!_collider)
		{
			_collider = owner->getComponent<Collider>();
		}
		if (!_collider) return false;

		_collider->destroyCollisionObject();

		const float effectiveMass = bodyType == BodyType::Dynamic ? mass : 0.0f;
		_body = physics->createBody(
			_collider->getShape(), getColliderWorldPosition(_collider, owner), 
			effectiveMass, _collider->isTrigger
		);

			// Apply friction setting
			_body->setFriction(friction);

		if (bodyType == BodyType::Kinematic)
		{
			_body->setCollisionFlags(
				_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT
			);
			_body->setActivationState(DISABLE_DEACTIVATION);
		}

		// Set user pointer for callbacks
		_body->setUserPointer(owner);
		_bodyDirty = false;
		return true;
	}

	void RigidBody::setBodyType(BodyType type)
	{
		if (bodyType == type) return;

		bodyType = type;
		_bodyDirty = true;
		initializeBody();
	}

	void RigidBody::start()
	{
		initializeBody();
		_lastWorldPosition = owner ? owner->transform.getWorldPosition() : glm::vec3(0.0f);
		_lastFrameDisplacementWorld = glm::vec3(0.0f);
	}

	void RigidBody::update(float deltaTime)
	{
		if (_physicsDisabled || !_body) return;

		const glm::vec3 previousWorldPosition = owner->transform.getWorldPosition();

		if (bodyType == BodyType::Static || bodyType == BodyType::Kinematic)
		{
			btTransform t;
			t.setIdentity();
			t.setOrigin(PhysicsSystem::toBullet(getColliderWorldPosition(_collider, owner)));
			t.setRotation(PhysicsSystem::toBullet(owner->transform.getWorldRotation()));
			_body->setWorldTransform(t);
			if (auto* motionState = _body->getMotionState())
			{
				motionState->setWorldTransform(t);
			}
			if (bodyType == BodyType::Kinematic)
			{
				_body->setActivationState(DISABLE_DEACTIVATION);
			}
			_lastFrameDisplacementWorld = owner->transform.getWorldPosition() - _lastWorldPosition;
			_lastWorldPosition = owner->transform.getWorldPosition();
			return;
		}

		btTransform t;
		_body->getMotionState()->getWorldTransform(t);

		// Sync the engine transform with the physics world transform
		owner->transform.setPosition(PhysicsSystem::toGlm(t.getOrigin()) - getColliderWorldCenter(_collider, owner));
		owner->transform.setRotation(PhysicsSystem::toGlm(t.getRotation()));
		_lastFrameDisplacementWorld = owner->transform.getWorldPosition() - previousWorldPosition;
		_lastWorldPosition = owner->transform.getWorldPosition();
	}

	void RigidBody::setLinearVelocity(glm::vec3 velocity)
	{
		if (!_body) return;
		_body->setLinearVelocity(PhysicsSystem::toBullet(velocity));
	}

	glm::vec3 RigidBody::getLinearVelocity() const
	{
		if (!_body) return glm::vec3(0.0f);
		return PhysicsSystem::toGlm(_body->getLinearVelocity());
	}
}