#pragma once

#include <array>
#include <atomic>
#include <expected>
#include <memory>
#include <span>
#include <string>

#include "Core/Types.hpp"

struct SDL_AudioStream;
struct SDL_IOStream;

namespace VS::Audio {

class VoiceEngine;

// Native float mix; separate movie/source streams retain their own formats.
class AudioDevice {
 public:
  static constexpr i32 SampleRate = 48000;
  static constexpr i32 Channels = 2;
  static constexpr i32 kBufferFrames = SampleRate / 20;

  explicit AudioDevice(bool realtime = true) : m_realtime_(realtime) {}
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  std::expected<void, std::string> Open();
  void Close();

  void Submit(std::span<const float> interleaved_frames);
  // The engine must outlive this device's open stream.
  std::expected<void, std::string> RenderFrom(VoiceEngine& engine);
  void Pump();
  void Clear();
  i32 QueuedBytes() const;
  bool IsOpen() const { return m_stream_ != nullptr; }
  bool IsRealtime() const { return m_realtime_; }

  // Another s16 source mixed into this device at its own rate (a movie
  // soundtrack, a streamed track); SDL resamples it. Null if the device is
  // closed. Release with CloseSource.
  SDL_AudioStream* OpenSource(i32 sample_rate, i32 channels);
  static void CloseSource(SDL_AudioStream* source);

  // The first device opened (the desktop host's), for engine systems that play
  // sound on their own.
  static AudioDevice* Primary();

 private:
  void CloseCapture();
  bool Prime();
  void ObserveSubmission();
  void Supply(int additional);
  void DrainCapture();
  const bool m_realtime_;
  SDL_AudioStream* m_stream_ = nullptr;
  u32 m_device_ = 0;  // SDL_AudioDeviceID
  SDL_IOStream* m_capture_ = nullptr;
  VoiceEngine* m_engine_ = nullptr;
  static constexpr usize kCaptureSamples =
      static_cast<usize>(SampleRate) * Channels * 2;
  using CaptureBuffer = std::array<float, kCaptureSamples>;
  std::unique_ptr<CaptureBuffer> m_capture_pending_;
  std::unique_ptr<CaptureBuffer> m_capture_drain_;
  usize m_capture_samples_ = 0;
  std::atomic<bool> m_capture_enabled_{false};
  std::atomic<bool> m_capture_overflow_{false};
  std::atomic<bool> m_callback_failed_{false};
  std::atomic<u64> m_rendered_frames_{0};
  std::atomic<u64> m_max_callback_ns_{0};
  std::atomic<u64> m_starved_requests_{0};
  std::atomic<u64> m_missing_bytes_{0};
  std::atomic<bool> m_rebuffer_{false};
  u64 m_last_submit_ns_ = 0;
  u64 m_max_submit_gap_ns_ = 0;
  u64 m_last_report_ms_ = 0;
};

}  // namespace VS::Audio
