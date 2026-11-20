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

#include "Audio/Control/SoundControl.hpp"
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/StreamPlayback.hpp"
#include "Audio/VoiceMixerLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Audio/XaMixLive.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Audio::Seq {
namespace {
using OpcodeHandler = SeqOpcodeHandler;

static_assert(std::size(VS::Audio::Seq::kOpcodeHandlers) == kOpcodeCount);
static_assert(std::size(VS::Audio::Seq::kExtendedOpcodeHandlers) ==
              kExtendedOpcodeCount);
static_assert(std::size(VS::Audio::Seq::kOpcodeLengths) >= kOpcodeCount);
static_assert(std::size(VS::Audio::Seq::kExtendedOpcodeLengths) >=
              kExtendedOpcodeCount);
static_assert(std::size(VS::Audio::Seq::kNoteLengths) == kNoteLengthCount);
static_assert(std::size(VS::Audio::Seq::kSemitonePitches) >= kSemitoneCount);
static_assert(std::size(VS::Audio::Seq::kRandomPitchOffsets) ==
              kRandomPitchOffsetCount);
static_assert(std::size(VS::Audio::Seq::musicChannels) == kMusicChannelCount);
static_assert(std::size(VS::Audio::SoundEffects::channels) >=
              kEffectChannelCount);
static_assert(std::size(VS::Audio::Driver::recentTickCosts) ==
              std::tuple_size_v<decltype(PlaybackState::recent_tick_costs)>);
static_assert(sizeof(VS::Audio::Seq::kOpcodeLengths[0]) ==
              sizeof(std::uint8_t));
static_assert(sizeof(VS::Audio::Seq::kNoteLengths[0]) == sizeof(std::uint16_t));
static_assert(sizeof(VS::Audio::Seq::kSemitonePitches[0]) ==
              sizeof(std::uint32_t));

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

const SequencerTables& LiveTables() {
  static const SequencerTables tables{
      std::span<const std::uint8_t>(VS::Audio::Seq::kOpcodeLengths,
                                    kOpcodeCount),
      std::span<const std::uint8_t>(VS::Audio::Seq::kExtendedOpcodeLengths,
                                    kExtendedOpcodeCount),
      std::span<const std::uint16_t, kNoteLengthCount>(
          VS::Audio::Seq::kNoteLengths),
      std::span<const std::uint32_t, kSemitoneCount>(
          std::data(VS::Audio::Seq::kSemitonePitches), kSemitoneCount),
      std::span<const std::int8_t>(VS::Audio::Seq::kNoteRemapTables),
      std::span<const std::uint8_t, kRandomPitchOffsetCount>(
          VS::Audio::Seq::kRandomPitchOffsets),
  };
  return tables;
}

// The bytes from `start` to the end of the program memory it lies in.
std::span<const std::uint8_t> BytesFrom(std::uint8_t* start) {
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
  explicit LiveStepHost(VS::Audio::SeqChannel& channel)
      : channel_(channel),
        region_(LiveChannelProgram(channel).value_or(LiveProgramMemory{})) {}

  [[nodiscard]] const LiveProgramMemory& Region() const { return region_; }

  void LoadStep(StepState& state) const {
    const SequenceChannel record = LoadSequenceChannel(channel_, region_);
    state.channel = record.channel;
    state.program = record.program;
    state.pitch_slide = record.pitch_slide;
    state.assigned_voice = record.assigned_voice;
    state.layer = LoadMusicLayer(*VS::Audio::Seq::activeMusicConfig);
    state.scheduler = LoadVoiceScheduler(VS::Audio::Mixer::voiceScheduler);
    state.mix_behavior = VS::Audio::Driver::controlLatches.mix_behavior;
    state.driver_update_flags = VS::Audio::Driver::controlLatches.update_flags;
    state.random_pitch_phase = VS::Audio::Seq::randomPitchPhase;
  }

  void StoreStep(const StepState& state) {
    StoreSequenceChannel(channel_,
                         {state.channel, state.program, state.pitch_slide,
                          state.assigned_voice, false},
                         region_);
    StoreMusicLayer(*VS::Audio::Seq::activeMusicConfig, state.layer);
    StoreVoiceScheduler(VS::Audio::Mixer::voiceScheduler, state.scheduler);
    VS::Audio::Driver::controlLatches.update_flags = state.driver_update_flags;
  }

  ProgramData Program() override { return region_.View(); }

  const SequencerTables& Tables() override { return LiveTables(); }

  CommandOutcome RunCommand(StepState& state, std::uint8_t opcode,
                            std::uint32_t channel_bit) override {
    const std::size_t index = std::size_t{opcode} - kEndOfTrackOpcode;
    if (index >= std::size(VS::Audio::Seq::kOpcodeHandlers)) {
      return CommandOutcome::kMissing;
    }
    return Run(state, VS::Audio::Seq::kOpcodeHandlers[index], channel_bit);
  }

  CommandOutcome RunExtendedCommand(StepState& state, std::uint8_t opcode,
                                    std::uint32_t channel_bit) override {
    if (opcode >= std::size(VS::Audio::Seq::kExtendedOpcodeHandlers)) {
      return CommandOutcome::kMissing;
    }
    return Run(state, VS::Audio::Seq::kExtendedOpcodeHandlers[opcode],
               channel_bit);
  }

  std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) override {
    return LiveInstrument(index);
  }

