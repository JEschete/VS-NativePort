#include "Audio/AudioDevice.hpp"

#include "Core/Log.hpp"

#include <SDL3/SDL.h>

namespace VS::Audio {

namespace {
AudioDevice* g_primary = nullptr;
}

AudioDevice::~AudioDevice() { Close(); }

AudioDevice* AudioDevice::Primary()
{
    return g_primary;
}

std::expected<void, std::string> AudioDevice::Open()
{
    SDL_AudioSpec spec{SDL_AUDIO_S16, Channels, SampleRate};
    // a device opened directly (not SDL_OpenAudioDeviceStream) takes more than one bound stream: OpenSource
    m_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (m_device == 0) {
        return std::unexpected(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
    }
    m_stream = SDL_CreateAudioStream(&spec, &spec);
    if (!m_stream || !SDL_BindAudioStream(m_device, m_stream)) {
        const std::string error = SDL_GetError();
        Close();
        return std::unexpected("audio stream: " + error);
    }
    if (g_primary == nullptr) {
        g_primary = this;
    }
    VS_LOG_INFO("audio", "output {} Hz, {} ch", SampleRate, Channels);
    return {};
}

void AudioDevice::Close()
{
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
    if (m_device != 0) {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }
    if (g_primary == this) {
        g_primary = nullptr;
    }
}

SDL_AudioStream* AudioDevice::OpenSource(i32 sampleRate, i32 channels)
{
    if (m_device == 0) {
        return nullptr;
    }
    const SDL_AudioSpec src{SDL_AUDIO_S16, channels, sampleRate};
    const SDL_AudioSpec dst{SDL_AUDIO_S16, Channels, SampleRate};
    SDL_AudioStream* s = SDL_CreateAudioStream(&src, &dst);
    if (s == nullptr || !SDL_BindAudioStream(m_device, s)) {
        VS_LOG_ERROR("audio", "source {} Hz {} ch: {}", sampleRate, channels, SDL_GetError());
        SDL_DestroyAudioStream(s);
        return nullptr;
    }
    return s;
}

void AudioDevice::CloseSource(SDL_AudioStream* source)
{
    if (source != nullptr) {
        SDL_DestroyAudioStream(source);  // also unbinds it
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
