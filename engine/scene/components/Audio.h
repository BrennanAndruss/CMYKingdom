#pragma once

#include <memory>
#include <string>

#include <glm/vec3.hpp>




namespace engine
{
    class AudioEngine
    {
    public:
        AudioEngine();
        ~AudioEngine();

        bool init();
        void shutdown();

        bool playMusic(const std::string& filePath, bool loop = true);
        void stopMusic();
        void setMusicVolume(float volume);

        bool playOneShot(const std::string& filePath);
        void updateListener(const glm::vec3& camPos, const glm::vec3& camFront, const glm::vec3& camUp);

        bool isInitialized() const { return _impl != nullptr; }

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
