#pragma once

#include <glm/glm.hpp>

#include "scene/components/Component.h"

namespace engine
{
    class AnimatedVelocity : public Component
    {
    public:
        enum class WaveMode
        {
            Constant = 0,
            Sine = 1,
            Triangle = 2,
        };

        bool enabled = true;
        bool useLocalSpace = false;
        glm::vec3 savedPosition = glm::vec3(0.0f);

        glm::vec3 linearBase = glm::vec3(0.0f);
        glm::vec3 linearAmplitude = glm::vec3(0.0f);

        // Degrees per second.
        glm::vec3 angularBase = glm::vec3(0.0f);
        glm::vec3 angularAmplitude = glm::vec3(0.0f);

        WaveMode waveMode = WaveMode::Sine;
        float frequency = 1.0f;
        float phase = 0.0f;
        float timeScale = 1.0f;

        void update(float deltaTime) override;
        glm::vec3 getLastLinearVelocityWorld() const { return _lastLinearVelocityWorld; }
        glm::vec3 getLastFrameDisplacementWorld() const { return _lastFrameDisplacementWorld; }

    private:
        float _elapsed = 0.0f;
        glm::vec3 _lastLinearVelocityWorld = glm::vec3(0.0f);
        glm::vec3 _lastFrameDisplacementWorld = glm::vec3(0.0f);
        static float evalWave(WaveMode mode, float t, float frequency, float phase);
    };
}
