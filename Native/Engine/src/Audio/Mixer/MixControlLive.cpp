// Hand-maintained (tools/port/owned.csv).
// Live binding for MixControl.hpp: the sound-effect and music channel
// tables, the active, secondary and saved music layers, the voice scheduler,
// the driver flags, the cutscene stream state, the command arguments and
// parameters, the command table, the reverb unit and the voice output.
#include "Audio/MixControlLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/MixControl.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/TickTimer.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Legacy/Psx.hpp"

using VS::Legacy::kSpuOff;
using VS::Legacy::kSpuOn;

namespace VS::Audio::Mixer {
namespace {
static_assert(std::size(VS::Audio::Seq::musicChannels) ==
              Seq::kMusicChannelCount);
static_assert(std::size(VS::Audio::Seq::savedMusicChannels) ==
              Seq::kMusicChannelCount);
static_assert(std::size(VS::Audio::SoundEffects::channels) >=
              kEffectChannelCount);
// func_80018C30 dispatches on the command's low byte.
constexpr std::size_t kCommandCount = 0x100;
static_assert(std::size(VS::Audio::Mixer::kCommandHandlers) == kCommandCount);
static_assert(sizeof(VS::Audio::Driver::commandArguments.param2) ==
              sizeof(std::uint32_t));
static_assert(sizeof(VS::Audio::Driver::legacySpuEvent) ==
              sizeof(std::int32_t));
static_assert(std::is_convertible_v<decltype(&LiveSetTempoScale),
                                    VS::Audio::CommandHandler>);

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

const char* RefusalReason(CommandRefusal refusal) {
  switch (refusal) {
    case CommandRefusal::kZeroLengthSlide:
      return "a slide of zero 16-bit ticks runs over one tick";
    case CommandRefusal::kSlideStepOverflow:
      return "a slide divides INT_MIN by -1";
    case CommandRefusal::kActiveLayerMissing:
      return "no music layer is active";
    case CommandRefusal::kEffectVoiceOutsideChannels:
      return "paused sound-effect voices lie below the first sound-effect "
             "channel";
    case CommandRefusal::kSequenceUnreadable:
      return "a sequence argument lies outside the sequence memory";
    case CommandRefusal::kNoCommandHandler:
      return "the command table has no handler for the command";
  }
  return "refused";
}

Seq::MusicCommand CommandOf(const VS::Audio::CommandParams& params) {
  return {static_cast<std::uint32_t>(params.param1),
          Same<std::uint32_t>(params.param2),
          Same<std::uint32_t>(params.param3),
          Same<std::uint32_t>(params.param4),
          Same<std::uint32_t>(params.ext_param1),
          Same<std::uint32_t>(params.ext_param2)};
}

// param1 is the selected effect's whole native identity.
EffectSelection SelectionOf(const VS::Audio::CommandParams& params) {
  static_assert(sizeof(params.param1) == sizeof(EffectId));
  return {static_cast<EffectId>(params.param1),
          Same<std::uint32_t>(params.param2)};
}

// The secondary layer's record when it is one of its own.
VS::Audio::MusicConfig* SecondaryRecord() {
  VS::Audio::MusicConfig* secondary = VS::Audio::Seq::secondaryMusicConfig;
  return secondary != VS::Audio::Seq::activeMusicConfig ? secondary : nullptr;
}

EffectControl LoadEffect(const VS::Audio::SeqChannel& legacy) {
  EffectControl effect;
  effect.tags = Same<std::int32_t>(legacy.effect_tags);
  effect.effect_id = static_cast<EffectId>(legacy.effect_id);
  effect.update_flags = Same<std::uint32_t>(legacy.update_flags);
  effect.pitch_scale = Same<std::int32_t>(legacy.effect_pitch_scale);
  effect.pitch_scale_step =
      Same<std::uint32_t>(legacy.effect_pitch_scale_slide_step);
  effect.pitch_scale_length =
      Same<std::uint16_t>(legacy.effect_pitch_scale_slide_length);
  effect.dirty_flags = Same<std::uint32_t>(legacy.voice.dirty_flags);
  return effect;
}

void StoreEffect(VS::Audio::SeqChannel& legacy, const EffectControl& effect) {
  legacy.effect_tags = effect.tags;
  legacy.effect_id = static_cast<std::uintptr_t>(effect.effect_id);
  legacy.update_flags = effect.update_flags;
  legacy.effect_pitch_scale = effect.pitch_scale;
  legacy.effect_pitch_scale_slide_step = effect.pitch_scale_step;
  legacy.effect_pitch_scale_slide_length = effect.pitch_scale_length;
  legacy.voice.dirty_flags = effect.dirty_flags;
}

CommandState LoadCommandState() {
  CommandState state;
  for (std::size_t i = 0; i < state.effects.size(); ++i) {
    state.effects[i] = LoadEffect(VS::Audio::SoundEffects::channels[i]);
  }
  for (std::size_t i = 0; i < state.music_dirty_flags.size(); ++i) {
    state.music_dirty_flags[i] =
        Same<std::uint32_t>(VS::Audio::Seq::musicChannels[i].voice.dirty_flags);
  }
  if (const VS::Audio::MusicConfig* active =
          VS::Audio::Seq::activeMusicConfig) {
    state.active = Seq::LoadMusicLayer(*active);
  }
  state.has_secondary = VS::Audio::Seq::secondaryMusicConfig != nullptr;
  state.scheduler = Seq::LoadVoiceScheduler(VS::Audio::Mixer::voiceScheduler);
  state.mix_behavior =
      Same<std::uint32_t>(VS::Audio::Driver::controlLatches.mix_behavior);
  state.driver_update_flags =
      Same<std::uint32_t>(VS::Audio::Driver::controlLatches.update_flags);
  state.muted_music_channels =
      Same<std::int32_t>(VS::Audio::Mixer::mutedMusicChannels);
  state.stream_voices =
      Same<std::uint32_t>(VS::Audio::Cutscene::streamState.voice_mask);
  state.stream_control_flags =
      Same<std::int32_t>(VS::Audio::Cutscene::streamState.control_flags);
  state.music_queued = VS::Audio::Seq::pendingMusicData.param1 != 0;
  return state;
}

void StoreCommandState(const CommandState& state) {
  for (std::size_t i = 0; i < state.effects.size(); ++i) {
    StoreEffect(VS::Audio::SoundEffects::channels[i], state.effects[i]);
  }
  for (std::size_t i = 0; i < state.music_dirty_flags.size(); ++i) {
    VS::Audio::Seq::musicChannels[i].voice.dirty_flags =
        state.music_dirty_flags[i];
  }
  if (VS::Audio::MusicConfig* active = VS::Audio::Seq::activeMusicConfig;
      active != nullptr && state.active) {
    Seq::StoreMusicLayer(*active, *state.active);
  }
  Seq::StoreVoiceScheduler(VS::Audio::Mixer::voiceScheduler, state.scheduler);
  VS::Audio::Driver::controlLatches.mix_behavior = state.mix_behavior;
  VS::Audio::Driver::controlLatches.update_flags = state.driver_update_flags;
  VS::Audio::Mixer::mutedMusicChannels = state.muted_music_channels;
  VS::Audio::Cutscene::streamState.voice_mask =
      static_cast<int>(state.stream_voices);
  VS::Audio::Cutscene::streamState.control_flags = state.stream_control_flags;
  if (!state.music_queued) VS::Audio::Seq::pendingMusicData.param1 = 0;
}

class LiveControlHost final : public ControlHost {
 public:
  explicit LiveControlHost(const char* operation) : operation_(operation) {}

