#include "vs/audio/AudioDevice.hpp"

#include "vs/core/Log.hpp"

#include <SDL3/SDL.h>

namespace VS::Audio {

AudioDevice::~AudioDevice() { Close(); }

std::expected<void, std::string> AudioDevice::Open()
{
    SDL_AudioSpec spec{SDL_AUDIO_S16, Channels, SampleRate};
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!m_stream) {
        return std::unexpected(std::string("SDL_OpenAudioDeviceStream failed: ") + SDL_GetError());
    }
    SDL_ResumeAudioStreamDevice(m_stream);
    VS_LOG_INFO("audio", "output {} Hz, {} ch", SampleRate, Channels);
    return {};
}

void AudioDevice::Close()
{
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
}

void AudioDevice::Submit(std::span<const i16> interleavedFrames)
{
    if (m_stream && !interleavedFrames.empty()) {
        SDL_PutAudioStreamData(m_stream, interleavedFrames.data(), static_cast<int>(interleavedFrames.size_bytes()));
    }
}

i32 AudioDevice::QueuedBytes() const
{
    return m_stream ? SDL_GetAudioStreamQueued(m_stream) : 0;
}

} // namespace VS::Audio
