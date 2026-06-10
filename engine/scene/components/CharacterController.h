#pragma once

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <glm/glm.hpp>
#include <memory>
#include "scene/components/Component.h"
#include "scene/Transform.h"
#include "renderer/resources/Mesh.h"



namespace engine
{
	class CharacterController : public Component
	{
	public:
	
		float height = 1.0f;
		float radius = 0.5f;
		float gravity = 9.81f;
		float mass = 1.0f;
		float radiusScale = 0.95f;
		float targetHeight = 1.0f;
		glm::vec3 colliderOffset = glm::vec3(0.0f, -2.0f, 0.0f);

		engine::Transform* visualTransform = nullptr;

		void fitToMesh(const engine::Mesh& mesh);
		void start() override;
		void update(float deltaTime) override;
		void postPhysicsUpdate(float deltaTime) override;

		void move(glm::vec3 delta);
		void moveWorldOffset(const glm::vec3& delta);
		glm::vec3 getCurrentSyncedWorldPosition() const { return _currentSyncedWorldPosition; }
		glm::vec3 getPreviousSyncedWorldPosition() const { return _previousSyncedWorldPosition; }
		bool isOnGround() const;
		bool canJump() const;
		void jump(glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f));
		void teleport(const glm::vec3& position);
		void setGravity(float newGravity);
		float getGravity() const;

	private:
		btPairCachingGhostObject* _ghostObject = nullptr;
		btKinematicCharacterController* _controller = nullptr;
		std::unique_ptr<btConvexShape> _shape;

		glm::vec3 _walkDirection = glm::vec3(0.0f);
		bool _justTeleported = false;
		bool _hasGroundAnchor = false;
		float _groundAnchorY = 0.0f;
		glm::vec3 _currentSyncedWorldPosition = glm::vec3(0.0f);
		glm::vec3 _previousSyncedWorldPosition = glm::vec3(0.0f);
		glm::vec3 _postPhysicsSyncedWorldPosition = glm::vec3(0.0f);
	};
}