  void ReleaseMusic(CommandState& state, Seq::ChannelGroup layer,
                    std::uint32_t music_id_filter) override {
    StoreCommandState(state);
    if (layer == Seq::ChannelGroup::kMusic) {
      Seq::LiveReleaseMusicConfiguration(
          VS::Audio::Seq::activeMusicConfig,
          std::data(VS::Audio::Seq::musicChannels), music_id_filter);
    } else {
      Seq::LiveReleaseMusicConfiguration(VS::Audio::Seq::secondaryMusicConfig,
                                         VS::Audio::Seq::secondaryMusicChannels,
                                         music_id_filter);
    }
    state = LoadCommandState();
  }

  void InvalidateMusicVolumes(CommandState& state,
                              Seq::ChannelGroup layer) override {
    VS::Audio::MusicConfig* config = VS::Audio::Seq::activeMusicConfig;
    VS::Audio::SeqChannel* channels = std::data(VS::Audio::Seq::musicChannels);
    if (layer != Seq::ChannelGroup::kMusic) {
      config = VS::Audio::Seq::secondaryMusicConfig;
      channels = VS::Audio::Seq::secondaryMusicChannels;
    }
    if (config == nullptr || channels == nullptr) {
      Refuse(CommandRefusal::kActiveLayerMissing);
      return;
    }
    StoreCommandState(state);
    SoundEffects::LiveInvalidateMusicChannelVolumes(config, channels);
    state = LoadCommandState();
  }