  std::span<const std::uint8_t> ChannelKeymap() override {
    return BytesFrom(channel_.keymap);
  }

  std::span<const std::uint8_t> DrumKeymap() override {
    return BytesFrom(reinterpret_cast<std::uint8_t*>(
        VS::Audio::Seq::activeMusicConfig->keymap_table));
  }

  bool HasLfoWaveform(std::uint16_t shape) override {
    return shape < std::size(VS::Audio::Seq::kLfoWaveforms) &&
           VS::Audio::Seq::kLfoWaveforms[shape] != nullptr;
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

  VS::Audio::SeqChannel& channel_;
  LiveProgramMemory region_;
};

std::uint32_t ChannelBit(ChannelGroup group, std::size_t index) {
  if (group == ChannelGroup::kSoundEffects) {
    return 1U << (kFirstEffectVoice + index);
  }
  return 1U << index;
}

// The channel table `group` names: empty without a secondary layer's.
VS::Audio::SeqChannel* ChannelOf(ChannelGroup group, std::size_t index) {
  switch (group) {
    case ChannelGroup::kMusic:
      if (index < std::size(VS::Audio::Seq::musicChannels)) {
        return &VS::Audio::Seq::musicChannels[index];
      }
      break;
    case ChannelGroup::kSecondaryMusic:
      if (VS::Audio::Seq::secondaryMusicChannels != nullptr &&
          index < kMusicChannelCount) {
        return &VS::Audio::Seq::secondaryMusicChannels[index];
      }
      break;
    case ChannelGroup::kSoundEffects:
      if (index < kEffectChannelCount) {
        return &VS::Audio::SoundEffects::channels[index];
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
      : previous_(VS::Audio::Seq::activeMusicConfig) {
    if (group == ChannelGroup::kSecondaryMusic) {
      VS::Audio::Seq::activeMusicConfig = VS::Audio::Seq::secondaryMusicConfig;
    }
  }
  ~ScopedActiveLayer() { VS::Audio::Seq::activeMusicConfig = previous_; }
  ScopedActiveLayer(const ScopedActiveLayer&) = delete;
  ScopedActiveLayer& operator=(const ScopedActiveLayer&) = delete;

 private:
  VS::Audio::MusicConfig* previous_;
};

LevelSlide LoadSlide(std::int32_t value, std::int32_t step,
                     std::int16_t length) {
  return {value, step, length};
}

void LoadPlayback(PlaybackState& state) {
  const VS::Audio::DriverFlags& latches = VS::Audio::Driver::controlLatches;
  state.control_latches = latches.control_latches;
  state.mix_behavior = latches.mix_behavior;
  state.driver_update_flags = latches.update_flags;
  state.stream_control_flags = VS::Audio::Cutscene::streamState.control_flags;
  state.active = LoadMusicLayer(*VS::Audio::Seq::activeMusicConfig);
  state.active_is_primary =
      VS::Audio::Seq::activeMusicConfig == &VS::Audio::Seq::primaryMusicConfig;
  state.secondary.reset();
  if (const VS::Audio::MusicConfig* secondary =
          VS::Audio::Seq::secondaryMusicConfig) {
    state.secondary = LoadMusicLayer(*secondary);
  }
  state.scheduler = LoadVoiceScheduler(VS::Audio::Mixer::voiceScheduler);
  state.music_loops_remaining = VS::Audio::Seq::musicLoopsRemaining;
  state.music_transition_timer = VS::Audio::Driver::commandArguments.ext_param2;
  state.tick_counter = Same<std::uint16_t>(VS::Audio::Seq::tickCounter);
  state.music_pending = VS::Audio::Seq::pendingMusicData.param1 != 0;
  state.master_fade = {VS::Audio::Seq::masterFade.value,
                       VS::Audio::Seq::masterFade.step,
                       VS::Audio::Seq::masterFade.ticks_remaining};
  state.random_pitch_phase = VS::Audio::Seq::randomPitchPhase;
  state.disc_volume =
      LoadSlide(VS::Audio::Seq::discVolume, VS::Audio::Seq::discVolumeFadeStep,
                VS::Audio::Seq::discVolumeFadeLength);
  state.tempo_scale =
      LoadSlide(VS::Audio::Mixer::tempoScale, VS::Audio::Mixer::tempoScaleStep,
                VS::Audio::Mixer::tempoScaleSlideLength);
  state.music_pitch_scale = LoadSlide(
      VS::Audio::Mixer::musicPitchScale, VS::Audio::Mixer::musicPitchScaleStep,
      VS::Audio::Mixer::musicPitchScaleSlideLength);
  std::copy(std::begin(VS::Audio::Driver::recentTickCosts),
            std::end(VS::Audio::Driver::recentTickCosts),
            state.recent_tick_costs.begin());
  state.recent_tick_cost_total = VS::Audio::Driver::recentTickCostTotal;
}

void StorePlayback(const PlaybackState& state) {
  VS::Audio::DriverFlags& latches = VS::Audio::Driver::controlLatches;
  latches.control_latches = state.control_latches;
  latches.mix_behavior = state.mix_behavior;
  latches.update_flags = state.driver_update_flags;
  VS::Audio::Cutscene::streamState.control_flags = state.stream_control_flags;
  StoreMusicLayer(*VS::Audio::Seq::activeMusicConfig, state.active);
  if (VS::Audio::MusicConfig* secondary = VS::Audio::Seq::secondaryMusicConfig;
      secondary != nullptr && state.secondary &&
      secondary != VS::Audio::Seq::activeMusicConfig) {
    StoreMusicLayer(*secondary, *state.secondary);
  }
  StoreVoiceScheduler(VS::Audio::Mixer::voiceScheduler, state.scheduler);
  VS::Audio::Seq::musicLoopsRemaining = state.music_loops_remaining;
  VS::Audio::Driver::commandArguments.ext_param2 = state.music_transition_timer;
  VS::Audio::Seq::tickCounter = state.tick_counter;
  VS::Audio::Seq::masterFade.value = state.master_fade.value;
  VS::Audio::Seq::masterFade.step = state.master_fade.step;
  VS::Audio::Seq::masterFade.ticks_remaining =
      state.master_fade.ticks_remaining;
  VS::Audio::Seq::randomPitchPhase = state.random_pitch_phase;
  VS::Audio::Seq::discVolume = state.disc_volume.value;
  VS::Audio::Seq::discVolumeFadeStep = state.disc_volume.step;
  VS::Audio::Seq::discVolumeFadeLength = state.disc_volume.length;
  VS::Audio::Mixer::tempoScale = state.tempo_scale.value;
  VS::Audio::Mixer::tempoScaleStep = state.tempo_scale.step;
  VS::Audio::Mixer::tempoScaleSlideLength = state.tempo_scale.length;
  VS::Audio::Mixer::musicPitchScale = state.music_pitch_scale.value;
  VS::Audio::Mixer::musicPitchScaleStep = state.music_pitch_scale.step;
  VS::Audio::Mixer::musicPitchScaleSlideLength = state.music_pitch_scale.length;
  std::copy(state.recent_tick_costs.begin(), state.recent_tick_costs.end(),
            std::begin(VS::Audio::Driver::recentTickCosts));
  VS::Audio::Driver::recentTickCostTotal = state.recent_tick_cost_total;
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
  VS::Audio::MusicConfig* config = nullptr;
  VS::Audio::SeqChannel* channels = nullptr;
};

LiveMusicGroup MusicGroupOf(ChannelGroup group) {
  if (group == ChannelGroup::kMusic) {
    return {VS::Audio::Seq::activeMusicConfig,
            std::data(VS::Audio::Seq::musicChannels)};
  }
  if (group == ChannelGroup::kSecondaryMusic) {
    return {VS::Audio::Seq::secondaryMusicConfig,
            VS::Audio::Seq::secondaryMusicChannels};
  }
  return {};
}

// One driver tick over the live driver state. Every effect that takes the
// state stores it before and loads it after.
class LivePlaybackHost final : public PlaybackHost {
 public:
  std::int32_t ReadTimer() override {
    return static_cast<std::int32_t>(
        VS::Legacy::GetRCnt(VS::Legacy::kRootCounter2));
  }

  void ReleasePendingVoices(PlaybackState& state) override {
    StorePlayback(state);
    Mixer::LiveReleasePendingVoices();
    LoadPlayback(state);
  }

  void DetachSecondaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    VS::Audio::Seq::secondaryMusicConfig = nullptr;
    LoadPlayback(state);
  }

  // func_80019A58 copies the PS1 layer record (0x80 bytes) and its 32
  // channels (0x2200 bytes); the native records are copied whole.
  void PromoteSecondaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    VS::Audio::MusicConfig* promoted = VS::Audio::Seq::secondaryMusicConfig;
    VS::Audio::SeqChannel* channels = VS::Audio::Seq::secondaryMusicChannels;
    if (promoted != VS::Audio::Seq::activeMusicConfig) {
      std::memcpy(VS::Audio::Seq::activeMusicConfig, promoted,
                  offsetof(VS::Audio::MusicConfig, reserved80));
    }
    if (channels != nullptr &&
        channels != std::data(VS::Audio::Seq::musicChannels)) {
      std::copy_n(channels, kMusicChannelCount,
                  std::data(VS::Audio::Seq::musicChannels));
    }
    VS::Audio::Seq::secondaryMusicConfig = nullptr;
    promoted->music_id = 0;
    promoted->active_channels = 0;
    LoadPlayback(state);
  }

  void StartPendingMusic(PlaybackState& state) override {
    StorePlayback(state);
    const std::uintptr_t pending = VS::Audio::Seq::pendingMusicData.param1;
    auto* const start = reinterpret_cast<std::uint8_t*>(pending);
    const std::optional<LiveProgramMemory> region = LiveProgramRegion(start);
    std::uint16_t music_id = 0;
    if (!region || region->From(start).size < kPendingMusicHeaderBytes) {
      ReportRefusal("TickPlayback",
                    "the pending track lies outside the sequence data");
    } else {
      std::memcpy(&music_id, start + kPendingMusicIdOffset, sizeof(music_id));
      VS::Audio::Seq::embeddedSoundEffectParams.param1 = pending;
      VS::Audio::Seq::embeddedSoundEffectParams.param3 = music_id;
      LiveStartOrResumeMusic(&VS::Audio::Seq::embeddedSoundEffectParams);
    }
    VS::Audio::Seq::pendingMusicData.param1 = 0;
    LoadPlayback(state);
  }

  void CommitVoices(PlaybackState& state) override {
    StorePlayback(state);
    Mixer::LiveCommitPlaybackVoices();
    LoadPlayback(state);
  }

  ChannelTimers Timers(ChannelGroup group, std::size_t index) override {
    const VS::Audio::SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return {};
    return {Same<std::uint16_t>(channel->note_length),
            Same<std::uint16_t>(channel->gate_length),
            Same<std::int32_t>(channel->priority),
            Same<std::int32_t>(channel->effect_tags)};
  }

  void SetTimers(ChannelGroup group, std::size_t index,
                 const ChannelTimers& timers) override {
    VS::Audio::SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return;
    channel->note_length = timers.note_length;
    channel->gate_length = timers.gate_length;
    channel->priority = timers.priority;
    channel->effect_tags = timers.effect_tags;
  }

  void StepChannel(PlaybackState& state, ChannelGroup group,
                   std::size_t index) override {
    VS::Audio::SeqChannel* channel = ChannelOf(group, index);
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
    VS::Audio::SeqChannel* channel = ChannelOf(group, index);
    if (channel == nullptr) return;
    StorePlayback(state);
    {
      const ScopedActiveLayer layer(group);
      Mixer::LiveUpdateChannelSlidesAndDelays(
          channel, ChannelBit(group, index),
          group == ChannelGroup::kSoundEffects
              ? Driver::ChannelKind::kSoundEffect
              : Driver::ChannelKind::kMusic);
    }
    LoadPlayback(state);
  }

  void ActivatePrimaryLayer(PlaybackState& state) override {
    StorePlayback(state);
    VS::Audio::Seq::activeMusicConfig = &VS::Audio::Seq::primaryMusicConfig;
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
    for (VS::Audio::SeqChannel& channel : VS::Audio::Seq::musicChannels) {
      channel.voice.dirty_flags |= kSampleRateDirtyFlag;
    }
  }

  EffectSlides EffectSlidesOf(std::size_t index) override {
    const VS::Audio::SeqChannel* channel =
        ChannelOf(ChannelGroup::kSoundEffects, index);
    if (channel == nullptr) return {};
    return {Same<std::int16_t>(channel->effect_volume),
            Same<std::int16_t>(channel->effect_volume_slide_step),
            Same<std::uint16_t>(channel->effect_volume_slide_length),
            Same<std::uint16_t>(channel->effect_pan),
            Same<std::int16_t>(channel->effect_pan_slide_step),
            Same<std::uint16_t>(channel->effect_pan_slide_length),
            Same<std::int32_t>(channel->effect_pitch_scale),
            Same<std::uint32_t>(channel->effect_pitch_scale_slide_step),
            Same<std::uint16_t>(channel->effect_pitch_scale_slide_length),
            Same<std::uint32_t>(channel->voice.dirty_flags)};
  }

  void SetEffectSlides(std::size_t index, const EffectSlides& slides) override {
    VS::Audio::SeqChannel* channel =
        ChannelOf(ChannelGroup::kSoundEffects, index);
    if (channel == nullptr) return;
    channel->effect_volume = slides.volume;
    channel->effect_volume_slide_step = slides.volume_step;
    channel->effect_volume_slide_length = slides.volume_length;
    channel->effect_pan = slides.pan;
    channel->effect_pan_slide_step = slides.pan_step;
    channel->effect_pan_slide_length = slides.pan_length;
    channel->effect_pitch_scale = slides.pitch_scale;
    channel->effect_pitch_scale_slide_step = slides.pitch_scale_step;
    channel->effect_pitch_scale_slide_length = slides.pitch_scale_length;
    channel->voice.dirty_flags = slides.dirty_flags;
  }

  void WriteDiscVolume(PlaybackState& state) override {
    StorePlayback(state);
    WriteLiveDiscVolume();
    LoadPlayback(state);
  }

  void UpdateCutsceneStream(PlaybackState& state) override {
    StorePlayback(state);
    VS::Audio::UpdateCutsceneStream();
    LoadPlayback(state);
  }

  void FinishTick(PlaybackState& state) override {
    StorePlayback(state);
    VS::Audio::Driver::TickVoiceOutput();
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
  if (VS::Audio::Seq::activeMusicConfig == nullptr) {
    ReportRefusal(operation, "no music layer is active");
    return false;
  }
  if (VS::Audio::Seq::secondaryMusicConfig ==
      VS::Audio::Seq::activeMusicConfig) {
    ReportRefusal(operation, "the secondary music layer is the active one");
    return false;
  }
  return true;
}
}  // namespace

// decomp: SLUS_010.40 func_8001A8D8 (src/SLUS_010.40/25AC.c:4760)
void LiveExecuteChannelStep(VS::Audio::SeqChannel* channel, int channel_bit) {
  if (channel == nullptr || VS::Audio::Seq::activeMusicConfig == nullptr) {
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
void LiveReleaseSoundEffectVoice(VS::Audio::SeqChannel* channel,
                                 int voice_bit) {
  if (channel == nullptr) {
    ReportRefusal("ReleaseSoundEffectVoice", "no channel");
    return;
  }
  VoiceSchedulerState scheduler =
      LoadVoiceScheduler(VS::Audio::Mixer::voiceScheduler);
  EffectIdentity effect{Same<std::int32_t>(channel->effect_tags),
                        channel->effect_id != 0};
  ReleaseSoundEffectVoice(scheduler, effect,
                          static_cast<std::uint32_t>(voice_bit));
  StoreVoiceScheduler(VS::Audio::Mixer::voiceScheduler, scheduler);
  channel->effect_tags = effect.tags;
  if (!effect.has_effect) channel->effect_id = 0;
}
}  // namespace VS::Audio::Seq
