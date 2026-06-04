#include "systems/PlayerController.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <iostream>
#include <limits>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "core/Input.h"
#include "scene/Object.h"
#include "scene/Scene.h"
#include "scene/components/Audio.h"
#include "scene/components/Collider.h"
#include "scene/components/RigidBody.h"

namespace
{
	engine::Object* findGroundCarrier(const engine::Object& player,
		const engine::CharacterController& characterController,
		engine::Object* preferredCarrier)
	{
		engine::Scene* scene = player.getScene();
		const glm::vec3 playerPos = player.transform.getPosition();
		const float playerFootY = playerPos.y - (characterController.height * 0.5f);
		const float xzPadding = characterController.radius * 0.35f;
		const float footTopTolerance = 0.45f;

		auto isCandidate = [&](engine::Object& object) -> bool
		{
			auto* boxCollider = object.getComponent<engine::BoxCollider>();
			if (!boxCollider || boxCollider->isTrigger)
			{
				return false;
			}

			const glm::vec3 worldPos = object.transform.getWorldPosition();
			const glm::vec3 worldScale = glm::abs(object.transform.getWorldScale());
			const glm::vec3 center = worldPos + (boxCollider->center * worldScale);
			const glm::vec3 halfExtents = boxCollider->size * worldScale;
			const float topY = center.y + halfExtents.y;

			const bool withinX = std::abs(playerPos.x - center.x) <= (halfExtents.x + xzPadding);
			const bool withinZ = std::abs(playerPos.z - center.z) <= (halfExtents.z + xzPadding);
			const bool nearTop = std::abs(playerFootY - topY) <= footTopTolerance;
			return withinX && withinZ && nearTop;
		};

		if (preferredCarrier && isCandidate(*preferredCarrier))
		{
			return preferredCarrier;
		}

		float closestTop = -std::numeric_limits<float>::infinity();
		float closestFootDistance = std::numeric_limits<float>::max();
		engine::Object* bestCarrier = nullptr;

		for (const auto& objectPtr : scene->getObjects())
		{
			if (!objectPtr || objectPtr.get() == &player || objectPtr->markedForDeletion)
			{
				continue;
			}

			if (!isCandidate(*objectPtr))
			{
				continue;
			}

			const glm::vec3 worldPos = objectPtr->transform.getWorldPosition();
			const glm::vec3 worldScale = glm::abs(objectPtr->transform.getWorldScale());
			auto* boxCollider = objectPtr->getComponent<engine::BoxCollider>();
			const glm::vec3 center = worldPos + (boxCollider->center * worldScale);
			const glm::vec3 halfExtents = boxCollider->size * worldScale;
			const float topY = center.y + halfExtents.y;
			const float footDistance = std::abs(playerFootY - topY);
			if (footDistance < closestFootDistance || (footDistance == closestFootDistance && topY > closestTop))
			{
				closestTop = topY;
				closestFootDistance = footDistance;
				bestCarrier = objectPtr.get();
			}
		}

		return bestCarrier;
	}
}