  void InvalidateSoundEffectVolumes(CommandState& state) override {
    StoreCommandState(state);
    SoundEffects::LiveInvalidateSoundEffectVolumes();
    state = LoadCommandState();
  }

  void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                      std::uint32_t right, std::uint32_t scale) override {
    Driver::SetLiveVoiceVolume(static_cast<int>(voice), left, right, scale);
  }

  void SetVoiceSampleRate(std::uint32_t voice, std::int32_t rate) override {
    Driver::SetLiveVoiceSampleRate(static_cast<int>(voice), rate);
  }

  void SetVoiceAttack(std::uint32_t voice, std::int32_t rate,
                      std::uint32_t mode) override {
    Driver::LiveSetVoiceAttackRateAndMode(static_cast<int>(voice), rate, mode);
  }

  void SetVoiceSustain(std::uint32_t voice, std::int32_t rate,
                       std::uint32_t mode) override {
    Driver::LiveSetVoiceSustainRateAndMode(static_cast<int>(voice), rate, mode);
  }

  void Refuse(CommandRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};

class LiveRefusals final : public CommandRefusalSink {
 public:
  explicit LiveRefusals(const char* operation) : operation_(operation) {}

  void Refuse(CommandRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};

// Runs a command core over the live command state.
template <typename Core>
void RunCommand(const char* operation, Core core) {
  LiveControlHost host(operation);
  CommandState state = LoadCommandState();
  core(state, host);
  StoreCommandState(state);
}

bool HasParams(const char* operation, const VS::Audio::CommandParams* params) {
  if (params != nullptr) return true;
  Seq::ReportRefusal(operation, "no command parameters");
  return false;
}

Seq::LevelSlide LoadTempoScale() {
  return {Same<std::int32_t>(VS::Audio::Mixer::tempoScale),
          Same<std::int32_t>(VS::Audio::Mixer::tempoScaleStep),
          Same<std::int16_t>(VS::Audio::Mixer::tempoScaleSlideLength)};
}

void StoreTempoScale(const Seq::LevelSlide& slide) {
  VS::Audio::Mixer::tempoScale = slide.value;
  VS::Audio::Mixer::tempoScaleStep = slide.step;
  VS::Audio::Mixer::tempoScaleSlideLength = slide.length;
}

Seq::LevelSlide LoadMusicPitchScale() {
  return {Same<std::int32_t>(VS::Audio::Mixer::musicPitchScale),
          Same<std::int32_t>(VS::Audio::Mixer::musicPitchScaleStep),
          Same<std::int16_t>(VS::Audio::Mixer::musicPitchScaleSlideLength)};
}

void StoreMusicPitchScale(const Seq::LevelSlide& slide) {
  VS::Audio::Mixer::musicPitchScale = slide.value;
  VS::Audio::Mixer::musicPitchScaleStep = slide.step;
  VS::Audio::Mixer::musicPitchScaleSlideLength = slide.length;
}

// The low byte of the first command word (lb 0x0 of the parameters).
std::int8_t ScaleByte(const VS::Audio::CommandParams& params) {
  return static_cast<std::int8_t>(static_cast<std::uint8_t>(params.param1));
}

using SlideCore = void (*)(Seq::LevelSlide&, const Seq::MusicCommand&,
                           CommandRefusalSink&);

void SlideTempo(const char* operation, VS::Audio::CommandParams* params,
                SlideCore core) {
  if (!HasParams(operation, params)) return;
  LiveRefusals sink(operation);
  Seq::LevelSlide slide = LoadTempoScale();
  core(slide, CommandOf(*params), sink);
  StoreTempoScale(slide);
}

void SlideMusicPitch(const char* operation, VS::Audio::CommandParams* params,
                     SlideCore core) {
  if (!HasParams(operation, params)) return;
  LiveRefusals sink(operation);
  Seq::LevelSlide slide = LoadMusicPitchScale();
  core(slide, CommandOf(*params), sink);
  StoreMusicPitchScale(slide);
}

std::int32_t LiveReverbMode() { return VS::Audio::Driver::OutputReverbMode(); }
void EnableLiveReverb(bool enabled) {
  VS::Audio::Driver::EnableOutputReverb(enabled ? kSpuOn : kSpuOff);
}
void SetLiveReverbMode(std::int32_t mode) {
  VS::Audio::Driver::SetOutputReverbMode(mode);
}

class LiveReverb final : public ReverbHost {
 public:
  std::int32_t ReverbMode() override { return LiveReverbMode(); }
  void EnableReverb(bool enabled) override { EnableLiveReverb(enabled); }
  void SetReverbMode(std::int32_t mode) override { SetLiveReverbMode(mode); }
};

CommandWords LoadArguments() {
  const VS::Audio::LegacyCommandArguments& arguments =
      VS::Audio::Driver::commandArguments;
  return {Same<std::uint32_t>(arguments.param2),
          Same<std::uint32_t>(arguments.param3),
          Same<std::uint32_t>(arguments.param4),
          Same<std::uint32_t>(arguments.ext_param1),
          Same<std::uint32_t>(arguments.ext_param2)};
}

void StoreArguments(const CommandWords& words) {
  VS::Audio::LegacyCommandArguments& arguments =
      VS::Audio::Driver::commandArguments;
  arguments.param2 = static_cast<int>(words.param2);
  arguments.param3 = static_cast<int>(words.param3);
  arguments.param4 = static_cast<int>(words.param4);
  arguments.ext_param1 = static_cast<int>(words.ext_param1);
  arguments.ext_param2 = static_cast<int>(words.ext_param2);
}

CommandWords LoadParameters() {
  const VS::Audio::CommandParams& parameters = VS::Audio::Mixer::commandParams;
  return {Same<std::uint32_t>(parameters.param2),
          Same<std::uint32_t>(parameters.param3),
          Same<std::uint32_t>(parameters.param4),
          Same<std::uint32_t>(parameters.ext_param1),
          Same<std::uint32_t>(parameters.ext_param2)};
}

void StoreParameters(const CommandWords& words) {
  VS::Audio::CommandParams& parameters = VS::Audio::Mixer::commandParams;
  parameters.param2 = words.param2;
  parameters.param3 = words.param3;
  parameters.param4 = words.param4;
  parameters.ext_param1 = words.ext_param1;
  parameters.ext_param2 = words.ext_param2;
}

DispatchState LoadDispatch() {
  DispatchState state;
  state.arguments = LoadArguments();
  state.parameters = LoadParameters();
  state.control_latches =
      Same<std::uint32_t>(VS::Audio::Driver::controlLatches.control_latches);
  if (const VS::Audio::MusicConfig* active =
          VS::Audio::Seq::activeMusicConfig) {
    state.active = Seq::LoadMusicLayer(*active);
  }
  if (const VS::Audio::MusicConfig* secondary = SecondaryRecord()) {
    state.secondary = Seq::LoadMusicLayer(*secondary);
  }
  return state;
}

// The secondary layer is stored before the active one, which wins when
// both name one record.
void StoreDispatch(const DispatchState& state) {
  StoreArguments(state.arguments);
  StoreParameters(state.parameters);
  VS::Audio::Driver::controlLatches.control_latches = state.control_latches;
  if (VS::Audio::MusicConfig* secondary = SecondaryRecord();
      secondary != nullptr && state.secondary) {
    Seq::StoreMusicLayer(*secondary, *state.secondary);
  }
  if (VS::Audio::MusicConfig* active = VS::Audio::Seq::activeMusicConfig;
      active != nullptr && state.active) {
    Seq::StoreMusicLayer(*active, *state.active);
  }
}

class LiveDispatchHost final : public DispatchHost {
 public:
  LiveDispatchHost()
      : LiveDispatchHost(
            reinterpret_cast<std::uint8_t*>(static_cast<std::intptr_t>(
                VS::Audio::Driver::commandArguments.param2))) {}
  explicit LiveDispatchHost(std::uint8_t* second_sequence)
      : first_(static_cast<std::uint8_t*>(
            VS::Audio::Driver::commandArguments.param1)),
        second_(second_sequence) {}

