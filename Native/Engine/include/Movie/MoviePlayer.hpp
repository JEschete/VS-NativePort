#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Core/Types.hpp"

namespace VS::Movie {

enum class ClockSource : std::uint8_t {
  kAudioSamples,
  kLogicalTicks,
  kRealtimeFallback,
};

struct VideoFrameView {
  std::span<const u32> rgba;
  i32 width = 0;
  i32 height = 0;
  i32 stride_pixels = 0;
  std::int64_t presentation_time_ns = 0;
};

struct AudioOutput {
  i32 sample_rate = 0;
  i32 channels = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t submitted_frames = 0;
  std::uint64_t consumed_frames = 0;
  std::uint64_t queued_frames = 0;
  bool audible = false;
};

struct PlaybackFrame {
  VideoFrameView video;
  AudioOutput audio;
  ClockSource clock_source = ClockSource::kLogicalTicks;
  std::int64_t presentation_time_ns = 0;
  bool video_finished = false;
};

class PlaybackSession {
 public:
  static std::expected<PlaybackSession, std::string> Open(
      std::string_view disc_path);

  ~PlaybackSession();
  PlaybackSession(PlaybackSession&&) noexcept;
  PlaybackSession& operator=(PlaybackSession&&) noexcept;
  PlaybackSession(const PlaybackSession&) = delete;
  PlaybackSession& operator=(const PlaybackSession&) = delete;

  // Advances bounded audio buffering and decodes only video due at the
  // selected clock. The returned frame remains valid until the next call.
  std::expected<PlaybackFrame, std::string> Update(std::uint32_t elapsed_ticks);
  // Maintains the audio source without decoding or presenting video.
  std::expected<AudioOutput, std::string> PumpAudio(
      std::uint32_t elapsed_ticks);
  void Close();

 private:
  struct Impl;
  explicit PlaybackSession(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> m_impl_;
};

}  // namespace VS::Movie
