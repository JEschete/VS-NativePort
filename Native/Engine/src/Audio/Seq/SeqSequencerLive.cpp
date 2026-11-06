// Hand-maintained (tools/port/owned.csv).
// Live binding for SeqSequencerLive.hpp: the channel tables, the music
// layers, the voice scheduler, the sequencer's tables and handler tables, and
// the driver's latches, slides and tick costs.
#include "Audio/SeqSequencerLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/VoiceMixerLive.hpp"
#include "Audio/XaMixLive.hpp"

extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/StreamPlayback.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio::Seq {
namespace {
using OpcodeHandler = std::decay_t<decltype(Audio_Seq_kOpcodeHandlers[0])>;

static_assert(std::size(Audio_Seq_kOpcodeHandlers) == kOpcodeCount);
static_assert(std::size(Audio_Seq_kExtendedOpcodeHandlers) ==
              kExtendedOpcodeCount);
static_assert(std::size(Audio_Seq_kOpcodeLengths) >= kOpcodeCount);
static_assert(std::size(Audio_Seq_kExtendedOpcodeLengths) >=
              kExtendedOpcodeCount);
static_assert(std::size(Audio_Seq_kNoteLengths) == kNoteLengthCount);
static_assert(std::size(Audio_Seq_kSemitonePitches) >= kSemitoneCount);
static_assert(std::size(Audio_Seq_kRandomPitchOffsets) ==
              kRandomPitchOffsetCount);
static_assert(std::size(Audio_Seq_musicChannels) == kMusicChannelCount);
static_assert(std::size(Audio_SoundEffects_channels) >= kEffectChannelCount);
static_assert(std::size(Audio_Driver_recentTickCosts) ==
              std::tuple_size_v<decltype(PlaybackState::recent_tick_costs)>);
static_assert(sizeof(Audio_Seq_kOpcodeLengths[0]) == sizeof(std::uint8_t));
static_assert(sizeof(Audio_Seq_kNoteLengths[0]) == sizeof(std::uint16_t));
static_assert(sizeof(Audio_Seq_kSemitonePitches[0]) == sizeof(std::uint32_t));

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

const SequencerTables& LiveTables() {
  static const SequencerTables tables{
      std::span<const std::uint8_t>(Audio_Seq_kOpcodeLengths, kOpcodeCount),
      std::span<const std::uint8_t>(Audio_Seq_kExtendedOpcodeLengths,
                                    kExtendedOpcodeCount),
      std::span<const std::uint16_t, kNoteLengthCount>(Audio_Seq_kNoteLengths),
      std::span<const std::uint32_t, kSemitoneCount>(
          std::data(Audio_Seq_kSemitonePitches), kSemitoneCount),
      std::span<const std::int8_t>(
          reinterpret_cast<const std::int8_t*>(Audio_Seq_kNoteRemapTables),
          std::size(Audio_Seq_kNoteRemapTables)),
      std::span<const std::uint8_t, kRandomPitchOffsetCount>(
          reinterpret_cast<const std::uint8_t*>(Audio_Seq_kRandomPitchOffsets),
          kRandomPitchOffsetCount),
  };
  return tables;
}

// The bytes from `start` to the end of the program memory it lies in.
std::span<const std::uint8_t> BytesFrom(char* start) {
  const std::optional<LiveProgramMemory> memory = LiveProgramRegion(start);
  if (!memory) return {};
  return memory->From(start).View();
}

const char* StepRefusalReason(StepRefusal refusal) {
  switch (refusal) {
    case StepRefusal::kOpcodeOutside:
      return "the program counter lies outside the program memory";
    case StepRefusal::kOperandsOutside:
      return "the program memory ends inside an opcode's operands";
    case StepRefusal::kUnknownExtendedOpcode:
      return "an FE opcode names no extended command";
    case StepRefusal::kCommandLimit:
      return "the channel ran too many commands without a note";
    case StepRefusal::kLookaheadFailed:
      return "the next-note lookahead left the program memory";
    case StepRefusal::kKeymapOutside:
      return "the note's keymap entry lies outside the program memory";
    case StepRefusal::kInstrumentMissing:
      return "the instrument lies outside the instrument table";
    case StepRefusal::kNoteRemapMissing:
      return "the layer remaps notes without a remap table";
    case StepRefusal::kWaveformMissing:
      return "the LFO waveform is missing";
    case StepRefusal::kRandomPitchPhaseOutside:
      return "the random pitch phase lies outside its table";
    case StepRefusal::kPitchBendWithoutLength:
      return "a pitch bend started without a slide length";
    case StepRefusal::kCommandMissing:
      return "the handler table has no command for the opcode";
    case StepRefusal::kCommandRefused:
      return "the opcode's command refused";
  }
  return "refused";
}

// One channel step over the live channel. Every command runs over the
// channel record; the step's state is stored before and loaded after it.
class LiveStepHost final : public StepHost {
 public:
  explicit LiveStepHost(Audio_SeqChannel& channel)
      : channel_(channel),
        region_(LiveChannelProgram(channel).value_or(LiveProgramMemory{})) {}

  [[nodiscard]] const LiveProgramMemory& Region() const { return region_; }

  void LoadStep(StepState& state) const {
    const SequenceChannel record = LoadSequenceChannel(channel_, region_);
    state.channel = record.channel;
    state.program = record.program;
    state.pitch_slide = record.pitch_slide;
    state.assigned_voice = record.assigned_voice;
    state.layer = LoadMusicLayer(*Audio_Seq_activeMusicConfig);
    state.scheduler = LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
    state.mix_behavior = Audio_Driver_controlLatches.mixBehavior;
    state.driver_update_flags = Audio_Driver_controlLatches.updateFlags;
    state.random_pitch_phase = Audio_Seq_randomPitchPhase;
  }

  void StoreStep(const StepState& state) {
    StoreSequenceChannel(channel_,
                         {state.channel, state.program, state.pitch_slide,
                          state.assigned_voice, false},
                         region_);
    StoreMusicLayer(*Audio_Seq_activeMusicConfig, state.layer);
    StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
    Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  }

  ProgramData Program() override { return region_.View(); }

  const SequencerTables& Tables() override { return LiveTables(); }

  CommandOutcome RunCommand(StepState& state, std::uint8_t opcode,
                            std::uint32_t channel_bit) override {
    const std::size_t index = std::size_t{opcode} - kEndOfTrackOpcode;
    if (index >= std::size(Audio_Seq_kOpcodeHandlers)) {
      return CommandOutcome::kMissing;
    }
    return Run(state, Audio_Seq_kOpcodeHandlers[index], channel_bit);
  }

  CommandOutcome RunExtendedCommand(StepState& state, std::uint8_t opcode,
                                    std::uint32_t channel_bit) override {
    if (opcode >= std::size(Audio_Seq_kExtendedOpcodeHandlers)) {
      return CommandOutcome::kMissing;
    }
    return Run(state, Audio_Seq_kExtendedOpcodeHandlers[opcode], channel_bit);
  }

  std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) override {
    return LiveInstrument(index);
  }

  std::span<const std::uint8_t> ChannelKeymap() override {
    return BytesFrom(channel_.keymap);
  }

  std::span<const std::uint8_t> DrumKeymap() override {
    return BytesFrom(
        reinterpret_cast<char*>(Audio_Seq_activeMusicConfig->keymapTable));
  }

  bool HasLfoWaveform(std::uint16_t shape) override {
    return shape < std::size(Audio_Seq_kLfoWaveforms) &&
           Audio_Seq_kLfoWaveforms[shape] != nullptr;
  }

  void Refuse(StepRefusal refusal) override {
    ReportRefusal("ExecuteChannelStep", StepRefusalReason(refusal));
  }

 private:
  static constexpr std::size_t kEndOfTrackOpcode = 0xA0;

  // Runs `handler` over the channel record. Its program may now lie in other
  // memory (FE0B restarts the channel when it takes the channel's slot).
  CommandOutcome Run(StepState& state, OpcodeHandler handler,
                     std::uint32_t channel_bit) {
    if (handler == nullptr) return CommandOutcome::kMissing;
    StoreStep(state);
    const std::uint64_t refusals = LiveRefusalCount();
    {
      const ScopedChannelProgram scope(channel_, region_);
      handler(&channel_, static_cast<int>(channel_bit));
      region_ = LiveChannelProgram(channel_).value_or(LiveProgramMemory{});
    }
    LoadStep(state);
    return LiveRefusalCount() == refusals ? CommandOutcome::kRan
                                          : CommandOutcome::kRefused;
  }

  Audio_SeqChannel& channel_;
  LiveProgramMemory region_;
};

std::uint32_t ChannelBit(ChannelGroup group, std::size_t index) {
  if (group == ChannelGroup::kSoundEffects) {
    return 1U << (kFirstEffectVoice + index);
  }
  return 1U << index;
}

// The channel table `group` names: empty without a secondary layer's.
Audio_SeqChannel* ChannelOf(ChannelGroup group, std::size_t index) {
  switch (group) {
    case ChannelGroup::kMusic:
      if (index < std::size(Audio_Seq_musicChannels)) {
        return &Audio_Seq_musicChannels[index];
      }
      break;
    case ChannelGroup::kSecondaryMusic:
      if (Audio_Seq_secondaryMusicChannels != nullptr &&
          index < kMusicChannelCount) {
        return &Audio_Seq_secondaryMusicChannels[index];
      }
      break;
    case ChannelGroup::kSoundEffects:
      if (index < kEffectChannelCount) {
        return &Audio_SoundEffects_channels[index];
      }
      break;
  }
  ReportRefusal("TickPlayback", "the channel group has no such channel");
  return nullptr;
}

// While a secondary layer's channel runs, the secondary layer is the active
// one, as func_80019A58 makes it for the secondary pass.
class ScopedActiveLayer {
 public:
  explicit ScopedActiveLayer(ChannelGroup group)
      : previous_(Audio_Seq_activeMusicConfig) {
    if (group == ChannelGroup::kSecondaryMusic) {
      Audio_Seq_activeMusicConfig = Audio_Seq_secondaryMusicConfig;
    }
  }
  ~ScopedActiveLayer() { Audio_Seq_activeMusicConfig = previous_; }
  ScopedActiveLayer(const ScopedActiveLayer&) = delete;
  ScopedActiveLayer& operator=(const ScopedActiveLayer&) = delete;

 private:
  Audio_MusicConfig* previous_;
};

LevelSlide LoadSlide(std::int32_t value, std::int32_t step,
                     std::int16_t length) {
  return {value, step, length};
}

void LoadPlayback(PlaybackState& state) {
  const Audio_DriverFlags& latches = Audio_Driver_controlLatches;
  state.control_latches = latches.controlLatches;
  state.mix_behavior = latches.mixBehavior;
  state.driver_update_flags = latches.updateFlags;
  state.stream_control_flags = Audio_Cutscene_streamControlFlags;
  state.active = LoadMusicLayer(*Audio_Seq_activeMusicConfig);
  state.active_is_primary =
      Audio_Seq_activeMusicConfig == &Audio_Seq_primaryMusicConfig;
  state.secondary.reset();
  if (const Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig) {
    state.secondary = LoadMusicLayer(*secondary);
  }
  state.scheduler = LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  state.music_loops_remaining = Audio_Seq_musicLoopsRemaining;
  state.music_transition_timer = Audio_Seq_musicTransitionTimer;
  state.tick_counter = Same<std::uint16_t>(Audio_Seq_tickCounter);
  state.music_pending = Audio_Seq_pendingMusicData.param1 != 0;
  state.master_fade = {Audio_Seq_masterFade.value, Audio_Seq_masterFade.step,
                       Audio_Seq_masterFade.ticksRemaining};
  state.random_pitch_phase = Audio_Seq_randomPitchPhase;
  state.disc_volume =
      LoadSlide(Audio_Seq_discVolume, Audio_Seq_discVolumeFadeStep,
                Audio_Seq_discVolumeFadeLength);
  state.tempo_scale =
      LoadSlide(Audio_Mixer_tempoScale, Audio_Mixer_tempoScaleStep,
                Audio_Mixer_tempoScaleSlideLength);
  state.music_pitch_scale =
      LoadSlide(Audio_Mixer_musicPitchScale, Audio_Mixer_musicPitchScaleStep,
                Audio_Mixer_musicPitchScaleSlideLength);
  std::copy(std::begin(Audio_Driver_recentTickCosts),
            std::end(Audio_Driver_recentTickCosts),
            state.recent_tick_costs.begin());
  state.recent_tick_cost_total = Audio_Driver_recentTickCostTotal;
}

void StorePlayback(const PlaybackState& state) {
  Audio_DriverFlags& latches = Audio_Driver_controlLatches;
  latches.controlLatches = state.control_latches;
  latches.mixBehavior = state.mix_behavior;
  latches.updateFlags = state.driver_update_flags;
  Audio_Cutscene_streamControlFlags = state.stream_control_flags;
  StoreMusicLayer(*Audio_Seq_activeMusicConfig, state.active);
  if (Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
      secondary != nullptr && state.secondary &&
      secondary != Audio_Seq_activeMusicConfig) {
    StoreMusicLayer(*secondary, *state.secondary);
  }
  StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Seq_musicLoopsRemaining = state.music_loops_remaining;
  Audio_Seq_musicTransitionTimer = state.music_transition_timer;
  Audio_Seq_tickCounter = state.tick_counter;
  Audio_Seq_masterFade.value = state.master_fade.value;
  Audio_Seq_masterFade.step = state.master_fade.step;
  Audio_Seq_masterFade.ticksRemaining = state.master_fade.ticks_remaining;
  Audio_Seq_randomPitchPhase = state.random_pitch_phase;
  Audio_Seq_discVolume = state.disc_volume.value;
  Audio_Seq_discVolumeFadeStep = state.disc_volume.step;
  Audio_Seq_discVolumeFadeLength = state.disc_volume.length;
  Audio_Mixer_tempoScale = state.tempo_scale.value;
  Audio_Mixer_tempoScaleStep = state.tempo_scale.step;
  Audio_Mixer_tempoScaleSlideLength = state.tempo_scale.length;
  Audio_Mixer_musicPitchScale = state.music_pitch_scale.value;
  Audio_Mixer_musicPitchScaleStep = state.music_pitch_scale.step;
  Audio_Mixer_musicPitchScaleSlideLength = state.music_pitch_scale.length;
  std::copy(state.recent_tick_costs.begin(), state.recent_tick_costs.end(),
            std::begin(Audio_Driver_recentTickCosts));
  Audio_Driver_recentTickCostTotal = state.recent_tick_cost_total;
}

const char* PlaybackRefusalReason(PlaybackRefusal refusal) {
  switch (refusal) {
    case PlaybackRefusal::kSecondaryLayerMissing:
      return "a transition named the secondary music layer while none plays";
    case PlaybackRefusal::kEffectVoiceOutsideChannels:
      return "sound-effect voices below the first sound-effect channel are "
             "active";
    case PlaybackRefusal::kLoopFastForwardLimit:
      return "the fast-forward to the requested loop never reached it";
  }
  return "refused";
}

// The music layer and channels `group` names; empty for sound effects and a
// missing secondary layer.
struct LiveMusicGroup {
  Audio_MusicConfig* config = nullptr;
  Audio_SeqChannel* channels = nullptr;
};

LiveMusicGroup MusicGroupOf(ChannelGroup group) {
  if (group == ChannelGroup::kMusic) {
    return {Audio_Seq_activeMusicConfig, std::data(Audio_Seq_musicChannels)};
  }
  if (group == ChannelGroup::kSecondaryMusic) {
    return {Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels};
  }
  return {};
}

// One driver tick over the live driver state. Every effect that takes the
// state stores it before and loads it after.
class LivePlaybackHost final : public PlaybackHost {
 public:
  std::int32_t ReadTimer() override {
    return static_cast<std::int32_t>(GetRCnt(RCntCNT2));
  }

  void ReleasePendingVoices(PlaybackState& state) override {
    StorePlayback(state);
    Mixer::LiveReleasePendingVoices();
    LoadPlayback(state);
  }

  void DetachSecondaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    Audio_Seq_secondaryMusicConfig = nullptr;
    LoadPlayback(state);
  }

  // func_80019A58 copies the PS1 layer record (0x80 bytes) and its 32
  // channels (0x2200 bytes); the native records are copied whole.
  void PromoteSecondaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    Audio_MusicConfig* promoted = Audio_Seq_secondaryMusicConfig;
    Audio_SeqChannel* channels = Audio_Seq_secondaryMusicChannels;
    if (promoted != Audio_Seq_activeMusicConfig) {
      std::memcpy(Audio_Seq_activeMusicConfig, promoted,
                  offsetof(Audio_MusicConfig, reserved80));
    }
    if (channels != nullptr && channels != std::data(Audio_Seq_musicChannels)) {
      std::copy_n(channels, kMusicChannelCount,
                  std::data(Audio_Seq_musicChannels));
    }
    Audio_Seq_secondaryMusicConfig = nullptr;
    promoted->musicId = 0;
    promoted->activeChannels = 0;
    LoadPlayback(state);
  }

  void StartPendingMusic(PlaybackState& state) override {
    StorePlayback(state);
    const std::uintptr_t pending = Audio_Seq_pendingMusicData.param1;
    auto* const start = reinterpret_cast<char*>(pending);
    const std::optional<LiveProgramMemory> region = LiveProgramRegion(start);
    std::uint16_t music_id = 0;
    if (!region || region->From(start).size < kPendingMusicHeaderBytes) {
      ReportRefusal("TickPlayback",
                    "the pending track lies outside the sequence data");
    } else {
      std::memcpy(&music_id, start + kPendingMusicIdOffset, sizeof(music_id));
      Audio_Seq_embeddedSoundEffectParams.param1 = pending;
      Audio_Seq_embeddedSoundEffectParams.param3 = music_id;
      LiveStartOrResumeMusic(&Audio_Seq_embeddedSoundEffectParams);
    }
    Audio_Seq_pendingMusicData.param1 = 0;
    LoadPlayback(state);
  }

  void CommitVoices(PlaybackState& state) override {
    StorePlayback(state);
    Mixer::LiveCommitPlaybackVoices();
    LoadPlayback(state);
  }

  ChannelTimers Timers(ChannelGroup group, std::size_t index) override {
    const Audio_SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return {};
    return {Same<std::uint16_t>(channel->noteLength),
            Same<std::uint16_t>(channel->gateLength),
            Same<std::int32_t>(channel->priority),
            Same<std::int32_t>(channel->effectTags)};
  }

  void SetTimers(ChannelGroup group, std::size_t index,
                 const ChannelTimers& timers) override {
    Audio_SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return;
    channel->noteLength = timers.note_length;
    channel->gateLength = timers.gate_length;
    channel->priority = timers.priority;
    channel->effectTags = timers.effect_tags;
  }

  void StepChannel(PlaybackState& state, ChannelGroup group,
                   std::size_t index) override {
    Audio_SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return;
    StorePlayback(state);
    {
      const ScopedActiveLayer layer(group);
      LiveExecuteChannelStep(channel,
                             static_cast<int>(ChannelBit(group, index)));
    }
    LoadPlayback(state);
  }

  void UpdateChannelSlides(PlaybackState& state, ChannelGroup group,
                           std::size_t index) override {
    Audio_SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return;
    StorePlayback(state);
    {
      const ScopedActiveLayer layer(group);
      Mixer::LiveUpdateChannelSlidesAndDelays(
          channel, ChannelBit(group, index),
          group == ChannelGroup::kSoundEffects
              ? Driver::ChannelKind::SoundEffect
              : Driver::ChannelKind::Music);
    }
    LoadPlayback(state);
  }

  void ActivatePrimaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
    LoadPlayback(state);
  }

  void InvalidateMusicVolumes(PlaybackState& state,
                              ChannelGroup group) override {
    const LiveMusicGroup music = MusicGroupOf(group);
    if (music.config == nullptr || music.channels == nullptr) return;
    StorePlayback(state);
    SoundEffects::LiveInvalidateMusicChannelVolumes(music.config,
                                                    music.channels);
    LoadPlayback(state);
  }

  void MarkMusicPitchDirty() override {
    for (Audio_SeqChannel& channel : Audio_Seq_musicChannels) {
      channel.voice.dirtyFlags |= kSampleRateDirtyFlag;
    }
  }

  EffectSlides EffectSlidesOf(std::size_t index) override {
    const Audio_SeqChannel* channel =
        ChannelOf(ChannelGroup::kSoundEffects, index);
    if (channel == nullptr) return {};
    return {Same<std::int16_t>(channel->effectVolume),
            Same<std::int16_t>(channel->effectVolumeSlideStep),
            Same<std::uint16_t>(channel->effectVolumeSlideLength),
            Same<std::uint16_t>(channel->effectPan),
            Same<std::int16_t>(channel->effectPanSlideStep),
            Same<std::uint16_t>(channel->effectPanSlideLength),
            Same<std::int32_t>(channel->effectPitchScale),
            Same<std::uint32_t>(channel->effectPitchScaleSlideStep),
            Same<std::uint16_t>(channel->effectPitchScaleSlideLength),
            Same<std::uint32_t>(channel->voice.dirtyFlags)};
  }

  void SetEffectSlides(std::size_t index, const EffectSlides& slides) override {
    Audio_SeqChannel* channel = ChannelOf(ChannelGroup::kSoundEffects, index);
    if (channel == nullptr) return;
    channel->effectVolume = slides.volume;
    channel->effectVolumeSlideStep = slides.volume_step;
    channel->effectVolumeSlideLength = slides.volume_length;
    channel->effectPan = slides.pan;
    channel->effectPanSlideStep = slides.pan_step;
    channel->effectPanSlideLength = slides.pan_length;
    channel->effectPitchScale = slides.pitch_scale;
    channel->effectPitchScaleSlideStep = slides.pitch_scale_step;
    channel->effectPitchScaleSlideLength = slides.pitch_scale_length;
    channel->voice.dirtyFlags = slides.dirty_flags;
  }

  void WriteDiscVolume(PlaybackState& state) override {
    StorePlayback(state);
    WriteLiveDiscVolume();
    LoadPlayback(state);
  }

  void UpdateCutsceneStream(PlaybackState& state) override {
    StorePlayback(state);
    Audio_Native_UpdateCutsceneStream();
    LoadPlayback(state);
  }

  void FinishTick(PlaybackState& state) override {
    StorePlayback(state);
    Audio_Native_Tick();
    LoadPlayback(state);
  }

  void Refuse(PlaybackRefusal refusal) override {
    ReportRefusal("TickPlayback", PlaybackRefusalReason(refusal));
  }

 private:
  // A pending track's sequence starts with its AKAO header; its music id is
  // the halfword at byte 4 (func_80019A58 reads p[2]).
  static constexpr std::size_t kPendingMusicIdOffset = 4;
  static constexpr std::size_t kPendingMusicHeaderBytes =
      kPendingMusicIdOffset + sizeof(std::uint16_t);
  static constexpr std::uint32_t kSampleRateDirtyFlag = 0x10;
};

