#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"

// The voice mixer (Sound_UpdateSlidesAndDelays through func_80015970,
// SLUS_010.40/25AC.c:1400-2266): channel slides and LFO modulation, voice
// allocation and stealing, and the per-tick voice commit and release over
// typed copies of the music layers, their channels, the sound-effect
// channels and the voice scheduler. The voice output, the envelope levels it
// reports and the voice owner table go through MixerHost.
namespace VS::Audio::Mixer {
inline constexpr std::size_t kVoiceCount = Driver::kVoiceCount;
// A channel without a voice (and FindFreeVoice's "no voice").
inline constexpr std::uint32_t kNoVoice = Driver::kVoiceCount;
inline constexpr std::size_t kMusicChannelCount = Seq::kMusicChannelCount;
inline constexpr std::size_t kEffectChannelCount = Seq::kEffectChannelCount;
inline constexpr auto kFirstEffectVoiceBit = 1U << Seq::kFirstEffectVoice;
inline constexpr std::size_t kPanGainCount = 0x100;
inline constexpr std::size_t kCenterPan = 0x80;
// The envelope level that marks a voice in use (func_80015080).
inline constexpr std::int16_t kReservedEnvelope = 0x7FFF;
// Sound-effect tag that keeps the effect's volume, pan and pitch scale out
// of its mix (func_800147CC).
inline constexpr std::int32_t kFixedMixEffect = 0x2000000;
// Driver mix behaviour flag that mixes every voice to the centre.
inline constexpr std::uint32_t kMonoMix = 0x2;
// Music layer status flags func_80014D70 sets.
inline constexpr std::int32_t kVoiceUnavailable = 0x1;
inline constexpr std::int32_t kVoiceStolen = 0x2;
// Driver update requests the commit services.
inline constexpr std::uint32_t kReverbDepthUpdate = 0x80;

using PanGains = std::span<const std::int16_t, kPanGainCount>;

// A channel's position in an LFO waveform (FSoundChannel.vibratoWave and
// friends): `wave` is the waveform from its first sample through its loop
// marker (0, 0, offset), empty when the channel's pointer names no known
// waveform.
struct LfoCursor {
  std::span<const std::int16_t> wave;
  std::size_t position = 0;

  friend bool operator==(const LfoCursor& left, const LfoCursor& right) {
    return left.wave.data() == right.wave.data() &&
           left.wave.size() == right.wave.size() &&
           left.position == right.position;
  }
};

// The FSoundChannel fields the mixer reads and writes, at the original
// widths and signedness.
struct MixChannel {
  Seq::ChannelState state;
  Seq::PitchSlideState pitch_slide;
  std::uint32_t assigned_voice = kNoVoice;
  std::uint16_t sample_rate = 0;
  std::int16_t volume_left = 0;
  std::int16_t volume_right = 0;
  std::int32_t effect_tags = 0;
  std::uint16_t effect_pan = 0;
  std::int32_t effect_pitch_scale = 0;
  LfoCursor vibrato;
  LfoCursor tremolo;
  LfoCursor auto_pan;

  friend bool operator==(const MixChannel&, const MixChannel&) = default;
};

using MusicChannels = std::array<MixChannel, kMusicChannelCount>;

// A music layer (FSoundChannelConfig) and the channels it plays.
struct MusicLayer {
  Seq::MusicLayerState config;
  MusicChannels channels;

  friend bool operator==(const MusicLayer&, const MusicLayer&) = default;
};

// The primary layer plays g_ActiveMusicChannels; the secondary layer is the
// saved or crossfading track the driver plays beside it.
enum class LayerId : std::uint8_t { kPrimary, kSecondary };

// g_Sound_VoiceModeFlags.
struct VoiceModes {
  std::uint32_t reverb = 0;
  std::uint32_t noise = 0;
  std::uint32_t fm = 0;

  friend bool operator==(const VoiceModes&, const VoiceModes&) = default;
};

// The master fade value and the master volume the saved layer's fade
// replaced (g_Sound_MasterFadeTimer.value and .savedValue).
struct MasterFade {
  std::int32_t value = 0;
  std::int32_t saved_value = 0;

  friend bool operator==(const MasterFade&, const MasterFade&) = default;
};

// Everything the voice commit and release read or write.
struct MixerState {
  MusicLayer primary;
  std::optional<MusicLayer> secondary;
  // The layer g_pActiveMusicConfig names.
  LayerId active = LayerId::kPrimary;
  std::array<MixChannel, kEffectChannelCount> effects;
  Seq::VoiceSchedulerState scheduler;
  std::array<std::int16_t, kVoiceCount> envelopes{};
  VoiceModes voice_modes;
  // The cutscene stream's voices.
  std::uint32_t stream_voices = 0;
  std::uint32_t mix_behavior = 0;
  std::uint32_t driver_update_flags = 0;
  std::int32_t muted_channels = 0;
  std::int32_t music_pitch_scale = 0;
  MasterFade master_fade;

  friend bool operator==(const MixerState&, const MixerState&) = default;
};

// The levels a channel's modulation mixes with.
struct MixLevels {
  // The active music layer's master volume (music channels only).
  std::int32_t master_volume = 0;
  // g_Sound_MasterPitchScaleQ16_16 (music channels only).
  std::int32_t music_pitch_scale = 0;
  std::uint32_t mix_behavior = 0;
  PanGains pan_gains;
};

enum class MixRefusal : std::uint8_t {
  // An LFO read lies outside the channel's waveform or its loop marker
  // jumps outside it; the LFO keeps its position and level.
  kWaveformOutside,
  // A side-chained channel has no channel record before it.
  kSidechainWithoutPrevious,
  // The layer's voice search starts outside the voices.
  kVoiceSearchOutside,
  // Sound-effect voices below the first sound-effect channel are keyed.
  kEffectVoiceOutsideChannels,
  // A channel to configure names no voice.
  kChannelWithoutVoice,
  // The secondary layer is named while none plays.
  kSecondaryLayerMissing,
};

class RefusalSink {
 public:
  virtual ~RefusalSink() = default;
  virtual void Refuse(MixRefusal refusal) = 0;
};

// The channel a configure request names.
struct ChannelRef {
  Seq::ChannelGroup group = Seq::ChannelGroup::kMusic;
  std::size_t index = 0;

