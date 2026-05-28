#include "scene/components/CharacterController.h"

#include <cmath>
#include <iostream>

#include "physics/PhysicsSystem.h"
#include "scene/Scene.h"
#include "scene/Object.h"



namespace engine
{
	namespace
	{
		glm::vec3 makeControllerPosition(const glm::vec3& position, float height, const glm::vec3& offset)
		{
			return position + glm::vec3(0.0f, height * 0.5f, 0.0f) + offset;
		}
	}

	void CharacterController::start()
	{
		PhysicsSystem* physics = owner->getScene()->getPhysicsSystem();
		const glm::vec3 controllerPosition = makeControllerPosition(owner->transform.getWorldPosition(), height, colliderOffset);
		owner->transform.setPosition(controllerPosition);
		_currentSyncedWorldPosition = controllerPosition;
		_previousSyncedWorldPosition = _currentSyncedWorldPosition;
		_groundAnchorY = controllerPosition.y;
		_hasGroundAnchor = true;
		
		const glm::vec3 halfExtents(radius, height * 0.5f, radius);
		_shape = std::make_unique<btBoxShape>(PhysicsSystem::toBullet(halfExtents));

		_ghostObject = new btPairCachingGhostObject();
		_ghostObject->setCollisionShape(_shape.get());

		btTransform t;
		t.setIdentity();
		t.setOrigin(PhysicsSystem::toBullet(controllerPosition));
		_ghostObject->setWorldTransform(t);

		// Set user pointer for callbacks
		_ghostObject->setUserPointer(owner);
		_ghostObject->setCollisionFlags(_ghostObject->getCollisionFlags() |
			btCollisionObject::CF_CHARACTER_OBJECT);
		_ghostObject->setActivationState(DISABLE_DEACTIVATION);

		// Configure controller logic
		float stepHeight = 0.0f;
		_controller = new btKinematicCharacterController(_ghostObject, _shape.get(), stepHeight);
		_controller->setUp(btVector3(0, 1, 0));
		_controller->setGravity(btVector3(0, -gravity, 0));
		_controller->setMaxPenetrationDepth(0.01f);

		// Add to world with filters
		int group = btBroadphaseProxy::CharacterFilter;
		int mask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;

		physics->addCollisionObject(_ghostObject, group, mask);
		physics->addAction(_controller);
	}

	void CharacterController::fitToMesh(const engine::Mesh& mesh)
	{
		auto bbox = mesh.getBBox();

		float modelHeight = bbox.max.y - bbox.min.y;
		float modelWidth = bbox.max.x - bbox.min.x;
		float modelDepth = bbox.max.z - bbox.min.z;

		if (modelHeight > 0.0001f)
		{
			float scale = targetHeight / modelHeight;

			owner->transform.setScale(glm::vec3(scale));

			height = targetHeight;
			radius = glm::max(modelWidth, modelDepth) * scale * 0.5f * radiusScale;

			if (visualTransform)
			{
				float localOffsetY = (-height * 0.5f / scale) - bbox.min.y;
				visualTransform->setPosition(glm::vec3(0.0f, localOffsetY - colliderOffset.y, 0.0f));
			}
		}
	}

	void CharacterController::update(float deltaTime)
	{
		_controller->setWalkDirection(PhysicsSystem::toBullet(_walkDirection));

		_walkDirection = glm::vec3(0.0f);
	}

	void CharacterController::postPhysicsUpdate(float deltaTime)
	{
		if (!_ghostObject || !_controller)
		{
			return;
		}

		if (_justTeleported)
		{
			_justTeleported = false;
			_previousSyncedWorldPosition = owner->transform.getPosition();
			_currentSyncedWorldPosition = _previousSyncedWorldPosition;
			_postPhysicsSyncedWorldPosition = _currentSyncedWorldPosition;
			return;
		}

		btTransform t = _ghostObject->getWorldTransform();
		_previousSyncedWorldPosition = _currentSyncedWorldPosition;
		glm::vec3 syncedPosition = PhysicsSystem::toGlm(t.getOrigin());
		const float verticalDelta = std::abs(syncedPosition.y - _currentSyncedWorldPosition.y);
		if (verticalDelta < 0.02f)
		{
			syncedPosition.y = _currentSyncedWorldPosition.y;
		}
		const bool grounded = _controller->onGround();
		if (grounded)
		{
			if (!_hasGroundAnchor)
			{
				_groundAnchorY = syncedPosition.y;
				_hasGroundAnchor = true;
			}
			syncedPosition.y = _groundAnchorY;
		}
		else
		{
			_hasGroundAnchor = false;
		}
		owner->transform.setPosition(syncedPosition);
		_currentSyncedWorldPosition = syncedPosition;
		_postPhysicsSyncedWorldPosition = _currentSyncedWorldPosition;
	}

	void CharacterController::moveWorldOffset(const glm::vec3& delta)
	{
		if (!_ghostObject || !_controller || glm::dot(delta, delta) <= 0.0f)
		{
			return;
		}

		const glm::vec3 newPosition = owner->transform.getPosition() + delta;
		btVector3 bulletPos = PhysicsSystem::toBullet(newPosition);

		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(bulletPos);

		_controller->warp(bulletPos);
		_ghostObject->setWorldTransform(transform);
		_ghostObject->setInterpolationWorldTransform(transform);
		owner->transform.setPosition(newPosition);
		_previousSyncedWorldPosition = newPosition;
		_currentSyncedWorldPosition = newPosition;
		_groundAnchorY = newPosition.y;
		_hasGroundAnchor = false;
	}

	void CharacterController::move(glm::vec3 direction)
	{
		_walkDirection = direction;
		if (_controller)
		{
			_controller->setWalkDirection(PhysicsSystem::toBullet(_walkDirection));
		}
	}

	bool CharacterController::isOnGround() const
	{
		return _controller ? _controller->onGround() : false;
	}

	bool CharacterController::canJump() const
	{
		return _controller ? _controller->canJump() : false;
	}

	void CharacterController::jump(glm::vec3 direction)
	{
		if (_controller)
		{
			_controller->jump(btVector3(direction.x, direction.y, direction.z));
		}
	}

	

void CharacterController::teleport(const glm::vec3& position)
{
	if (!_ghostObject || !_controller)
		return;

	const glm::vec3 adjustedPos = makeControllerPosition(position, height, colliderOffset);

	btVector3 bulletPos = PhysicsSystem::toBullet(adjustedPos);

	// Warp character controller
	_controller->warp(bulletPos);

	// Hard sync ghost transform
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(bulletPos);

	_ghostObject->setWorldTransform(transform);
	_ghostObject->setInterpolationWorldTransform(transform);

	// Clear velocities/motion
	_controller->setLinearVelocity(btVector3(0, 0, 0));
	_controller->setWalkDirection(btVector3(0, 0, 0));

	_walkDirection = glm::vec3(0.0f);
	owner->transform.setPosition(adjustedPos);
	_groundAnchorY = adjustedPos.y;
	_hasGroundAnchor = false;

	// Flag to skip physics sync for one frame to let it settle
	_justTeleported = true;
}

		
}