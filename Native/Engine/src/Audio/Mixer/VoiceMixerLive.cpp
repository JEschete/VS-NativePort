// Hand-maintained (tools/port/owned.csv).
// Live binding for VoiceMixer.hpp: the channel tables, the music layers, the
// voice scheduler, envelopes, owners and mode masks, the LFO shape table and
// the voice output.
#include "Audio/VoiceMixerLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SequencerOutput.h"
#include "Audio/VoiceMixer.hpp"
#include "Audio/VoiceOutputLive.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
}

namespace VS::Audio::Mixer {
namespace {
static_assert(std::size(Audio_Seq_musicChannels) == kMusicChannelCount);
static_assert(std::size(Audio_SoundEffects_channels) >= kEffectChannelCount);
static_assert(std::size(Audio_Mixer_voiceEnvelopes) == kVoiceCount);
static_assert(std::size(Audio_Mixer_voiceOwners) == kVoiceCount);
static_assert(std::size(Audio_Mixer_kPanGains) == kPanGainCount);
static_assert(sizeof(Audio_Mixer_kPanGains[0]) == sizeof(std::int16_t));
// A legacy field's offset and the offset the original reads it at.
struct FieldPin {
  std::size_t offset = 0;
  std::size_t expected = 0;
};
constexpr std::array kFieldPins{
    FieldPin{offsetof(Audio_VoiceParams, sampleRate), 0x10},
    FieldPin{offsetof(Audio_VoiceParams, volume), 0x18},
    FieldPin{offsetof(Audio_VoiceEnvelope, envelopeLevel), 0x4},
    FieldPin{offsetof(Audio_VoiceModeMasks, noise), 0x4},
    FieldPin{offsetof(Audio_VoiceModeMasks, fm), 0x8},
    FieldPin{offsetof(Audio_FadeTimer, savedValue), 0xC},
    FieldPin{offsetof(Audio_DriverFlags, updateFlags), 0x8},
};
static_assert(std::ranges::all_of(kFieldPins, [](const FieldPin& pin) {
  return pin.offset == pin.expected;
}));
static_assert(sizeof(Audio_CutsceneStreamState::voiceMask) ==
              sizeof(std::int32_t));

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

// The longest waveform the loop marker scan reads.
constexpr std::size_t kMaxWaveSamples = 0x1000;
// A loop marker is two zero samples and the offset after them.
constexpr std::size_t kLoopMarkerSamples = 3;

// One LFO shape: from its first sample through its loop marker.
struct WaveSpan {
  std::int16_t* base = nullptr;
  std::size_t size = 0;
};

std::size_t MarkerEnd(const std::int16_t* base) {
  for (std::size_t index = 0; index + 1 < kMaxWaveSamples; ++index) {
    if (base[index] == 0 && base[index + 1] == 0) {
      return index + kLoopMarkerSamples;
    }
  }
  return 0;
}

// The waveforms Audio_Seq_kLfoWaveforms names, rebuilt whenever a shape
// pointer changes.
class LfoWaveforms {
 public:
  static const LfoWaveforms& Current() {
    thread_local LfoWaveforms waveforms;
    if (!std::equal(std::begin(Audio_Seq_kLfoWaveforms),
                    std::end(Audio_Seq_kLfoWaveforms),
                    waveforms.table_.begin())) {
      waveforms.Rebuild();
    }
    return waveforms;
  }

  // The waveform with the lowest start whose samples hold `pointer`; an
  // empty cursor when none does.
  [[nodiscard]] LfoCursor Cursor(const std::int16_t* pointer) const {
    if (pointer == nullptr) return {};
    for (const WaveSpan& span : spans_) {
      if (std::less<>{}(pointer, span.base)) break;
      if (std::less<>{}(pointer, span.base + span.size)) {
        return {std::span<const std::int16_t>(span.base, span.size),
                static_cast<std::size_t>(pointer - span.base)};
      }
    }
    return {};
  }

  // Stores a resolved cursor back; an empty one leaves `pointer` unchanged.
  void Store(std::int16_t*& pointer, const LfoCursor& cursor) const {
    if (cursor.wave.empty()) return;
    for (const WaveSpan& span : spans_) {
      if (span.base == cursor.wave.data() && span.size == cursor.wave.size()) {
        pointer = span.base + cursor.position;
        return;
      }
    }
  }

