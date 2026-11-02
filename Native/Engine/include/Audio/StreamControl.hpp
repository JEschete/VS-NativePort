#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include "Content/AudioStream.hpp"

// The cutscene stream commands of the sound driver (Sound_Cmd_9E/9F,
// IRQCallbackProc through func_8001D7A8, SLUS_010.40/25AC.c:3786-3800 and
// 6238-6560). The stream plays on a pair of adjacent SPU voices that
// StreamHost owns; the commands validate their packed pan, volume and fade
// arguments before any state changes.
namespace VS::Audio::Cutscene {
inline constexpr std::uint32_t kMaximumStreamVolume = 0x7F00;
inline constexpr std::uint32_t kMaximumFadeTicks = 0x7FFF;
// The pan byte travels in bits 8-15 of the command's pan word.
inline constexpr std::uint32_t kMaximumPackedPan = 0xFF00;
inline constexpr std::size_t kStreamChannelCount = 2;
inline constexpr std::size_t kPanGainCount = 256;
// The highest first voice of a pair that stays within the 24 SPU voices.
inline constexpr std::int32_t kLastPairVoice = 22;

// The left and right volume of the pair's first voice, then of its second.
using StreamGains = std::array<std::uint32_t, 2 * kStreamChannelCount>;

struct StreamVoices {
  std::int32_t first = 0;
  std::uint32_t mask = 0;  // zero while no stream plays
};

struct StreamMix {
  std::uint32_t volume = kMaximumStreamVolume;
  std::uint32_t pan = 0;  // packed pan word
  bool stereo_source = false;
  bool mono_output = false;
};

// The identity of a stream's source buffer as a stream command names it. It
// is only compared with the bound stream's identity and never dereferenced.
enum class StreamSourceId : std::uintptr_t {};

// The bound stream's channel layout and pitch.
struct StreamFormat {
  std::uint32_t channels = 0;
  bool repeat = false;
  std::uint32_t source_pitch = 0;
  std::uint32_t sample_rate = 0;
};

// What StartStream records for a stream before its voices start: the
// reserved pair, packed pan, format flags (bit 0 stereo, bit 1 repeat), pitch
// register value and reverb routing.
struct StreamStart {
  StreamVoices voices;
  std::uint32_t pan = 0;
  std::uint32_t format_flags = 0;
  std::uint32_t sample_rate = 0;
  bool reverb = false;
};

struct StreamFade {
  std::int32_t step = 0;
  std::uint32_t ticks = 0;
  std::uint32_t target = 0;
};

enum class StreamStatus : std::uint8_t {
  kApplied,
  kUnbound,
  kInvalidPan,
  kInvalidFormat,
  kNoVoicePair,
  kVoiceFailed,
  kInvalidVolume,
  kInvalidFade,
};

class StreamHost {
 public:
  virtual ~StreamHost() = default;
  [[nodiscard]] virtual StreamVoices Voices() const = 0;
  [[nodiscard]] virtual StreamMix Mix() const = 0;
  [[nodiscard]] virtual std::span<const std::int16_t, kPanGainCount> PanGains()
      const = 0;
  // The format of the bound stream whose source is `source`, or nullopt when
  // that source is not the bound stream.
  [[nodiscard]] virtual std::optional<StreamFormat> BoundFormat(
      StreamSourceId source) const = 0;
  // The first voice of a free pair, or nullopt when none is free.
  virtual std::optional<std::int32_t> FindFreeVoicePair() = 0;
  virtual void PauseVoice(std::int32_t voice, bool paused) = 0;
  virtual void KeyOffVoices(std::uint32_t mask) = 0;
  // Ends the pair's reservation: no stream voices, no stream reverb, and
  // voice modes marked for the next driver update.
  virtual void ReleaseVoices(std::uint32_t mask) = 0;
  // Records `start` and reserves its pair from the sequencer.
  virtual void BeginStream(const StreamStart& start) = 0;
  virtual bool StartVoice(std::size_t channel, std::int32_t voice, float rate,
                          std::uint32_t left, std::uint32_t right,
                          bool reverb) = 0;
  virtual void SetVoiceGains(std::int32_t voice, std::uint32_t left,
                             std::uint32_t right) = 0;
  // The gains last written to the pair, if any.
  [[nodiscard]] virtual std::optional<StreamGains> AppliedGains() const = 0;
  virtual void SetAppliedGains(const std::optional<StreamGains>& gains) = 0;
  // Sets the volume and cancels any fade.
  virtual void SetVolume(std::uint32_t volume) = 0;
  virtual void StartFade(const StreamFade& fade) = 0;
  virtual void SetPan(std::uint32_t pan) = 0;
  // The room stream slot (Audio_soundState slot 2).
  [[nodiscard]] virtual bool SlotBound() const = 0;
  virtual std::expected<void, std::string> ValidateAssets(
      const Content::AudioStreamInfo& info) = 0;
  virtual void BindSlot(const Content::AudioStreamInfo& info) = 0;
  virtual void UnbindSlot() = 0;
};

// The pair's gains for `mix`: the centre pan gain on every output in mono, a
// stereo source's channels on their own sides at half volume, or a mono
// source panned on both voices.
[[nodiscard]] StreamGains MixStreamGains(
    const StreamMix& mix,
    std::span<const std::int16_t, kPanGainCount> pan_gains);
// Writes the playing pair's gains when they changed; true when written.
bool ApplyStreamGains(StreamHost& host);

// Each returns whether a stream was playing.
bool PauseStream(StreamHost& host);
bool ResumeStream(StreamHost& host);
// Each returns the voice mask it stopped, or 0 when no stream played.
std::uint32_t StopStreamVoices(StreamHost& host);
std::uint32_t StopStream(StreamHost& host);
StreamStatus StartStream(StreamHost& host, StreamSourceId source,
                         std::uint32_t pan, bool reverb);
StreamStatus ReplaceStream(StreamHost& host, StreamSourceId source,
                           std::uint32_t pan, bool reverb);
StreamStatus SetStreamVolume(StreamHost& host, std::uint32_t volume);
StreamStatus SlideStreamVolume(StreamHost& host, std::uint32_t ticks,
                               std::uint32_t target);
StreamStatus SetStreamPan(StreamHost& host, std::uint32_t pan);
}  // namespace VS::Audio::Cutscene

namespace VS::Audio {
// The handle the room stream slot returns: slot index 2 plus one.
inline constexpr int kRoomStreamHandle = 3;

enum class RoomStreamStatus : std::uint8_t {
  kBound,
  kSlotOccupied,
  kMissingData,
  kInvalidStream,
  kMissingAssets,
};

struct RoomStreamBinding {
  int handle = 0;  // kRoomStreamHandle once bound, otherwise 0
  RoomStreamStatus status = RoomStreamStatus::kMissingData;
  std::string error;
};

// Binds the AKAO stream in `bytes` to the empty room stream slot.
RoomStreamBinding BindRoomCutsceneStream(Cutscene::StreamHost& host,
                                         std::span<const std::uint8_t> bytes);
// Stops and unbinds the room stream; 1 when one was bound, otherwise 0.
int UnbindRoomCutsceneStream(Cutscene::StreamHost& host);
}  // namespace VS::Audio
