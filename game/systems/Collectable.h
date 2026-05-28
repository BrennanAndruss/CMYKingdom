#pragma once

#include "scene/components/Component.h"
#include "resources/Handle.h"

namespace engine
{
	struct Material;
}

class Collectable : public engine::Component
{
public:
	enum class Type
	{
		Cyan = 0,
		Magenta = 1,
		Yellow = 2,
		speedBoost = 3,
		JumpBoost = 4
	};

	Handle<engine::Material> defaultMat;
	Handle<engine::Material> collectedMat;
	bool isCollected = false;
	Type type = Type::Cyan;

	void start() override;
	void update(float deltaTime) override;
	void onCollected();

private:
	void tryRegisterCallback();
	bool _callbackRegistered = false;
};