 private:
  void Rebuild() {
    std::copy(std::begin(Audio_Seq_kLfoWaveforms),
              std::end(Audio_Seq_kLfoWaveforms), table_.begin());
    spans_.clear();
    for (std::int16_t* base : table_) {
      if (base == nullptr) continue;
      const std::size_t size = MarkerEnd(base);
      if (size != 0) spans_.push_back({base, size});
    }
    std::sort(spans_.begin(), spans_.end(),
              [](const WaveSpan& left, const WaveSpan& right) {
                return std::less<>{}(left.base, right.base);
              });
  }

  std::array<std::int16_t*, std::size(Audio_Seq_kLfoWaveforms)> table_{};
  std::vector<WaveSpan> spans_;
};

MixChannel LoadMixChannel(const Audio_SeqChannel& legacy,
                          const LfoWaveforms& waves) {
  MixChannel channel;
  channel.state = Seq::LoadChannelState(legacy);
  channel.pitch_slide = {Same<std::int32_t>(legacy.pitchSlide),
                         Same<std::int32_t>(legacy.pitchSlideStep),
                         Same<std::uint16_t>(legacy.pitchSlideTicksLeft)};
  channel.assigned_voice = Same<std::uint32_t>(legacy.voice.assignedVoice);
  channel.sample_rate = Same<std::uint16_t>(legacy.voice.sampleRate);
  channel.volume_left = Same<std::int16_t>(legacy.voice.volume.left);
  channel.volume_right = Same<std::int16_t>(legacy.voice.volume.right);
  channel.effect_tags = Same<std::int32_t>(legacy.effectTags);
  channel.effect_pan = Same<std::uint16_t>(legacy.effectPan);
  channel.effect_pitch_scale = Same<std::int32_t>(legacy.effectPitchScale);
  channel.vibrato = waves.Cursor(legacy.vibratoWave);
  channel.tremolo = waves.Cursor(legacy.tremoloWave);
  channel.auto_pan = waves.Cursor(legacy.autoPanWave);
  return channel;
}

void StoreMixChannel(Audio_SeqChannel& legacy, const MixChannel& channel,
                     const LfoWaveforms& waves) {
  Seq::StoreChannelState(legacy, channel.state);
  legacy.pitchSlide = channel.pitch_slide.slide;
  legacy.pitchSlideStep = channel.pitch_slide.step;
  legacy.pitchSlideTicksLeft = channel.pitch_slide.ticks_left;
  legacy.voice.assignedVoice = channel.assigned_voice;
  legacy.voice.sampleRate = channel.sample_rate;
  legacy.voice.volume.left = channel.volume_left;
  legacy.voice.volume.right = channel.volume_right;
  legacy.effectTags = channel.effect_tags;
  legacy.effectPan = channel.effect_pan;
  legacy.effectPitchScale = channel.effect_pitch_scale;
  waves.Store(legacy.vibratoWave, channel.vibrato);
  waves.Store(legacy.tremoloWave, channel.tremolo);
  waves.Store(legacy.autoPanWave, channel.auto_pan);
}

PanGains LivePanGains() { return PanGains(Audio_Mixer_kPanGains); }

// The record before `channel` when both lie in one channel table.
std::optional<std::uint16_t> PreviousSampleRate(
    const Audio_SeqChannel* channel) {
  const auto follows_first = [channel](const Audio_SeqChannel* first,
                                       std::size_t count) {
    return first != nullptr && std::less<>{}(first, channel) &&
           std::less<>{}(channel, first + count);
  };
  if (follows_first(std::data(Audio_Seq_musicChannels),
                    std::size(Audio_Seq_musicChannels)) ||
      follows_first(Audio_Seq_secondaryMusicChannels, kMusicChannelCount) ||
      follows_first(std::data(Audio_SoundEffects_channels),
                    std::size(Audio_SoundEffects_channels))) {
    return Same<std::uint16_t>(std::prev(channel)->voice.sampleRate);
  }
  return std::nullopt;
}

const char* RefusalReason(MixRefusal refusal) {
  switch (refusal) {
    case MixRefusal::kWaveformOutside:
      return "an LFO read lies outside its waveform";
    case MixRefusal::kSidechainWithoutPrevious:
      return "a side-chained channel has no channel record before it";
    case MixRefusal::kVoiceSearchOutside:
      return "the layer's voice search starts outside the voices";
    case MixRefusal::kEffectVoiceOutsideChannels:
      return "sound-effect voices below the first sound-effect channel are "
             "keyed";
    case MixRefusal::kChannelWithoutVoice:
      return "a channel to configure names no voice";
    case MixRefusal::kSecondaryLayerMissing:
      return "the secondary music layer is named while none plays";
  }
  return "refused";
}

class LiveRefusals : public virtual RefusalSink {
 public:
  explicit LiveRefusals(const char* operation) : operation_(operation) {}

