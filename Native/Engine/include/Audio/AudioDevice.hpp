#pragma once

#include <expected>
#include <span>
#include <string>

#include "Core/Types.hpp"

struct SDL_AudioStream;

namespace VS::Audio {

// Output device. The SPU-semantics mixer (voices, ADSR, reverb) and the XA
// streamer feed interleaved stereo s16 frames here (plan §6.3).
class AudioDevice {
 public:
  static constexpr i32 SampleRate = 44100;
  static constexpr i32 Channels = 2;

  AudioDevice() = default;
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  std::expected<void, std::string> Open();
  void Close();

  void Submit(std::span<const i16> interleaved_frames);
  i32 QueuedBytes() const;
  bool IsOpen() const { return m_stream_ != nullptr; }

  // Another s16 source mixed into this device at its own rate (a movie
  // soundtrack, a streamed track); SDL resamples it. Null if the device is
  // closed. Release with CloseSource.
  SDL_AudioStream* OpenSource(i32 sample_rate, i32 channels);
  static void CloseSource(SDL_AudioStream* source);

  // The first device opened (the desktop host's), for engine systems that play
  // sound on their own.
  static AudioDevice* Primary();

 private:
  SDL_AudioStream* m_stream_ = nullptr;
  u32 m_device_ = 0;  // SDL_AudioDeviceID
};

}  // namespace VS::Audio