// The driver tick needs an active music layer, and a secondary layer distinct
// from it.
bool PlaybackLayersValid(const char* operation) {
  if (Audio_Seq_activeMusicConfig == nullptr) {
    ReportRefusal(operation, "no music layer is active");
    return false;
  }
  if (Audio_Seq_secondaryMusicConfig == Audio_Seq_activeMusicConfig) {
    ReportRefusal(operation, "the secondary music layer is the active one");
    return false;
  }
  return true;
}
}  // namespace

// decomp: SLUS_010.40 func_8001A8D8 (src/SLUS_010.40/25AC.c:4760)
void LiveExecuteChannelStep(Audio_SeqChannel* channel, int channel_bit) {
  if (channel == nullptr || Audio_Seq_activeMusicConfig == nullptr) {
    ReportRefusal("ExecuteChannelStep", "no channel or music layer");
    return;
  }
  LiveStepHost host(*channel);
  StepState state;
  host.LoadStep(state);
  ExecuteChannelStep(state, host, static_cast<std::uint32_t>(channel_bit));
  host.StoreStep(state);
}

// decomp: SLUS_010.40 func_80019A58 (src/SLUS_010.40/25AC.c:4287)
CounterEventResult LiveTickPlayback() {
  if (!PlaybackLayersValid("TickPlayback")) return 0;
  LivePlaybackHost host;
  PlaybackState state;
  LoadPlayback(state);
  const std::int32_t cost = TickPlayback(state, host);
  StorePlayback(state);
  return cost;
}

// decomp: SLUS_010.40 func_8001924C (src/SLUS_010.40/25AC.c:4024)
void LiveUpdateGlobalSlides() {
  if (!PlaybackLayersValid("UpdateGlobalSlides")) return;
  LivePlaybackHost host;
  PlaybackState state;
  LoadPlayback(state);
  UpdateGlobalSlides(state, host);
  StorePlayback(state);
}

// decomp: SLUS_010.40 Sound_ClearVoiceFromSchedulerState
// (src/SLUS_010.40/25AC.c:4970)
void LiveReleaseSoundEffectVoice(Audio_SeqChannel* channel, int voice_bit) {
  if (channel == nullptr) {
    ReportRefusal("ReleaseSoundEffectVoice", "no channel");
    return;
  }
  VoiceSchedulerState scheduler =
      LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  EffectIdentity effect{Same<std::int32_t>(channel->effectTags),
                        channel->effectId != 0};
  ReleaseSoundEffectVoice(scheduler, effect,
                          static_cast<std::uint32_t>(voice_bit));
  StoreVoiceScheduler(Audio_Mixer_voiceScheduler, scheduler);
  channel->effectTags = effect.tags;
  if (!effect.has_effect) channel->effectId = 0;
}
}  // namespace VS::Audio::Seq