  std::int32_t ReverbMode() override { return LiveReverbMode(); }
  void EnableReverb(bool enabled) override { EnableLiveReverb(enabled); }
  void SetReverbMode(std::int32_t mode) override { SetLiveReverbMode(mode); }

  void BeginCommand() override {
    tick_timer::DisableEvent(VS::Audio::Driver::legacySpuEvent);
  }
  void EndCommand() override {
    tick_timer::EnableEvent(VS::Audio::Driver::legacySpuEvent);
  }

  std::optional<AkaoHeader> SequenceAt(ArgumentSlot slot) override {
    std::uint8_t* const start = Sequence(slot);
    if (start == nullptr) return std::nullopt;
    const std::optional<Seq::LiveProgramMemory> region =
        Seq::LiveProgramRegion(start);
    if (!region || region->From(start).size < kAkaoHeaderBytes) {
      Refuse(CommandRefusal::kSequenceUnreadable);
      return std::nullopt;
    }
    return ReadAkaoHeader(std::span(
        reinterpret_cast<const std::uint8_t*>(start), kAkaoHeaderBytes));
  }

  void CopyFirstArgument() override {
    VS::Audio::Mixer::commandParams.param1 = reinterpret_cast<std::uintptr_t>(
        VS::Audio::Driver::commandArguments.param1);
  }