  void Refuse(MixRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};

// The legacy music layers a mixer pass runs over.
struct LiveLayers {
  Audio_MusicConfig* primary = nullptr;
  Audio_MusicConfig* secondary = nullptr;
  Audio_SeqChannel* secondary_channels = nullptr;
};

// Without a secondary layer the active layer is the primary one. With one,
// the active layer must be g_PrimaryMusicConfig or the secondary layer, and
// the secondary layer and its channels must be records of their own.
std::optional<LiveLayers> BindLayers(const char* operation) {
  Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
  if (active == nullptr) {
    Seq::ReportRefusal(operation, "no music layer is active");
    return std::nullopt;
  }
  if (secondary == nullptr) return LiveLayers{active, nullptr, nullptr};
  Audio_SeqChannel* channels = Audio_Seq_secondaryMusicChannels;
  if (secondary == &Audio_Seq_primaryMusicConfig || channels == nullptr ||
      channels == std::data(Audio_Seq_musicChannels)) {
    Seq::ReportRefusal(operation,
                       "the secondary music layer aliases the primary one");
    return std::nullopt;
  }
  if (active != &Audio_Seq_primaryMusicConfig && active != secondary) {
    Seq::ReportRefusal(operation, "the active music layer is neither layer");
    return std::nullopt;
  }
  return LiveLayers{&Audio_Seq_primaryMusicConfig, secondary, channels};
}

void LoadChannels(std::span<MixChannel> typed, const Audio_SeqChannel* legacy,
                  const LfoWaveforms& waves) {
  for (std::size_t index = 0; index < typed.size(); ++index) {
    typed[index] = LoadMixChannel(legacy[index], waves);
  }
}

void StoreChannels(std::span<const MixChannel> typed, Audio_SeqChannel* legacy,
                   const LfoWaveforms& waves) {
  for (std::size_t index = 0; index < typed.size(); ++index) {
    StoreMixChannel(legacy[index], typed[index], waves);
  }
}

void LoadMixer(MixerState& state, const LiveLayers& layers,
               const LfoWaveforms& waves) {
  state.primary.config = Seq::LoadMusicLayer(*layers.primary);
  LoadChannels(state.primary.channels, std::data(Audio_Seq_musicChannels),
               waves);
  state.secondary.reset();
  state.active = LayerId::kPrimary;
  if (layers.secondary != nullptr) {
    state.secondary.emplace();
    state.secondary->config = Seq::LoadMusicLayer(*layers.secondary);
    LoadChannels(state.secondary->channels, layers.secondary_channels, waves);
    if (Audio_Seq_activeMusicConfig == layers.secondary) {
      state.active = LayerId::kSecondary;
    }
  }
  LoadChannels(state.effects, std::data(Audio_SoundEffects_channels), waves);
  state.scheduler = Seq::LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
    state.envelopes[voice] =
        Same<std::int16_t>(Audio_Mixer_voiceEnvelopes[voice].envelopeLevel);
  }
  state.voice_modes = {Same<std::uint32_t>(Audio_Mixer_voiceModes.reverb),
                       Same<std::uint32_t>(Audio_Mixer_voiceModes.noise),
                       Same<std::uint32_t>(Audio_Mixer_voiceModes.fm)};
  state.stream_voices =
      Same<std::uint32_t>(Audio_Cutscene_streamState.voiceMask);
  state.mix_behavior =
      Same<std::uint32_t>(Audio_Driver_controlLatches.mixBehavior);
  state.driver_update_flags =
      Same<std::uint32_t>(Audio_Driver_controlLatches.updateFlags);
  state.muted_channels = Same<std::int32_t>(Audio_Mixer_mutedMusicChannels);
  state.music_pitch_scale = Same<std::int32_t>(Audio_Mixer_musicPitchScale);
  state.master_fade = {Same<std::int32_t>(Audio_Seq_masterFade.value),
                       Same<std::int32_t>(Audio_Seq_masterFade.savedValue)};
}

void StoreMixer(const MixerState& state, const LiveLayers& layers,
                const LfoWaveforms& waves) {
  Seq::StoreMusicLayer(*layers.primary, state.primary.config);
  StoreChannels(state.primary.channels, std::data(Audio_Seq_musicChannels),
                waves);
  if (layers.secondary != nullptr && state.secondary) {
    Seq::StoreMusicLayer(*layers.secondary, state.secondary->config);
    StoreChannels(state.secondary->channels, layers.secondary_channels, waves);
  }
  Audio_Seq_activeMusicConfig =
      state.active == LayerId::kSecondary && layers.secondary != nullptr
          ? layers.secondary
          : layers.primary;
  StoreChannels(state.effects, std::data(Audio_SoundEffects_channels), waves);
  Seq::StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
    Audio_Mixer_voiceEnvelopes[voice].envelopeLevel = state.envelopes[voice];
  }
  Audio_Mixer_voiceModes.reverb = Same<int>(state.voice_modes.reverb);
  Audio_Mixer_voiceModes.noise = Same<int>(state.voice_modes.noise);
  Audio_Mixer_voiceModes.fm = Same<int>(state.voice_modes.fm);
  Audio_Cutscene_streamState.voiceMask = Same<int>(state.stream_voices);
  Audio_Driver_controlLatches.mixBehavior = state.mix_behavior;
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  Audio_Mixer_mutedMusicChannels = state.muted_channels;
  Audio_Mixer_musicPitchScale = state.music_pitch_scale;
  Audio_Seq_masterFade.value = state.master_fade.value;
  Audio_Seq_masterFade.savedValue = state.master_fade.saved_value;
}

// One mixer pass over the live voice output. A channel to configure is
// stored to its record first, and its dirty flags read back after.
class LiveMixer final : public MixerHost, private LiveRefusals {
 public:
  LiveMixer(const char* operation, const LiveLayers& layers,
            const LfoWaveforms& waves)
      : LiveRefusals(operation), layers_(layers), waves_(waves) {}

