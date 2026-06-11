#pragma once

#include <memory>
#include <string>

#include <glm/vec3.hpp>

namespace engine
{
	struct AudioClip;
}

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
        bool playMusic(const AudioClip& clip, bool loop = true);
        void stopMusic();
        void setMusicVolume(float volume);

        bool playLoopingEffect(const std::string& filePath, bool loop = true);
        bool playLoopingEffect(const AudioClip& clip, bool loop = true);
        void stopLoopingEffect();
        void setLoopingEffectVolume(float volume);

        bool playOneShot(const std::string& filePath);
        bool playOneShot(const AudioClip& clip);
        bool preloadOneShot(const std::string& filePath);
        bool preloadOneShot(const AudioClip& clip);
        bool playPreloadedOneShot(const std::string& filePath);
        bool playPreloadedOneShot(const AudioClip& clip);
        void updateListener(const glm::vec3& camPos, const glm::vec3& camFront, const glm::vec3& camUp);

        bool isInitialized() const { return _impl != nullptr; }

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
