#include "Audio/AudioDevice.hpp"

#include <SDL3/SDL.h>

#include <string>

#include "Core/Log.hpp"

namespace VS::Audio {

namespace {
AudioDevice* g_primary = nullptr;
}

AudioDevice::~AudioDevice() { Close(); }

AudioDevice* AudioDevice::Primary() { return g_primary; }

std::expected<void, std::string> AudioDevice::Open() {
  SDL_AudioSpec spec{SDL_AUDIO_S16, Channels, SampleRate};
  // a device opened directly (not SDL_OpenAudioDeviceStream) takes more than
  // one bound stream: OpenSource
  m_device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
  if (m_device_ == 0) {
    return std::unexpected(std::string("SDL_OpenAudioDevice failed: ") +
                           SDL_GetError());
  }
  m_stream_ = SDL_CreateAudioStream(&spec, &spec);
  if (!m_stream_ || !SDL_BindAudioStream(m_device_, m_stream_)) {
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

void AudioDevice::Close() {
  if (m_stream_) {
    SDL_DestroyAudioStream(m_stream_);
    m_stream_ = nullptr;
  }
  if (m_device_ != 0) {
    SDL_CloseAudioDevice(m_device_);
    m_device_ = 0;
  }
  if (g_primary == this) {
    g_primary = nullptr;
  }
}

SDL_AudioStream* AudioDevice::OpenSource(i32 sample_rate, i32 channels) {
  if (m_device_ == 0) {
    return nullptr;
  }
  const SDL_AudioSpec src{SDL_AUDIO_S16, channels, sample_rate};
  const SDL_AudioSpec dst{SDL_AUDIO_S16, Channels, SampleRate};
  SDL_AudioStream* s = SDL_CreateAudioStream(&src, &dst);
  if (s == nullptr || !SDL_BindAudioStream(m_device_, s)) {
    VS_LOG_ERROR("audio", "source {} Hz {} ch: {}", sample_rate, channels,
                 SDL_GetError());
    SDL_DestroyAudioStream(s);
    return nullptr;
  }
  return s;
}

void AudioDevice::CloseSource(SDL_AudioStream* source) {
  if (source != nullptr) {
    SDL_DestroyAudioStream(source);  // also unbinds it
  }
}

void AudioDevice::Submit(std::span<const i16> interleaved_frames) {
  if (m_stream_ && !interleaved_frames.empty()) {
    SDL_PutAudioStreamData(m_stream_, interleaved_frames.data(),
                           static_cast<int>(interleaved_frames.size_bytes()));
  }
}

i32 AudioDevice::QueuedBytes() const {
  return m_stream_ ? SDL_GetAudioStreamQueued(m_stream_) : 0;
}

}  // namespace VS::Audio