  void QueueFirstSequence() override {
    VS::Audio::Seq::pendingMusicData.param1 = reinterpret_cast<std::uintptr_t>(
        VS::Audio::Driver::commandArguments.param1);
  }

  void ReplaceFirstArgument(std::uint32_t music_id) override {
    VS::Audio::Driver::commandArguments.param1 =
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(music_id));
  }

  void LoadSequence(DispatchState& state, ArgumentSlot slot) override {
    StoreDispatch(state);
    Seq::LiveLoadMusicSequence(
        reinterpret_cast<VS::Audio::AkaoSequenceFile*>(Sequence(slot)));
    state = LoadDispatch();
  }

  // func_80018C30 copies the PS1 layer record (0x80 bytes) and its 32
  // channels (0x2200 bytes); the native records are copied whole.
  void SaveActiveLayer(DispatchState& state) override {
    StoreDispatch(state);
    std::copy_n(std::data(VS::Audio::Seq::musicChannels),
                Seq::kMusicChannelCount,
                std::data(VS::Audio::Seq::savedMusicChannels));
    std::memcpy(&VS::Audio::Seq::savedMusicConfig,
                &VS::Audio::Seq::primaryMusicConfig,
                offsetof(VS::Audio::MusicConfig, reserved80));
    state = LoadDispatch();
  }

  void AttachSavedLayer(DispatchState& state) override {
    StoreDispatch(state);
    VS::Audio::Seq::secondaryMusicConfig = &VS::Audio::Seq::savedMusicConfig;
    VS::Audio::Seq::secondaryMusicChannels =
        std::data(VS::Audio::Seq::savedMusicChannels);
    state = LoadDispatch();
  }

  void RunHandler(DispatchState& state, std::uint32_t command) override {
    const VS::Audio::CommandHandler handler =
        VS::Audio::Mixer::kCommandHandlers[command];
    if (handler == nullptr) {
      Refuse(CommandRefusal::kNoCommandHandler);
      return;
    }
    StoreDispatch(state);
    handler(&VS::Audio::Mixer::commandParams);
    state = LoadDispatch();
  }

  void Refuse(CommandRefusal refusal) override {
    Seq::ReportRefusal("DispatchCommand", RefusalReason(refusal));
  }

 private:
  [[nodiscard]] std::uint8_t* Sequence(ArgumentSlot slot) const {
    return slot == ArgumentSlot::kFirst ? first_ : second_;
  }

