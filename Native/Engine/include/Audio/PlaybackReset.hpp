#pragma once

#include <cstddef>
#include <cstdint>

// The sound driver's cold reset before its sequencer interrupt starts
// (func_8001369C). The reset sees the driver only through PlaybackResetHost, so
// it reaches no global sequencer, mixer or stream state.
namespace VS::Audio::Driver {
inline constexpr std::size_t kMusicChannelCount = 32;
inline constexpr std::size_t kSoundEffectChannelCount = 12;
// Sound-effect channels own the upper half of the 24 hardware voices.
inline constexpr std::uint32_t kFirstSoundEffectVoice = 12;
// The assignment a music channel holds when it owns no hardware voice.
inline constexpr std::uint32_t kNoVoice = 24;

enum class ChannelKind : std::uint8_t { kMusic = 0, kSoundEffect = 1 };

// One channel's cleared state: no pending updates, priority 0, the voice it
// owns, and for sound effects full volume with no volume or pitch slides.
struct ChannelReset {
  ChannelKind kind = ChannelKind::kMusic;
  std::uint32_t voice = kNoVoice;
  std::int16_t effect_volume = 0;
};

// The primary music layer becomes the active one; the secondary and saved
// layers, tempo and pitch scales, loops and channel mutes are cleared.
struct MusicLayerReset {
  std::int32_t master_volume = 0;  // 16.16
  std::int32_t reverb_depth = 0;   // 16.16
};

// The voice scheduler and its masks are cleared and the effect tempo restarts.
struct VoiceSchedulerReset {
  std::uint32_t effect_tempo = 0;  // 16.16
  std::uint32_t tempo_accumulator = 0;
};

// The driver latches, sample bank selections, CD level and XA stream volume.
struct DriverControlReset {
  std::uint32_t mix_behavior = 0;
  std::int32_t disc_volume = 0;  // 16.16
  std::int32_t stream_volume = 0;
};

struct PlaybackReset {
  MusicLayerReset music;
  VoiceSchedulerReset voices;
  DriverControlReset controls;
  // Deferred voice-mode update the first tick applies.
  std::uint32_t voice_mode_update = 0;
  int reverb_mode = 0;
};

// The values the original stores.
[[nodiscard]] PlaybackReset DefaultPlaybackReset();

// The channel reset for slot `index` of the music table, or of the
// sound-effect table when `kind` is kSoundEffect. Returns false for a slot
// outside the table.
[[nodiscard]] bool ChannelResetFor(ChannelKind kind, std::size_t index,
                                   ChannelReset& reset);

class PlaybackResetHost {
 public:
  virtual ~PlaybackResetHost() = default;
  // Resets the native voice engine and output before any driver state.
  virtual void ResetOutput() = 0;
  virtual void ResetMusicLayers(const MusicLayerReset& reset) = 0;
  virtual void ResetVoiceScheduler(const VoiceSchedulerReset& reset) = 0;
  virtual void ResetDriverControls(const DriverControlReset& reset) = 0;
  virtual void ResetChannel(std::size_t index, const ChannelReset& reset) = 0;
  virtual void RequestVoiceModeUpdate(std::uint32_t flags) = 0;
  virtual void SetReverbMode(int mode) = 0;
  virtual void EnableReverb(bool enabled) = 0;
};

// Resets the output, the music layers, voice scheduler and driver controls,
// the 32 music channels then the 12 sound-effect channels, requests the
// voice-mode update and selects and enables the default reverb.
void ResetPlaybackState(PlaybackResetHost& host);
}  // namespace VS::Audio::Driver