void PlayerController::start()
{
	_characterController = owner->getComponent<engine::CharacterController>();
	assert(_characterController && "PlayerController requires CharacterController component");

	assert(cameraTransform && "PlayerController requires Camera component transform");
	const glm::vec3 focus = owner->transform.getPosition() + glm::vec3(0.0f, eyeHeight, 0.0f);
	const glm::quat yawQuat = glm::angleAxis(glm::radians(_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::quat pitchQuat = glm::angleAxis(glm::radians(_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
	const glm::vec3 offset = yawQuat * pitchQuat * glm::vec3(0.0f, eyeHeight, -cameraDistance);
	cameraTransform->setPosition(focus + offset);
	cameraTransform->lookAt(focus);
}

void PlayerController::setAudioEngine(engine::AudioEngine* audioEngine)
{
	_audio = audioEngine;
}

void PlayerController::update(float deltaTime)
{
	if (!enabled)
	{
		return;
	}

	if (speedBoostTimer > 0.0f)
	{
					speedBoostTimer -= deltaTime;

					if (speedBoostTimer <= 0.0f)
					{
									speedBoostTimer = 0.0f;
									moveSpeed = baseMoveSpeed;
					}
	}

	if (jumpBoostTimer > 0.0f)
	{
		jumpBoostTimer -= deltaTime;

		if (jumpBoostTimer <= 0.0f)
		{
			jumpBoostTimer = 0.0f;
			jumpForce = baseJumpForce;
		}
	}

	if (_jumpBufferTimer > 0.0f)
	{
		_jumpBufferTimer -= deltaTime;
		if (_jumpBufferTimer < 0.0f)
		{
			_jumpBufferTimer = 0.0f;
		}
	}

	const bool groundedNow = _characterController->isOnGround();

	if (groundedNow)
	{
		_groundedGraceTimer = groundedGraceDuration;
	}
	else if (_groundedGraceTimer > 0.0f)
	{
		_groundedGraceTimer -= deltaTime;
		if (_groundedGraceTimer < 0.0f)
		{
			_groundedGraceTimer = 0.0f;
		}
	}
	

	const glm::vec3 currentPlayerWorldPosition = _characterController->getCurrentSyncedWorldPosition();
	const glm::vec3 previousPlayerWorldPosition = _characterController->getPreviousSyncedWorldPosition();
	const glm::vec3 playerDelta = currentPlayerWorldPosition - previousPlayerWorldPosition;
	

	// Mouse look and click
	glm::vec2 mouseDelta = engine::Input::getMouseDelta();
	if (invertMouseMove) mouseDelta.y *= -1.0f;

	//if (engine::Input::isMouseDown(GLFW_MOUSE_BUTTON_LEFT))
	{
		_yaw -= mouseDelta.x * sensitivity;
		_pitch += mouseDelta.y * sensitivity;
		_pitch = glm::clamp(_pitch, -89.0f, 89.0f);
	}

	glm::quat yawQuat = glm::angleAxis(glm::radians(_yaw), glm::vec3(0.0f, 1.0f, 0.0f));

	// WASD movement
	glm::vec3 input(0.0f);
	if (engine::Input::isKeyDown(GLFW_KEY_W)) input.z += 1.0f;
	if (engine::Input::isKeyDown(GLFW_KEY_S)) input.z -= 1.0f;
	if (engine::Input::isKeyDown(GLFW_KEY_A)) input.x -= 1.0f;
	if (engine::Input::isKeyDown(GLFW_KEY_D)) input.x += 1.0f;
	bool isMoving = input.x != 0.0f || input.z != 0.0f;
	bool isSprinting = engine::Input::isKeyDown(GLFW_KEY_LEFT_SHIFT);
	const bool isFastMovement = isSprinting || moveSpeed > (baseMoveSpeed * 1.01f);

	if (animator && !hasJumped)
	{
		Handle<engine::AnimationClip> desired = {};
		if (isMoving && sprintClip.valid()) desired = sprintClip;
		else if (idleClip.valid()) desired = idleClip;

		// Set playback speed for sprinting
		if (desired.valid())
		{
			if (isMoving && isSprinting)
			{
				animator->playbackSpeed = sprintPlaybackSpeed;
			}
			else
			{
				animator->playbackSpeed = 1.0f;
			}

		}

		// Only switch if desired clip matches neither current clip nor active blend target.
		const bool currentMatches = animator->clip.valid() && animator->clip.index == desired.index;
		const bool targetMatches = animator->isBlending && animator->targetClip.valid() && animator->targetClip.index == desired.index;
		if (desired.valid() && !currentMatches && !targetMatches)
		{
			animator->play(desired, true, 0.0f, locomotionCrossfade);
		}
	}

	if (groundedNow)
	{
		hasJumped = false;
	}
	else if (_groundedGraceTimer <= 0.0f)
	{
		_groundCarrier = nullptr;
		_lastGroundCarrierWorldPosition = glm::vec3(0.0f);
	}

	if (engine::Input::isKeyPressed(GLFW_KEY_SPACE))
	{
		_jumpBufferTimer = jumpBufferDuration;
	}

	if (_jumpBufferTimer > 0.0f && !hasJumped && _groundedGraceTimer > 0.0f)
	{
		hasJumped = true;
		_jumpBufferTimer = 0.0f;
		_groundedGraceTimer = 0.0f;
		if (_audio)
		{
			_audio->stopLoopingEffect();
			_activeLoopingSoundPath.clear();
		}
		const float effectiveMass = glm::max(_characterController->mass, 0.001f);
		_characterController->jump(glm::vec3(0.0f, jumpForce / effectiveMass, 0.0f));
		if (_audio && !jumpSoundPath.empty())
		{
			_audio->playOneShot(jumpSoundPath);
		}

		if (animator && jumpClip.valid())
		{
			animator->playbackSpeed = 1.0f;
			// play jump animation when jumping; start from beginning and don't loop
			// play jump with a short crossfade from current animation
			animator->play(jumpClip, false, 0.0f, jumpCrossfade);
			// std::cout << "Jump initiated! Playing jump animation.\n";
		}
	}

	if (_audio)
	{
		const bool wantsLoop = isMoving && !hasJumped;
		std::string desiredLoopPath;
		if (wantsLoop)
		{
			desiredLoopPath = isFastMovement && !runFastSoundPath.empty() ? runFastSoundPath : runSoundPath;
		}

		if (desiredLoopPath != _activeLoopingSoundPath)
		{
			if (desiredLoopPath.empty())
			{
				_audio->stopLoopingEffect();
				_activeLoopingSoundPath.clear();
			}
			else if (_audio->playLoopingEffect(desiredLoopPath, true))
			{
				_activeLoopingSoundPath = desiredLoopPath;
			}
		}
	}
	
	// Normalize input and transform to camera space
	glm::vec3 walkDir = glm::vec3(0.0f);
	if (input != glm::vec3(0.0f))
	{
		input = glm::normalize(glm::vec3(input.x, 0.0f, input.z));
		walkDir = input.x * (yawQuat * glm::vec3(-1.0f, 0.0f, 0.0f)) +
			input.z * (yawQuat * glm::vec3(0.0f, 0.0f, 1.0f));
		
		walkDir.y = 0.0f;
		if (walkDir != glm::vec3(0.0f))
		{
			walkDir = glm::normalize(walkDir);
		}

		// Rotate player to face movement direction (smoothly)
		glm::vec3 desiredFacing = -walkDir;
		if (glm::length(desiredFacing) < 1e-6f)
		{
			// fallback to camera forward if movement direction is degenerate
			desiredFacing = yawQuat * glm::vec3(0.0f, 0.0f, -1.0f);
		}
		desiredFacing.y = 0.0f;
		desiredFacing = glm::normalize(desiredFacing);

		glm::quat targetQuat = glm::quatLookAt(desiredFacing, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat currentQuat = owner->transform.getRotation();

		// Interpolate rotation using rotationSpeed parameter (frame-rate independent)
		const float slerpSpeed = 8.0f;
		float t = glm::clamp(rotationSpeed * slerpSpeed * deltaTime, 0.0f, 1.0f);
		owner->transform.setRotation(glm::slerp(currentQuat, targetQuat, t));
	}
	
	// Apply horizontal movement through character controller
	float speedFactor = (isMoving && isSprinting) ? 1.0f : walkSpeedFactor;
	glm::vec3 desiredVelocity = walkDir * moveSpeed * speedFactor;
	if ((groundedNow || _groundedGraceTimer > 0.0f) && platformCarryFactor > 0.0f)
	{
		engine::Object* groundCarrier = findGroundCarrier(*owner, *_characterController, _groundCarrier);
		glm::vec3 carried(0.0f);
		if (groundCarrier)
		{
			if (auto* rigidBody = groundCarrier->getComponent<engine::RigidBody>())
			{
				carried = rigidBody->getLastFrameDisplacementWorld();
			}
			else
			{
				const glm::vec3 carrierWorldPosition = groundCarrier->transform.getWorldPosition();
				if (_groundCarrier == groundCarrier)
				{
					carried = carrierWorldPosition - _lastGroundCarrierWorldPosition;
				}
				_lastGroundCarrierWorldPosition = carrierWorldPosition;
			}

			_groundCarrier = groundCarrier;
			_pendingCarrierDelta = carried;
			_pendingCarrierName = groundCarrier->name;
		}
		else
		{
			_groundCarrier = nullptr;
			_pendingCarrierDelta = glm::vec3(0.0f);
			_pendingCarrierName.clear();
		}

		_characterController->moveWorldOffset(carried * platformCarryFactor);
	}

	_characterController->move(desiredVelocity);
}

void PlayerController::postPhysicsUpdate(float deltaTime)
{
	if (!enabled)
	{
		return;
	}

	const glm::vec3 previousPlayerWorldPosition = _characterController->getPreviousSyncedWorldPosition();

	const glm::vec3 finalPlayerWorldPosition = _characterController->getCurrentSyncedWorldPosition();
	const glm::vec3 playerDelta = finalPlayerWorldPosition - previousPlayerWorldPosition;



	// Orbit the camera after the player transform has been finalized for this frame.
	const glm::vec3 focus = owner->transform.getPosition() + glm::vec3(0.0f, eyeHeight, 0.0f);
	const glm::quat yawQuat = glm::angleAxis(glm::radians(_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::quat pitchQuat = glm::angleAxis(glm::radians(_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
	const glm::vec3 cameraOffset = yawQuat * pitchQuat * glm::vec3(0.0f, eyeHeight, -cameraDistance);
	cameraTransform->setPosition(focus + cameraOffset);
	cameraTransform->lookAt(focus);
}

void PlayerController::activateSpeedBoost()
{
    speedBoostTimer = speedBoostDuration;
    moveSpeed = baseMoveSpeed * speedBoostMultiplier;
}

void PlayerController::activateJumpBoost()
{
    jumpBoostTimer = jumpBoostDuration;
    jumpForce = baseJumpForce * jumpBoostMultiplier;
}
