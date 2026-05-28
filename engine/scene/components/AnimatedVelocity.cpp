#include "scene/components/AnimatedVelocity.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "scene/Object.h"
#include "scene/components/RigidBody.h"
#include "scene/Transform.h"

namespace engine
{
    namespace
    {
        glm::vec3 toWorldVector(const Transform& transform, const glm::vec3& localVector)
        {
            const glm::vec3 right = transform.getRight();
            const glm::vec3 up = transform.getUp();
            const glm::vec3 forward = transform.getForward();
            return (right * localVector.x) + (up * localVector.y) + (forward * localVector.z);
        }
    }

    float AnimatedVelocity::evalWave(WaveMode mode, float t, float frequency, float phase)
    {
        const float omegaT = glm::two_pi<float>() * frequency * t + phase;
        switch (mode)
        {
        case WaveMode::Constant:
            return 0.0f;
        case WaveMode::Sine:
            return std::sin(omegaT);
        case WaveMode::Triangle:
            return (2.0f / glm::pi<float>()) * std::asin(std::sin(omegaT));
        default:
            return 0.0f;
        }
    }

    void AnimatedVelocity::update(float deltaTime)
    {
        if (!enabled || !owner)
        {
            _lastLinearVelocityWorld = glm::vec3(0.0f);
            _lastFrameDisplacementWorld = glm::vec3(0.0f);
            return;
        }

		if (auto* rigidBody = owner->getComponent<RigidBody>())
		{
			if (rigidBody->getBodyType() != RigidBody::BodyType::Kinematic)
			{
				rigidBody->setBodyType(RigidBody::BodyType::Kinematic);
			}
		}

        const float safeFrequency = std::max(0.0f, frequency);
        const float waveValue = evalWave(waveMode, _elapsed, safeFrequency, phase);

        glm::vec3 linearVelocity = linearBase + (linearAmplitude * waveValue);
        glm::vec3 angularVelocity = angularBase + (angularAmplitude * waveValue);

        if (useLocalSpace)
        {
            linearVelocity = toWorldVector(owner->transform, linearVelocity);
        }

        _lastLinearVelocityWorld = linearVelocity;
        _lastFrameDisplacementWorld = linearVelocity * deltaTime;

        owner->transform.translate(_lastFrameDisplacementWorld);

        if (glm::dot(angularVelocity, angularVelocity) > 0.0f)
        {
            owner->transform.rotate(angularVelocity * deltaTime);
        }

        _elapsed += deltaTime * timeScale;
    }
}