  friend bool operator==(const ChannelRef&, const ChannelRef&) = default;
};

class MixerHost : public RefusalSink {
 public:
  // The envelope level voice `voice` plays at (SpuGetVoiceEnvX).
  virtual std::int16_t Envelope(std::uint32_t voice) = 0;
  // Hands the channel's voice parameters to its voice and clears the
  // channel's dirty flags (SetVoiceParams / SetVoiceParamsByFlags).
  virtual void ConfigureChannel(ChannelRef ref, MixChannel& channel) = 0;
  // g_Sound_VoiceChannelConfigs[voice] = the layer.
  virtual void SetVoiceOwner(std::uint32_t voice, LayerId layer) = 0;
  virtual void SetReverbDepth(std::int16_t left, std::int16_t right) = 0;
  virtual void SetNoiseClock(std::int32_t clock) = 0;
  virtual void SetReverbVoices(std::uint32_t voices) = 0;
  virtual void SetNoiseVoices(std::uint32_t voices) = 0;
  virtual void SetFrequencyModulationVoices(std::uint32_t voices) = 0;
  virtual void KeyOn(std::uint32_t voices) = 0;
  virtual void KeyOff(std::uint32_t voices) = 0;
};

// The layer state one channel's slides update.
struct SlideState {
  Seq::MusicLayerState active;
  Seq::VoiceSchedulerState scheduler;
  std::uint32_t driver_update_flags = 0;

  friend bool operator==(const SlideState&, const SlideState&) = default;
};

// Sound_UpdateSlidesAndDelays.
void UpdateChannelSlidesAndDelays(MixChannel& channel, SlideState& state,
                                  std::uint32_t channel_bit,
                                  Driver::ChannelKind kind, RefusalSink& sink);
// func_8001436C: `previous_sample_rate` is the sample rate of the channel
// record before this one, which the side chains read.
void UpdateMusicChannelModulation(
    MixChannel& channel, std::optional<std::uint16_t> previous_sample_rate,
    const MixLevels& levels, RefusalSink& sink);
// func_800147CC.
void UpdateSoundEffectModulation(
    MixChannel& channel, std::optional<std::uint16_t> previous_sample_rate,
    const MixLevels& levels, RefusalSink& sink);
// Sound_StealQuietestVoice: the quietest voice from the active layer's
// search start (or from voice 0), unassigned from every channel; kNoVoice
// when every voice is reserved.
std::uint32_t StealQuietestVoice(MixerState& state, bool full_scan,
                                 RefusalSink& sink);
// Sound_FindFreeVoice: the first silent voice from the active layer's search
// start (or from voice 0); kNoVoice when none is.
std::uint32_t FindFreeVoice(const MixerState& state, bool from_first_voice,
                            RefusalSink& sink);
// func_80014D70: modulates and configures the channels of `layer` in
// `channels`, keying on the pending ones. `fixed_voices` are channels whose
// voice is their own index.
void ApplyMusicChannelVoices(MixerState& state, PanGains pan_gains,
                             LayerId layer, std::uint32_t channels,
                             std::uint32_t fixed_voices,
                             std::uint32_t& key_on_voices, MixerHost& host);
// UnassignVoicesFromChannels: clears `voice` from `channels` and from the
// secondary layer's channels.
void UnassignVoice(MixerState& state, MusicChannels& channels,
                   std::uint32_t voice);
// func_8001503C.
bool IsVoiceAssignedToChannels(const MusicChannels& channels,
                               std::uint32_t channel_mask, std::uint32_t voice);
// func_80015080: refreshes the envelope levels voice allocation reads.
void RefreshVoiceAvailability(MixerState& state, std::uint32_t reserved_voices,
                              MixerHost& host);
// Sound_ApplyMasterFadeToChannelVolume.
void ApplySavedMusicMasterFade(Seq::MusicLayerState& layer, MasterFade& fade);
// Sound_RestoreChannelVolumeFromMasterFade.
void RestoreSavedMusicVolume(Seq::MusicLayerState& layer,
                             const MasterFade& fade);
// func_80015220: one tick's voice commit.
void CommitPlaybackVoices(MixerState& state, PanGains pan_gains,
                          MixerHost& host);
// ChannelMaskToVoiceMaskFiltered.
void AccumulateFilteredVoiceMask(const MusicChannels& channels,
                                 std::uint32_t& voice_mask,
                                 std::uint32_t channel_mask,
                                 std::uint32_t voice_filter);
// Sound_ProcessKeyOffRequests.
void ReleasePendingVoices(MixerState& state, MixerHost& host);
// The channels of each layer and the sound-effect voices one voice mode
// (noise, reverb or FM) covers.
struct VoiceModeRequest {
  std::uint32_t secondary_channels = 0;
  std::uint32_t active_channels = 0;
  std::uint32_t extra_voices = 0;
};
// func_80015970: the voices of the request's secondary and active channels,
// plus its extra voices.
std::uint32_t BuildVoiceModeMask(MixerState& state,
                                 const VoiceModeRequest& request,
                                 RefusalSink& sink);
}  // namespace VS::Audio::Mixer
