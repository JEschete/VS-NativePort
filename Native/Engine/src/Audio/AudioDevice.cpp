#include "Audio/AudioDevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include "Audio/VoiceEngine.hpp"
#include "Audio/XaPlayback.h"
#include "Core/Log.hpp"

namespace VS::Audio {

namespace {
AudioDevice* g_primary = nullptr;
}

AudioDevice::~AudioDevice() { Close(); }

AudioDevice* AudioDevice::Primary() { return g_primary; }

std::expected<void, std::string> AudioDevice::Open() {
  if (IsOpen()) {
    return std::unexpected("audio device is already open");
  }
  SDL_AudioSpec spec{SDL_AUDIO_F32, Channels, SampleRate};
  // a device opened directly (not SDL_OpenAudioDeviceStream) takes more than
  // one bound stream: OpenSource
  m_device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
  if (m_device_ == 0) {
    return std::unexpected(std::string("SDL_OpenAudioDevice failed: ") +
                           SDL_GetError());
  }
  m_stream_ = SDL_CreateAudioStream(&spec, &spec);
  if (!m_stream_) {
    const std::string error = SDL_GetError();
    Close();
    return std::unexpected("audio stream: " + error);
  }
  if (g_primary == nullptr) {
    g_primary = this;
  }
  m_starved_requests_ = 0;
  m_missing_bytes_ = 0;
  m_rebuffer_ = false;
  m_last_submit_ns_ = m_max_submit_gap_ns_ = 0;
  m_last_report_ms_ = SDL_GetTicks();
  if (!SDL_SetAudioStreamGetCallback(
          m_stream_,
          [](void* context, SDL_AudioStream*, int additional, int) {
            static_cast<AudioDevice*>(context)->Supply(additional);
          },
          this)) {
    const std::string error = SDL_GetError();
    Close();
    return std::unexpected("audio queue observation: " + error);
  }
  if (!Prime() || !SDL_BindAudioStream(m_device_, m_stream_)) {
    const std::string error = SDL_GetError();
    Close();
    return std::unexpected("buffered audio stream: " + error);
  }
  if (const char* capture = std::getenv("VS_AUDIO_CAPTURE")) {
    m_capture_ = SDL_IOFromFile(capture, "wb");
    if (!m_capture_) {
      Close();
      return std::unexpected("cannot open native mix capture: " +
                             std::string(capture));
    }
    m_capture_pending_ = std::make_unique<CaptureBuffer>();
    m_capture_drain_ = std::make_unique<CaptureBuffer>();
    m_capture_enabled_ = true;
    VS_LOG_INFO("audio", "native mix capture: {} (f32le, {} Hz, {} ch)",
                capture, SampleRate, Channels);
  }
  VS_LOG_INFO("audio", "output {} Hz, {} ch", SampleRate, Channels);
  return {};
}

void AudioDevice::Close() {
  if (g_primary == this) Audio_Native_StopXaStream();
  if (m_stream_) {
    if (!SDL_SetAudioStreamGetCallback(m_stream_, nullptr, nullptr)) {
      Core::LogWrite(Core::LogLevel::Error, "audio",
                     "stopping native renderer failed");
    }
    DrainCapture();
    SDL_DestroyAudioStream(m_stream_);
    m_stream_ = nullptr;
  }
  CloseCapture();
  m_engine_ = nullptr;
  m_capture_pending_.reset();
  m_capture_drain_.reset();
  m_capture_samples_ = 0;
  m_rendered_frames_ = 0;
  m_max_callback_ns_ = 0;
  m_capture_overflow_ = false;
  m_callback_failed_ = false;
  if (m_device_ != 0) {
    SDL_CloseAudioDevice(m_device_);
    m_device_ = 0;
  }
  if (g_primary == this) {
    g_primary = nullptr;
  }
}

void AudioDevice::CloseCapture() {
  m_capture_enabled_ = false;
  if (m_capture_ && !SDL_CloseIO(m_capture_)) {
    Core::LogWrite(Core::LogLevel::Error, "audio",
                   "native mix capture could not be finalized");
  }
  m_capture_ = nullptr;
}

std::expected<void, std::string> AudioDevice::RenderFrom(VoiceEngine& engine) {
  if (!m_stream_) return std::unexpected("native audio stream is not open");
  if (!m_realtime_) {
    return std::unexpected("accelerated playback uses sequencer-driven PCM");
  }
  if (m_engine_ == &engine) return {};
  if (m_engine_) return std::unexpected("native renderer is already attached");
  if (!SDL_LockAudioStream(m_stream_)) {
    return std::unexpected(SDL_GetError());
  }
  m_engine_ = &engine;
  m_rebuffer_ = false;
  if (!SDL_UnlockAudioStream(m_stream_)) {
    return std::unexpected(SDL_GetError());
  }
  return {};
}

void AudioDevice::Supply(int additional) {
  if (additional <= 0) return;
  const auto started = SDL_GetTicksNS();
  if (m_engine_) {
    std::array<float, VoiceEngine::kFramesPerTick * Channels> frames{};
    while (additional > 0) {
      m_engine_->Render(frames);
      if (!SDL_PutAudioStreamData(m_stream_, frames.data(), sizeof(frames))) {
        m_callback_failed_ = true;
        break;
      }
      m_rendered_frames_.fetch_add(VoiceEngine::kFramesPerTick,
                                   std::memory_order_relaxed);
      if (m_capture_enabled_.load(std::memory_order_relaxed)) {
        if (frames.size() <= kCaptureSamples - m_capture_samples_) {
          std::memcpy(m_capture_pending_->data() + m_capture_samples_,
                      frames.data(), sizeof(frames));
          m_capture_samples_ += frames.size();
        } else {
          m_capture_overflow_ = true;
        }
      }
      additional -= static_cast<int>(sizeof(frames));
    }
    const auto duration = SDL_GetTicksNS() - started;
    auto maximum = m_max_callback_ns_.load(std::memory_order_relaxed);
    while (duration > maximum &&
           !m_max_callback_ns_.compare_exchange_weak(
               maximum, duration, std::memory_order_relaxed)) {
    }
  }
  if (additional > 0) {
    m_starved_requests_.fetch_add(1, std::memory_order_relaxed);
    m_missing_bytes_.fetch_add(static_cast<u64>(additional),
                               std::memory_order_relaxed);
    m_rebuffer_ = true;
  }
}

void AudioDevice::DrainCapture() {
  if (!m_capture_ || !m_stream_) return;
  if (m_capture_overflow_.exchange(false)) {
    Core::LogWrite(Core::LogLevel::Error, "audio",
                   "native mix capture exceeded its two-second backlog");
    CloseCapture();
    return;
  }
  if (!SDL_LockAudioStream(m_stream_)) {
    Core::LogWrite(Core::LogLevel::Error, "audio",
                   "locking native capture failed");
    return;
  }
  const auto bytes = m_capture_samples_ * sizeof(float);
  std::memcpy(m_capture_drain_->data(), m_capture_pending_->data(), bytes);
  m_capture_samples_ = 0;
  if (!SDL_UnlockAudioStream(m_stream_)) {
    Core::LogWrite(Core::LogLevel::Error, "audio",
                   "unlocking native capture failed");
    return;
  }
  if (SDL_WriteIO(m_capture_, m_capture_drain_->data(), bytes) != bytes) {
    Core::LogWrite(Core::LogLevel::Error, "audio",
                   "writing native mix capture failed");
    CloseCapture();
  }
}

void AudioDevice::Pump() {
  if (!m_stream_) return;
  ObserveSubmission();
  DrainCapture();
  if (m_callback_failed_.exchange(false)) {
    VS_LOG_ERROR("audio", "submitting callback-rendered native mix failed");
  }
}

SDL_AudioStream* AudioDevice::OpenSource(i32 sample_rate, i32 channels) {
  if (m_device_ == 0) {
    return nullptr;
  }
  const SDL_AudioSpec src{SDL_AUDIO_S16, channels, sample_rate};
  const SDL_AudioSpec dst{SDL_AUDIO_F32, Channels, SampleRate};
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

void AudioDevice::Clear() {
  if (!m_stream_) return;
  if (!SDL_LockAudioStream(m_stream_)) {
    VS_LOG_ERROR("audio", "locking native mix for reset: {}", SDL_GetError());
    return;
  }
  if (!SDL_ClearAudioStream(m_stream_)) {
    VS_LOG_ERROR("audio", "clearing native mix: {}", SDL_GetError());
  } else {
    m_rebuffer_ = false;
    Prime();
  }
  if (!SDL_UnlockAudioStream(m_stream_)) {
    VS_LOG_ERROR("audio", "unlocking native mix after reset: {}",
                 SDL_GetError());
  }
}

bool AudioDevice::Prime() {
  // Audio arrives in NTSC-field batches, not on the device thread.
  static constexpr std::array<float,
                              static_cast<usize>(kBufferFrames) * Channels>
      kSilence{};
  if (!SDL_PutAudioStreamData(m_stream_, kSilence.data(), sizeof(kSilence))) {
    VS_LOG_ERROR("audio", "priming native mix: {}", SDL_GetError());
    return false;
  }
  return true;
}

void AudioDevice::ObserveSubmission() {
  constexpr u64 kNanosecondsPerMillisecond = 1000000;
  constexpr u64 kReportMilliseconds = 1000;
  const u64 now = SDL_GetTicksNS();
  if (m_last_submit_ns_ != 0) {
    m_max_submit_gap_ns_ =
        std::max(m_max_submit_gap_ns_, now - m_last_submit_ns_);
  }
  m_last_submit_ns_ = now;
  const u64 milliseconds = now / kNanosecondsPerMillisecond;
  if (milliseconds - m_last_report_ms_ >= kReportMilliseconds) {
    const auto starved = m_starved_requests_.exchange(0);
    const auto missing = m_missing_bytes_.exchange(0);
    VS_LOG_DEBUG(
        "audio",
        "output queue: starved_requests={} missing_frames={} "
        "max_submit_gap_ms={} rendered_frames={} max_callback_ms={}",
        starved, missing / (Channels * sizeof(float)),
        static_cast<double>(m_max_submit_gap_ns_) / kNanosecondsPerMillisecond,
        m_rendered_frames_.load(std::memory_order_relaxed),
        static_cast<double>(m_max_callback_ns_.exchange(0)) /
            kNanosecondsPerMillisecond);
    m_max_submit_gap_ns_ = 0;
    m_last_report_ms_ = milliseconds;
  }
}

void AudioDevice::Submit(std::span<const float> interleaved_frames) {
  constexpr i32 kMaximumQueuedBytes = static_cast<i32>(
      static_cast<usize>(SampleRate) * Channels * sizeof(float) / 4);
  if (m_engine_) {
    VS_LOG_ERROR("audio", "cannot push PCM into a demand-rendered native mix");
    return;
  }
  if (interleaved_frames.size() % Channels != 0 ||
      interleaved_frames.size_bytes() > std::numeric_limits<int>::max()) {
    VS_LOG_ERROR("audio", "invalid native mix block: {} samples",
                 interleaved_frames.size());
    return;
  }
  if (m_capture_) {
    if (SDL_WriteIO(m_capture_, interleaved_frames.data(),
                    interleaved_frames.size_bytes()) !=
        interleaved_frames.size_bytes()) {
      VS_LOG_ERROR("audio", "writing native mix capture failed");
      CloseCapture();
    }
  }
  if (m_stream_ && !interleaved_frames.empty()) {
    ObserveSubmission();
    const auto queued = QueuedBytes();
    if (queued < 0) {
      VS_LOG_ERROR("audio", "querying native mix queue: {}", SDL_GetError());
      return;
    }
    if (m_rebuffer_.exchange(false)) {
      VS_LOG_WARN("audio",
                  "native mix starved; restoring 50 ms scheduling buffer");
      if (!Prime()) return;
    }
    if (queued > kMaximumQueuedBytes) {
      VS_LOG_WARN("audio",
                  "native mix exceeded 250 ms queue; discarding stale output");
      Clear();
    }
    if (!SDL_PutAudioStreamData(
            m_stream_, interleaved_frames.data(),
            static_cast<int>(interleaved_frames.size_bytes()))) {
      VS_LOG_ERROR("audio", "submitting native mix: {}", SDL_GetError());
    }
  }
}

i32 AudioDevice::QueuedBytes() const {
  return m_stream_ ? SDL_GetAudioStreamQueued(m_stream_) : 0;
}

}  // namespace VS::Audio
