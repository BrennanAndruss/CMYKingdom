#include "scene/components/Audio.h"
#include <iostream>
#include <utility>

#include "../../../external/miniaudio/miniaudio.h"

namespace engine
{
    struct AudioEngine::Impl
    {
        ma_engine engine{};
        ma_sound music{};
        bool initialized = false;
        bool musicLoaded = false;
    };
}




namespace engine
{
    AudioEngine::AudioEngine() : _impl(std::make_unique<Impl>()) {}

    AudioEngine::~AudioEngine()
    {
        shutdown();
    }
    

    bool AudioEngine::init()
    {
        if (_impl->initialized)
        {
            return true;
        }

        const ma_result result = ma_engine_init(nullptr, &_impl->engine);
        if (result != MA_SUCCESS)
        {
            std::cerr << "AudioEngine: failed to initialize engine (" << result << ")" << std::endl;
            return false;
        }

        _impl->initialized = true;
        return true;
    }

    void AudioEngine::shutdown()
    {
        if (!_impl || !_impl->initialized)
        {
            return;
        }

        stopMusic();
        ma_engine_uninit(&_impl->engine);
        _impl->initialized = false;
    }

    bool AudioEngine::playMusic(const std::string& filePath, bool loop)
    {
        if (!_impl || !_impl->initialized)
        {
            return false;
        }

        if (_impl->musicLoaded)
        {
            ma_sound_uninit(&_impl->music);
            _impl->musicLoaded = false;
        }

        const ma_uint32 flags = MA_SOUND_FLAG_STREAM;
        const ma_result result = ma_sound_init_from_file(&_impl->engine, filePath.c_str(), flags, nullptr, nullptr, &_impl->music);
        if (result != MA_SUCCESS)
        {
            std::cerr << "AudioEngine: failed to load music '" << filePath << "' (" << result << ")" << std::endl;
            return false;
        }

        ma_sound_set_looping(&_impl->music, loop ? MA_TRUE : MA_FALSE);
        if (ma_sound_start(&_impl->music) != MA_SUCCESS)
        {
            std::cerr << "AudioEngine: failed to start music '" << filePath << "'" << std::endl;
            ma_sound_uninit(&_impl->music);
            return false;
        }

        _impl->musicLoaded = true;
        return true;
    }

    void AudioEngine::stopMusic()
    {
        if (!_impl || !_impl->musicLoaded)
        {
            return;
        }

        ma_sound_stop(&_impl->music);
        ma_sound_uninit(&_impl->music);
        _impl->musicLoaded = false;
    }

    void AudioEngine::setMusicVolume(float volume)
    {
        if (_impl && _impl->musicLoaded)
        {
            ma_sound_set_volume(&_impl->music, volume);
        }
    }

    bool AudioEngine::playOneShot(const std::string& filePath)
    {
        if (!_impl || !_impl->initialized)
        {
            return false;
        }

        const ma_result result = ma_engine_play_sound(&_impl->engine, filePath.c_str(), nullptr);
        return result == MA_SUCCESS;
    }

    void AudioEngine::updateListener(const glm::vec3& camPos, const glm::vec3& camFront, const glm::vec3& camUp)
    {
        if (!_impl || !_impl->initialized)
        {
            return;
        }

        ma_engine_listener_set_position(&_impl->engine, 0, camPos.x, camPos.y, camPos.z);
        ma_engine_listener_set_direction(&_impl->engine, 0, camFront.x, camFront.y, camFront.z);
        ma_engine_listener_set_world_up(&_impl->engine, 0, camUp.x, camUp.y, camUp.z);
    }
}