  void Refuse(MixRefusal refusal) override { LiveRefusals::Refuse(refusal); }

  std::int16_t Envelope(std::uint32_t voice) override {
    return Audio_Native_Envelope(voice);
  }

  void ConfigureChannel(ChannelRef ref, MixChannel& channel) override {
    Audio_SeqChannel* record = Record(ref);
    if (record == nullptr) return;
    StoreMixChannel(*record, channel, waves_);
    Audio_Native_ConfigureChannel(record);
    channel.state.voice.dirty_flags =
        Same<std::uint32_t>(record->voice.dirtyFlags);
  }

  void SetVoiceOwner(std::uint32_t voice, LayerId layer) override {
    Audio_Mixer_voiceOwners[voice] =
        layer == LayerId::kSecondary ? layers_.secondary : layers_.primary;
  }

  void SetReverbDepth(std::int16_t left, std::int16_t right) override {
    Audio_Native_SetReverbDepth(left, right);
  }

  void SetNoiseClock(std::int32_t clock) override {
    Audio_Native_SetNoiseClock(clock);
  }

  void SetReverbVoices(std::uint32_t voices) override {
    Driver::SetLiveVoiceReverbMask(voices);
  }

  void SetNoiseVoices(std::uint32_t voices) override {
    Driver::SetLiveVoiceNoiseMask(voices);
  }

  void SetFrequencyModulationVoices(std::uint32_t voices) override {
    Driver::SetLiveVoiceFrequencyModulationMask(voices);
  }

  void KeyOn(std::uint32_t voices) override { Driver::KeyOnLiveVoices(voices); }

  void KeyOff(std::uint32_t voices) override {
    Driver::KeyOffLiveVoices(voices);
  }

 private:
  Audio_SeqChannel* Record(ChannelRef ref) const {
    switch (ref.group) {
      case Seq::ChannelGroup::kMusic:
        if (ref.index < kMusicChannelCount) {
          return &Audio_Seq_musicChannels[ref.index];
        }
        break;
      case Seq::ChannelGroup::kSecondaryMusic:
        if (layers_.secondary_channels != nullptr &&
            ref.index < kMusicChannelCount) {
          return &layers_.secondary_channels[ref.index];
        }
        break;
      case Seq::ChannelGroup::kSoundEffects:
        if (ref.index < kEffectChannelCount) {
          return &Audio_SoundEffects_channels[ref.index];
        }
        break;
    }
    return nullptr;
  }