  std::uint8_t* first_;
  std::uint8_t* second_;
};
}  // namespace

// decomp: SLUS_010.40 Sound_Cmd_A4_SetSfxPitchMod (src/SLUS_010.40/25AC.c:3614;
// was Sound_Cmd_A4_unk)
void LiveSetSoundEffectPitchScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SetSoundEffectPitchScale", params)) return;
  RunCommand("SetSoundEffectPitchScale",
             [params](CommandState& state, ControlHost&) {
               SetSoundEffectPitchScale(state, SelectionOf(*params),
                                        Same<std::uint32_t>(params->param3));
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A5_FadeSfxPitchMod
// (src/SLUS_010.40/25AC.c:3640; was Sound_Cmd_A5_unk)
void LiveSlideSoundEffectPitchScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SlideSoundEffectPitchScale", params)) return;
  RunCommand("SlideSoundEffectPitchScale", [params](CommandState& state,
                                                    ControlHost& host) {
    SlideSoundEffectPitchScale(state, SelectionOf(*params),
                               Same<std::uint32_t>(params->param3),
                               Same<std::uint32_t>(params->param4), host);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_AC_SetAllSfxPitchMod
// (src/SLUS_010.40/25AC.c:3668; was Sound_Cmd_AC_unk)
void LiveSetWorldSoundEffectPitchScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SetWorldSoundEffectPitchScale", params)) return;
  RunCommand("SetWorldSoundEffectPitchScale",
             [params](CommandState& state, ControlHost&) {
               SetWorldSoundEffectPitchScale(state, CommandOf(*params));
             });
}

// decomp: SLUS_010.40 Sound_Cmd_AD_FadeAllSfxPitchMod
// (src/SLUS_010.40/25AC.c:3681; was Sound_Cmd_AD_unk)
void LiveSlideWorldSoundEffectPitchScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SlideWorldSoundEffectPitchScale", params)) return;
  RunCommand("SlideWorldSoundEffectPitchScale",
             [params](CommandState& state, ControlHost& host) {
               SlideWorldSoundEffectPitchScale(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_D0_SetTempoScale (src/SLUS_010.40/25AC.c:3702;
// was func_8001833C)
void LiveSetTempoScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SetTempoScale", params)) return;
  Seq::LevelSlide slide = LoadTempoScale();
  SetTempoScale(slide, ScaleByte(*params));
  StoreTempoScale(slide);
}

// decomp: SLUS_010.40 Sound_Cmd_D1_FadeTempoScale (src/SLUS_010.40/25AC.c:3708;
// was func_80018358)
void LiveSlideTempoScale(VS::Audio::CommandParams* params) {
  SlideTempo("SlideTempoScale", params, &SlideTempoScale);
}

// decomp: SLUS_010.40 Sound_Cmd_D2_FadeTempoScaleFrom
// (src/SLUS_010.40/25AC.c:3720; was func_800183C0)
void LiveSetTempoScaleRamp(VS::Audio::CommandParams* params) {
  SlideTempo("SetTempoScaleRamp", params, &SetTempoScaleRamp);
}

// decomp: SLUS_010.40 Sound_Cmd_D4_SetMasterPitchScale
// (src/SLUS_010.40/25AC.c:3734; was func_80018434)
void LiveSetMusicPitchScale(VS::Audio::CommandParams* params) {
  if (!HasParams("SetMusicPitchScale", params)) return;
  Seq::LevelSlide slide = LoadMusicPitchScale();
  SetMusicPitchScale(slide, ScaleByte(*params));
  StoreMusicPitchScale(slide);
}

// decomp: SLUS_010.40 Sound_Cmd_D5_FadeMasterPitchScale
// (src/SLUS_010.40/25AC.c:3740; was func_80018450)
void LiveSlideMusicPitchScale(VS::Audio::CommandParams* params) {
  SlideMusicPitch("SlideMusicPitchScale", params, &SlideMusicPitchScale);
}

// decomp: SLUS_010.40 Sound_Cmd_D6_FadeMasterPitchScaleFrom
// (src/SLUS_010.40/25AC.c:3756; was func_800184B8)
void LiveSetMusicPitchScaleRamp(VS::Audio::CommandParams* params) {
  SlideMusicPitch("SetMusicPitchScaleRamp", params, &SetMusicPitchScaleRamp);
}

// decomp: SLUS_010.40 Sound_Cmd_F0_StopMusic (src/SLUS_010.40/25AC.c:3770; was
// func_8001852C)
void LiveStopMusicAndCancelReplacement(VS::Audio::CommandParams* /*params*/) {
  RunCommand("StopMusicAndCancelReplacement", &StopMusicAndCancelReplacement);
}

// decomp: SLUS_010.40 Sound_Cmd_11_StopMusicById (src/SLUS_010.40/25AC.c:3782;
// was Sound_Cmd_11_StopAllMusic)
void LiveStopMatchingMusic(VS::Audio::CommandParams* params) {
  if (!HasParams("StopMatchingMusic", params)) return;
  RunCommand("StopMatchingMusic",
             [params](CommandState& state, ControlHost& host) {
               StopMatchingMusic(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_F1_StopSfx (src/SLUS_010.40/25AC.c:3794; was
// func_80018608)
void LiveStopWorldSoundEffects(VS::Audio::CommandParams* /*params*/) {
  RunCommand("StopWorldSoundEffects", [](CommandState& state, ControlHost&) {
    StopWorldSoundEffects(state);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_80_SetModeStereo (src/SLUS_010.40/25AC.c:3813;
// was Sound_Cmd_80_unk)
void LiveEnableStereoOutput(VS::Audio::CommandParams* /*params*/) {
  RunCommand("EnableStereoOutput", &EnableStereoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_81_SetModeMono (src/SLUS_010.40/25AC.c:3824;
// was Sound_Cmd_81_unk)
void LiveEnableMonoOutput(VS::Audio::CommandParams* /*params*/) {
  RunCommand("EnableMonoOutput", &EnableMonoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_90_SetMutedMusicChannelMask
// (src/SLUS_010.40/25AC.c:3835; was Sound_Cmd_90_unk)
void LiveSetMusicMuteMask(VS::Audio::CommandParams* params) {
  if (!HasParams("SetMusicMuteMask", params)) return;
  RunCommand("SetMusicMuteMask", [params](CommandState& state, ControlHost&) {
    SetMusicMuteMask(state, CommandOf(*params));
  });
}

// decomp: SLUS_010.40 Sound_Cmd_92_SetMusicJumpThreshold
// (src/SLUS_010.40/25AC.c:3847; was Sound_Cmd_92_unk)
void LiveSetMusicJumpThreshold(VS::Audio::CommandParams* params) {
  if (!HasParams("SetMusicJumpThreshold", params)) return;
  RunCommand("SetMusicJumpThreshold",
             [params](CommandState& state, ControlHost& host) {
               SetMusicJumpThreshold(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_9B_SuspendMusic (src/SLUS_010.40/25AC.c:3852;
// was Sound_Cmd_9B_unk)
void LivePauseMusic(VS::Audio::CommandParams* /*params*/) {
  RunCommand("PauseMusic", &PauseMusic);
}

// decomp: SLUS_010.40 Sound_Cmd_9A_RestoreMusic (src/SLUS_010.40/25AC.c:3885;
// was Sound_Cmd_9A_unk)
void LiveResumeMusic(VS::Audio::CommandParams* /*params*/) {
  RunCommand("ResumeMusic", [](CommandState& state, ControlHost& host) {
    ResumeMusic(state, host);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_9D_SuspendSfx (src/SLUS_010.40/25AC.c:3909; was
// Sound_Cmd_9D_unk)
void LivePauseWorldSoundEffects(VS::Audio::CommandParams* /*params*/) {
  RunCommand("PauseWorldSoundEffects", &PauseWorldSoundEffects);
}

// decomp: SLUS_010.40 Sound_Cmd_9C_RestoreSfx (src/SLUS_010.40/25AC.c:3948; was
// Sound_Cmd_9C_unk)
void LiveResumeWorldSoundEffects(VS::Audio::CommandParams* /*params*/) {
  RunCommand("ResumeWorldSoundEffects",
             [](CommandState& state, ControlHost& host) {
               ResumeWorldSoundEffects(state, host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_11_Nop (src/SLUS_010.40/25AC.c:3991)
void LiveIgnoreCommand(VS::Audio::CommandParams* /*params*/) {
  IgnoreCommand();
}

// decomp: SLUS_010.40 Sound_SetReverbMode (src/SLUS_010.40/25AC.c:3993)
void LiveSetReverbMode(int mode) {
  LiveReverb host;
  SetReverbMode(host, mode);
}

// decomp: SLUS_010.40 Sound_ExecuteSoundVm2Function
// (src/SLUS_010.40/25AC.c:4005; was func_80018C30)
unsigned LiveDispatchCommand(unsigned command) {
  LiveDispatchHost host;
  DispatchState state = LoadDispatch();
  const std::uint32_t result = DispatchCommand(state, host, command);
  StoreDispatch(state);
  return result;
}

// LiveDispatchCommand for SoundPlaybackLive's kStartMusicPair.
unsigned LiveDispatchCommand(unsigned command, std::uint8_t* second_sequence) {
  LiveDispatchHost host(second_sequence);
  DispatchState state = LoadDispatch();
  const std::uint32_t result = DispatchCommand(state, host, command);
  StoreDispatch(state);
  return result;
}
}  // namespace VS::Audio::Mixer
