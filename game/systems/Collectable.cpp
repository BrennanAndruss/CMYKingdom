#include "systems/Collectable.h"

#include "physics/PhysicsSystem.h"
#include "scene/Scene.h"
#include "scene/Object.h"
#include "scene/components/RigidBody.h"
#include "scene/components/MeshRenderer.h"
#include "scene/components/Collider.h"
#include "systems/PlayerController.h"
#include "MyGame.h"

#include <iostream>

namespace
{
	btCollisionObject* getCollisionObjectForCollectable(engine::Object* owner)
	{
		auto* collider = owner->getComponent<engine::Collider>();
		if (collider && collider->getCollisionObject())
		{
			return collider->getCollisionObject();
		}

		auto* rigidBody = owner->getComponent<engine::RigidBody>();
		if (rigidBody && rigidBody->getBody())
		{
			return rigidBody->getBody();
		}

		return nullptr;
	}
}

void Collectable::tryRegisterCallback()
{
	if (_callbackRegistered || !owner)
	{
		return;
	}

	auto* collisionObject = getCollisionObjectForCollectable(owner);
	if (!collisionObject)
	{
		return;
	}

	auto* physics = owner->getScene()->getPhysicsSystem();
	if (!physics)
	{
		return;
	}

	physics->registerCallback(collisionObject, [this](btCollisionObject* other)
		{
			// The callback is attached to the collectable, so only accept the player object.
			auto* otherOwner = other ? static_cast<engine::Object*>(other->getUserPointer()) : nullptr;
			if (!otherOwner || !otherOwner->getComponent<PlayerController>())
			{
				return;
			}

			if (auto* playerController = otherOwner->getComponent<PlayerController>())
			{
				if (type == Type::speedBoost)
				{
					playerController->activateSpeedBoost();
				}
				else if (type == Type::JumpBoost)
				{
					playerController->activateJumpBoost();
				}
			}

			onCollected();
		});
	_callbackRegistered = true;
}

void Collectable::start()
{
	if (auto* meshRenderer = owner ? owner->getComponent<engine::MeshRenderer>() : nullptr)
	{
		meshRenderer->writeStencil = true;
	}

	tryRegisterCallback();
}

void Collectable::update(float deltaTime)
{
	(void)deltaTime;
	tryRegisterCallback();
}

void Collectable::onCollected()
{
    if (isCollected)
    {
        return;
    }

    isCollected = true;

    // Trigger color restoration only for color collectables
    if (auto* game = MyGame::getActiveGame())
    {
        if (type == Type::Cyan || type == Type::Magenta || type == Type::Yellow)
        {
            game->onCollectableCollected();
        }
    }

    owner->markedForDeletion = true;
}