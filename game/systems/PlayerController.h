#pragma once

#include <glm/glm.hpp>
#include <string>
#include "scene/Transform.h"
#include "scene/components/Component.h"
#include "scene/components/CharacterController.h"
#include "resources/Handle.h"
#include "resources/SkeletalAnimation.h"
#include "scene/components/Animator.h"

class PlayerController : public engine::Component
{
public:
	float baseMoveSpeed = 0.1f;
	float moveSpeed = 0.1f;
	float speedBoostMultiplier = 2.0f;
	float speedBoostDuration = 15.0f;
	float speedBoostTimer = 0.0f;
	float rotationSpeed = 1.0f;
	float eyeHeight = 0.4f;
	float cameraDistance = 4.5f;
	float sensitivity = 0.1f;
	float jumpForce = 32.0f;
	float baseJumpForce = 32.0f;
	float jumpBoostMultiplier = 2.0f;
	float jumpBoostDuration = 15.0f;
	float jumpBoostTimer = 0.0f;
	float jumpBufferDuration = 0.20f;
	float groundedGraceDuration = 0.12f;
	float platformCarryFactor = 1.0f;
	// Tune blend feel from game setup without touching Animator internals.
	float locomotionCrossfade = 0.22f;
	float jumpCrossfade = 0.1f;
	bool invertMouseMove = false;
	bool enabled = true;
	bool hasJumped = false;

	engine::Transform* cameraTransform = nullptr;
	
	void start() override;
	void update(float deltaTime) override;
	void postPhysicsUpdate(float deltaTime) override;
	void activateSpeedBoost();
	void activateJumpBoost();
	engine::Animator* animator = nullptr;
	Handle<engine::AnimationClip> idleClip;
	Handle<engine::AnimationClip> sprintClip;
	Handle<engine::AnimationClip> jumpClip;

private:
	engine::CharacterController* _characterController = nullptr;
	engine::Object* _groundCarrier = nullptr;
	glm::vec3 _lastGroundCarrierWorldPosition = glm::vec3(0.0f);
	glm::vec3 _pendingCarrierDelta = glm::vec3(0.0f);
	std::string _pendingCarrierName;
	float _jumpBufferTimer = 0.0f;
	float _groundedGraceTimer = 0.0f;

	float _yaw = 0.0f;
	float _pitch = 0.0f;
};