  LiveLayers layers_;
  const LfoWaveforms& waves_;
};

using MixerPass = void (*)(MixerState&, MixerHost&);

void RunMixerPass(const char* operation, MixerPass pass) {
  const std::optional<LiveLayers> layers = BindLayers(operation);
  if (!layers) return;
  const LfoWaveforms& waves = LfoWaveforms::Current();
  const auto state = std::make_unique<MixerState>();
  LoadMixer(*state, *layers, waves);
  LiveMixer host(operation, *layers, waves);
  pass(*state, host);
  StoreMixer(*state, *layers, waves);
}

// The channel modulation passes; music reads the active layer's master
// volume and the music pitch scale.
template <typename Modulate>
void ModulateLiveChannel(const char* operation, Audio_SeqChannel* channel,
                         bool music, Modulate modulate) {
  const Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  if (channel == nullptr || (music && active == nullptr)) {
    Seq::ReportRefusal(operation, "the channel or the active layer is missing");
    return;
  }
  const LfoWaveforms& waves = LfoWaveforms::Current();
  const MixLevels levels{
      music ? Same<std::int32_t>(active->masterVolume) : 0,
      music ? Same<std::int32_t>(Audio_Mixer_musicPitchScale) : 0,
      Same<std::uint32_t>(Audio_Driver_controlLatches.mixBehavior),
      LivePanGains()};
  MixChannel typed = LoadMixChannel(*channel, waves);
  LiveRefusals refusals(operation);
  modulate(typed, PreviousSampleRate(channel), levels, refusals);
  StoreMixChannel(*channel, typed, waves);
}
}  // namespace

// decomp: SLUS_010.40 Sound_UpdateSlidesAndDelays
// (src/SLUS_010.40/25AC.c:1400)
void LiveUpdateChannelSlidesAndDelays(Audio_SeqChannel* channel,
                                      unsigned channel_bit,
                                      Driver::ChannelKind kind) {
  constexpr const char* kOperation = "UpdateChannelSlidesAndDelays";
  Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  if (channel == nullptr || active == nullptr) {
    Seq::ReportRefusal(kOperation,
                       "the channel or the active layer is missing");
    return;
  }
  const LfoWaveforms& waves = LfoWaveforms::Current();
  SlideState state{
      Seq::LoadMusicLayer(*active),
      Seq::LoadVoiceScheduler(Audio_Mixer_voiceScheduler),
      Same<std::uint32_t>(Audio_Driver_controlLatches.updateFlags)};
  MixChannel typed = LoadMixChannel(*channel, waves);
  LiveRefusals refusals(kOperation);
  UpdateChannelSlidesAndDelays(typed, state, channel_bit, kind, refusals);
  StoreMixChannel(*channel, typed, waves);
  Seq::StoreMusicLayer(*active, state.active);
  Seq::StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
}

// decomp: SLUS_010.40 func_8001436C (src/SLUS_010.40/25AC.c:1566)
void LiveUpdateMusicChannelModulation(Audio_SeqChannel* channel) {
  ModulateLiveChannel("UpdateMusicChannelModulation", channel, true,
                      &UpdateMusicChannelModulation);
}

// decomp: SLUS_010.40 func_800147CC (src/SLUS_010.40/25AC.c:1678)
void LiveUpdateSoundEffectModulation(Audio_SeqChannel* channel) {
  ModulateLiveChannel("UpdateSoundEffectModulation", channel, false,
                      &UpdateSoundEffectModulation);
}

// decomp: SLUS_010.40 func_80015220 (src/SLUS_010.40/25AC.c:2027)
void LiveCommitPlaybackVoices() {
  RunMixerPass("CommitPlaybackVoices", [](MixerState& state, MixerHost& host) {
    CommitPlaybackVoices(state, LivePanGains(), host);
  });
}

// decomp: SLUS_010.40 Sound_ProcessKeyOffRequests
// (src/SLUS_010.40/25AC.c:2187)
void LiveReleasePendingVoices() {
  RunMixerPass("ReleasePendingVoices", [](MixerState& state, MixerHost& host) {
    ReleasePendingVoices(state, host);
  });
}
}  // namespace VS::Audio::Mixer
