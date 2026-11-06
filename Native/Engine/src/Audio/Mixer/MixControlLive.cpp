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

#include "Audio/MixControl.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/VoiceOutputLive.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio::Mixer {
namespace {
static_assert(std::size(Audio_Seq_musicChannels) == Seq::kMusicChannelCount);
static_assert(std::size(Audio_Seq_savedMusicChannels) ==
              Seq::kMusicChannelCount);
static_assert(std::size(Audio_SoundEffects_channels) >= kEffectChannelCount);
// func_80018C30 dispatches on the command's low byte.
constexpr std::size_t kCommandCount = 0x100;
static_assert(std::size(Audio_Mixer_kCommandHandlers) == kCommandCount);
static_assert(sizeof(Audio_Driver_commandArguments.param2) ==
              sizeof(std::uint32_t));
static_assert(sizeof(Audio_Driver_legacySpuEvent) == sizeof(std::int32_t));
static_assert(
    std::is_convertible_v<decltype(&LiveSetTempoScale), Audio_CommandHandler>);

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

Seq::MusicCommand CommandOf(const Audio_CommandParams& params) {
  return {static_cast<std::uint32_t>(params.param1),
          Same<std::uint32_t>(params.param2),
          Same<std::uint32_t>(params.param3),
          Same<std::uint32_t>(params.param4),
          Same<std::uint32_t>(params.extParam1),
          Same<std::uint32_t>(params.extParam2)};
}

// param1 is the selected effect's whole native identity.
EffectSelection SelectionOf(const Audio_CommandParams& params) {
  static_assert(sizeof(params.param1) == sizeof(EffectId));
  return {static_cast<EffectId>(params.param1),
          Same<std::uint32_t>(params.param2)};
}

// The secondary layer's record when it is one of its own.
Audio_MusicConfig* SecondaryRecord() {
  Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
  return secondary != Audio_Seq_activeMusicConfig ? secondary : nullptr;
}

EffectControl LoadEffect(const Audio_SeqChannel& legacy) {
  EffectControl effect;
  effect.tags = Same<std::int32_t>(legacy.effectTags);
  effect.effect_id = static_cast<EffectId>(legacy.effectId);
  effect.update_flags = Same<std::uint32_t>(legacy.updateFlags);
  effect.pitch_scale = Same<std::int32_t>(legacy.effectPitchScale);
  effect.pitch_scale_step =
      Same<std::uint32_t>(legacy.effectPitchScaleSlideStep);
  effect.pitch_scale_length =
      Same<std::uint16_t>(legacy.effectPitchScaleSlideLength);
  effect.dirty_flags = Same<std::uint32_t>(legacy.voice.dirtyFlags);
  return effect;
}

void StoreEffect(Audio_SeqChannel& legacy, const EffectControl& effect) {
  legacy.effectTags = effect.tags;
  legacy.effectId = static_cast<std::uintptr_t>(effect.effect_id);
  legacy.updateFlags = effect.update_flags;
  legacy.effectPitchScale = effect.pitch_scale;
  legacy.effectPitchScaleSlideStep = effect.pitch_scale_step;
  legacy.effectPitchScaleSlideLength = effect.pitch_scale_length;
  legacy.voice.dirtyFlags = effect.dirty_flags;
}

CommandState LoadCommandState() {
  CommandState state;
  for (std::size_t i = 0; i < state.effects.size(); ++i) {
    state.effects[i] = LoadEffect(Audio_SoundEffects_channels[i]);
  }
  for (std::size_t i = 0; i < state.music_dirty_flags.size(); ++i) {
    state.music_dirty_flags[i] =
        Same<std::uint32_t>(Audio_Seq_musicChannels[i].voice.dirtyFlags);
  }
  if (const Audio_MusicConfig* active = Audio_Seq_activeMusicConfig) {
    state.active = Seq::LoadMusicLayer(*active);
  }
  state.has_secondary = Audio_Seq_secondaryMusicConfig != nullptr;
  state.scheduler = Seq::LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  state.mix_behavior =
      Same<std::uint32_t>(Audio_Driver_controlLatches.mixBehavior);
  state.driver_update_flags =
      Same<std::uint32_t>(Audio_Driver_controlLatches.updateFlags);
  state.muted_music_channels =
      Same<std::int32_t>(Audio_Mixer_mutedMusicChannels);
  state.stream_voices =
      Same<std::uint32_t>(Audio_Cutscene_streamState.voiceMask);
  state.stream_control_flags =
      Same<std::int32_t>(Audio_Cutscene_streamState.controlFlags);
  state.music_queued = Audio_Seq_pendingMusicData.param1 != 0;
  return state;
}

void StoreCommandState(const CommandState& state) {
  for (std::size_t i = 0; i < state.effects.size(); ++i) {
    StoreEffect(Audio_SoundEffects_channels[i], state.effects[i]);
  }
  for (std::size_t i = 0; i < state.music_dirty_flags.size(); ++i) {
    Audio_Seq_musicChannels[i].voice.dirtyFlags = state.music_dirty_flags[i];
  }
  if (Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
      active != nullptr && state.active) {
    Seq::StoreMusicLayer(*active, *state.active);
  }
  Seq::StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Driver_controlLatches.mixBehavior = state.mix_behavior;
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  Audio_Mixer_mutedMusicChannels = state.muted_music_channels;
  Audio_Cutscene_streamState.voiceMask = static_cast<int>(state.stream_voices);
  Audio_Cutscene_streamState.controlFlags = state.stream_control_flags;
  if (!state.music_queued) Audio_Seq_pendingMusicData.param1 = 0;
}

class LiveControlHost final : public ControlHost {
 public:
  explicit LiveControlHost(const char* operation) : operation_(operation) {}

  void ReleaseMusic(CommandState& state, Seq::ChannelGroup layer,
                    std::uint32_t music_id_filter) override {
    StoreCommandState(state);
    if (layer == Seq::ChannelGroup::kMusic) {
      Seq::LiveReleaseMusicConfiguration(Audio_Seq_activeMusicConfig,
                                         std::data(Audio_Seq_musicChannels),
                                         music_id_filter);
    } else {
      Seq::LiveReleaseMusicConfiguration(Audio_Seq_secondaryMusicConfig,
                                         Audio_Seq_secondaryMusicChannels,
                                         music_id_filter);
    }
    state = LoadCommandState();
  }

  void InvalidateMusicVolumes(CommandState& state,
                              Seq::ChannelGroup layer) override {
    Audio_MusicConfig* config = Audio_Seq_activeMusicConfig;
    Audio_SeqChannel* channels = std::data(Audio_Seq_musicChannels);
    if (layer != Seq::ChannelGroup::kMusic) {
      config = Audio_Seq_secondaryMusicConfig;
      channels = Audio_Seq_secondaryMusicChannels;
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

bool HasParams(const char* operation, const Audio_CommandParams* params) {
  if (params != nullptr) return true;
  Seq::ReportRefusal(operation, "no command parameters");
  return false;
}

Seq::LevelSlide LoadTempoScale() {
  return {Same<std::int32_t>(Audio_Mixer_tempoScale),
          Same<std::int32_t>(Audio_Mixer_tempoScaleStep),
          Same<std::int16_t>(Audio_Mixer_tempoScaleSlideLength)};
}

void StoreTempoScale(const Seq::LevelSlide& slide) {
  Audio_Mixer_tempoScale = slide.value;
  Audio_Mixer_tempoScaleStep = slide.step;
  Audio_Mixer_tempoScaleSlideLength = slide.length;
}

Seq::LevelSlide LoadMusicPitchScale() {
  return {Same<std::int32_t>(Audio_Mixer_musicPitchScale),
          Same<std::int32_t>(Audio_Mixer_musicPitchScaleStep),
          Same<std::int16_t>(Audio_Mixer_musicPitchScaleSlideLength)};
}

void StoreMusicPitchScale(const Seq::LevelSlide& slide) {
  Audio_Mixer_musicPitchScale = slide.value;
  Audio_Mixer_musicPitchScaleStep = slide.step;
  Audio_Mixer_musicPitchScaleSlideLength = slide.length;
}

// The low byte of the first command word (lb 0x0 of the parameters).
std::int8_t ScaleByte(const Audio_CommandParams& params) {
  return static_cast<std::int8_t>(static_cast<std::uint8_t>(params.param1));
}

using SlideCore = void (*)(Seq::LevelSlide&, const Seq::MusicCommand&,
                           CommandRefusalSink&);

void SlideTempo(const char* operation, Audio_CommandParams* params,
                SlideCore core) {
  if (!HasParams(operation, params)) return;
  LiveRefusals sink(operation);
  Seq::LevelSlide slide = LoadTempoScale();
  core(slide, CommandOf(*params), sink);
  StoreTempoScale(slide);
}

void SlideMusicPitch(const char* operation, Audio_CommandParams* params,
                     SlideCore core) {
  if (!HasParams(operation, params)) return;
  LiveRefusals sink(operation);
  Seq::LevelSlide slide = LoadMusicPitchScale();
  core(slide, CommandOf(*params), sink);
  StoreMusicPitchScale(slide);
}

std::int32_t LiveReverbMode() { return Audio_Native_ReverbMode(); }
void EnableLiveReverb(bool enabled) {
  Audio_Native_EnableReverb(enabled ? SPU_ON : SPU_OFF);
}
void SetLiveReverbMode(std::int32_t mode) { Audio_Native_SetReverbMode(mode); }

class LiveReverb final : public ReverbHost {
 public:
  std::int32_t ReverbMode() override { return LiveReverbMode(); }
  void EnableReverb(bool enabled) override { EnableLiveReverb(enabled); }
  void SetReverbMode(std::int32_t mode) override { SetLiveReverbMode(mode); }
};

CommandWords LoadArguments() {
  const Audio_CommandArguments& arguments = Audio_Driver_commandArguments;
  return {Same<std::uint32_t>(arguments.param2),
          Same<std::uint32_t>(arguments.param3),
          Same<std::uint32_t>(arguments.param4),
          Same<std::uint32_t>(arguments.extParam1),
          Same<std::uint32_t>(arguments.extParam2)};
}

void StoreArguments(const CommandWords& words) {
  Audio_CommandArguments& arguments = Audio_Driver_commandArguments;
  arguments.param2 = static_cast<int>(words.param2);
  arguments.param3 = static_cast<int>(words.param3);
  arguments.param4 = static_cast<int>(words.param4);
  arguments.extParam1 = static_cast<int>(words.ext_param1);
  arguments.extParam2 = static_cast<int>(words.ext_param2);
}

CommandWords LoadParameters() {
  const Audio_CommandParams& parameters = Audio_Mixer_commandParams;
  return {Same<std::uint32_t>(parameters.param2),
          Same<std::uint32_t>(parameters.param3),
          Same<std::uint32_t>(parameters.param4),
          Same<std::uint32_t>(parameters.extParam1),
          Same<std::uint32_t>(parameters.extParam2)};
}

void StoreParameters(const CommandWords& words) {
  Audio_CommandParams& parameters = Audio_Mixer_commandParams;
  parameters.param2 = words.param2;
  parameters.param3 = words.param3;
  parameters.param4 = words.param4;
  parameters.extParam1 = words.ext_param1;
  parameters.extParam2 = words.ext_param2;
}

DispatchState LoadDispatch() {
  DispatchState state;
  state.arguments = LoadArguments();
  state.parameters = LoadParameters();
  state.control_latches =
      Same<std::uint32_t>(Audio_Driver_controlLatches.controlLatches);
  if (const Audio_MusicConfig* active = Audio_Seq_activeMusicConfig) {
    state.active = Seq::LoadMusicLayer(*active);
  }
  if (const Audio_MusicConfig* secondary = SecondaryRecord()) {
    state.secondary = Seq::LoadMusicLayer(*secondary);
  }
  return state;
}

// The secondary layer is stored before the active one, which wins when
// both name one record.
void StoreDispatch(const DispatchState& state) {
  StoreArguments(state.arguments);
  StoreParameters(state.parameters);
  Audio_Driver_controlLatches.controlLatches = state.control_latches;
  if (Audio_MusicConfig* secondary = SecondaryRecord();
      secondary != nullptr && state.secondary) {
    Seq::StoreMusicLayer(*secondary, *state.secondary);
  }
  if (Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
      active != nullptr && state.active) {
    Seq::StoreMusicLayer(*active, *state.active);
  }
}

class LiveDispatchHost final : public DispatchHost {
 public:
  LiveDispatchHost()
      : LiveDispatchHost(reinterpret_cast<char*>(
            static_cast<std::intptr_t>(Audio_Driver_commandArguments.param2))) {
  }
  explicit LiveDispatchHost(char* second_sequence)
      : first_(static_cast<char*>(Audio_Driver_commandArguments.param1)),
        second_(second_sequence) {}

  std::int32_t ReverbMode() override { return LiveReverbMode(); }
  void EnableReverb(bool enabled) override { EnableLiveReverb(enabled); }
  void SetReverbMode(std::int32_t mode) override { SetLiveReverbMode(mode); }

  void BeginCommand() override { DisableEvent(Audio_Driver_legacySpuEvent); }
  void EndCommand() override { EnableEvent(Audio_Driver_legacySpuEvent); }

  std::optional<AkaoHeader> SequenceAt(ArgumentSlot slot) override {
    char* const start = Sequence(slot);
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
    Audio_Mixer_commandParams.param1 =
        reinterpret_cast<std::uintptr_t>(Audio_Driver_commandArguments.param1);
  }

  void QueueFirstSequence() override {
    Audio_Seq_pendingMusicData.param1 =
        reinterpret_cast<std::uintptr_t>(Audio_Driver_commandArguments.param1);
  }

  void ReplaceFirstArgument(std::uint32_t music_id) override {
    Audio_Driver_commandArguments.param1 =
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(music_id));
  }

  void LoadSequence(DispatchState& state, ArgumentSlot slot) override {
    StoreDispatch(state);
    Seq::LiveLoadMusicSequence(
        reinterpret_cast<Audio_AkaoSequenceFile*>(Sequence(slot)));
    state = LoadDispatch();
  }

  // func_80018C30 copies the PS1 layer record (0x80 bytes) and its 32
  // channels (0x2200 bytes); the native records are copied whole.
  void SaveActiveLayer(DispatchState& state) override {
    StoreDispatch(state);
    std::copy_n(std::data(Audio_Seq_musicChannels), Seq::kMusicChannelCount,
                std::data(Audio_Seq_savedMusicChannels));
    std::memcpy(&Audio_Seq_savedMusicConfig, &Audio_Seq_primaryMusicConfig,
                offsetof(Audio_MusicConfig, reserved80));
    state = LoadDispatch();
  }

  void AttachSavedLayer(DispatchState& state) override {
    StoreDispatch(state);
    Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
    Audio_Seq_secondaryMusicChannels = std::data(Audio_Seq_savedMusicChannels);
    state = LoadDispatch();
  }

  void RunHandler(DispatchState& state, std::uint32_t command) override {
    const Audio_CommandHandler handler = Audio_Mixer_kCommandHandlers[command];
    if (handler == nullptr) {
      Refuse(CommandRefusal::kNoCommandHandler);
      return;
    }
    StoreDispatch(state);
    handler(&Audio_Mixer_commandParams);
    state = LoadDispatch();
  }

  void Refuse(CommandRefusal refusal) override {
    Seq::ReportRefusal("DispatchCommand", RefusalReason(refusal));
  }

 private:
  [[nodiscard]] char* Sequence(ArgumentSlot slot) const {
    return slot == ArgumentSlot::kFirst ? first_ : second_;
  }

  char* first_;
  char* second_;
};
}  // namespace

// decomp: SLUS_010.40 Sound_Cmd_A4_unk (src/SLUS_010.40/25AC.c:3427)
void LiveSetSoundEffectPitchScale(Audio_CommandParams* params) {
  if (!HasParams("SetSoundEffectPitchScale", params)) return;
  RunCommand("SetSoundEffectPitchScale",
             [params](CommandState& state, ControlHost&) {
               SetSoundEffectPitchScale(state, SelectionOf(*params),
                                        Same<std::uint32_t>(params->param3));
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A5_unk (src/SLUS_010.40/25AC.c:3453)
void LiveSlideSoundEffectPitchScale(Audio_CommandParams* params) {
  if (!HasParams("SlideSoundEffectPitchScale", params)) return;
  RunCommand("SlideSoundEffectPitchScale", [params](CommandState& state,
                                                    ControlHost& host) {
    SlideSoundEffectPitchScale(state, SelectionOf(*params),
                               Same<std::uint32_t>(params->param3),
                               Same<std::uint32_t>(params->param4), host);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_AC_unk (src/SLUS_010.40/25AC.c:3481)
void LiveSetWorldSoundEffectPitchScale(Audio_CommandParams* params) {
  if (!HasParams("SetWorldSoundEffectPitchScale", params)) return;
  RunCommand("SetWorldSoundEffectPitchScale",
             [params](CommandState& state, ControlHost&) {
               SetWorldSoundEffectPitchScale(state, CommandOf(*params));
             });
}

// decomp: SLUS_010.40 Sound_Cmd_AD_unk (src/SLUS_010.40/25AC.c:3494)
void LiveSlideWorldSoundEffectPitchScale(Audio_CommandParams* params) {
  if (!HasParams("SlideWorldSoundEffectPitchScale", params)) return;
  RunCommand("SlideWorldSoundEffectPitchScale",
             [params](CommandState& state, ControlHost& host) {
               SlideWorldSoundEffectPitchScale(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 func_8001833C (src/SLUS_010.40/25AC.c:3515)
void LiveSetTempoScale(Audio_CommandParams* params) {
  if (!HasParams("SetTempoScale", params)) return;
  Seq::LevelSlide slide = LoadTempoScale();
  SetTempoScale(slide, ScaleByte(*params));
  StoreTempoScale(slide);
}

// decomp: SLUS_010.40 func_80018358 (src/SLUS_010.40/25AC.c:3521)
void LiveSlideTempoScale(Audio_CommandParams* params) {
  SlideTempo("SlideTempoScale", params, &SlideTempoScale);
}

// decomp: SLUS_010.40 func_800183C0 (src/SLUS_010.40/25AC.c:3532)
void LiveSetTempoScaleRamp(Audio_CommandParams* params) {
  SlideTempo("SetTempoScaleRamp", params, &SetTempoScaleRamp);
}

// decomp: SLUS_010.40 func_80018434 (src/SLUS_010.40/25AC.c:3546)
void LiveSetMusicPitchScale(Audio_CommandParams* params) {
  if (!HasParams("SetMusicPitchScale", params)) return;
  Seq::LevelSlide slide = LoadMusicPitchScale();
  SetMusicPitchScale(slide, ScaleByte(*params));
  StoreMusicPitchScale(slide);
}

// decomp: SLUS_010.40 func_80018450 (src/SLUS_010.40/25AC.c:3552)
void LiveSlideMusicPitchScale(Audio_CommandParams* params) {
  SlideMusicPitch("SlideMusicPitchScale", params, &SlideMusicPitchScale);
}

// decomp: SLUS_010.40 func_800184B8 (src/SLUS_010.40/25AC.c:3568)
void LiveSetMusicPitchScaleRamp(Audio_CommandParams* params) {
  SlideMusicPitch("SetMusicPitchScaleRamp", params, &SetMusicPitchScaleRamp);
}

// decomp: SLUS_010.40 func_8001852C (src/SLUS_010.40/25AC.c:3582)
void LiveStopMusicAndCancelReplacement(Audio_CommandParams* /*params*/) {
  RunCommand("StopMusicAndCancelReplacement", &StopMusicAndCancelReplacement);
}

// decomp: SLUS_010.40 Sound_Cmd_11_StopAllMusic (src/SLUS_010.40/25AC.c:3594)
void LiveStopMatchingMusic(Audio_CommandParams* params) {
  if (!HasParams("StopMatchingMusic", params)) return;
  RunCommand("StopMatchingMusic",
             [params](CommandState& state, ControlHost& host) {
               StopMatchingMusic(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 func_80018608 (src/SLUS_010.40/25AC.c:3607)
void LiveStopWorldSoundEffects(Audio_CommandParams* /*params*/) {
  RunCommand("StopWorldSoundEffects", [](CommandState& state, ControlHost&) {
    StopWorldSoundEffects(state);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_80_unk (src/SLUS_010.40/25AC.c:3626)
void LiveEnableStereoOutput(Audio_CommandParams* /*params*/) {
  RunCommand("EnableStereoOutput", &EnableStereoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_81_unk (src/SLUS_010.40/25AC.c:3637)
void LiveEnableMonoOutput(Audio_CommandParams* /*params*/) {
  RunCommand("EnableMonoOutput", &EnableMonoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_90_unk (src/SLUS_010.40/25AC.c:3648)
void LiveSetMusicMuteMask(Audio_CommandParams* params) {
  if (!HasParams("SetMusicMuteMask", params)) return;
  RunCommand("SetMusicMuteMask", [params](CommandState& state, ControlHost&) {
    SetMusicMuteMask(state, CommandOf(*params));
  });
}

// decomp: SLUS_010.40 Sound_Cmd_92_unk (src/SLUS_010.40/25AC.c:3660)
void LiveSetMusicJumpThreshold(Audio_CommandParams* params) {
  if (!HasParams("SetMusicJumpThreshold", params)) return;
  RunCommand("SetMusicJumpThreshold",
             [params](CommandState& state, ControlHost& host) {
               SetMusicJumpThreshold(state, CommandOf(*params), host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_9B_unk (src/SLUS_010.40/25AC.c:3665)
void LivePauseMusic(Audio_CommandParams* /*params*/) {
  RunCommand("PauseMusic", &PauseMusic);
}

// decomp: SLUS_010.40 Sound_Cmd_9A_unk (src/SLUS_010.40/25AC.c:3698)
void LiveResumeMusic(Audio_CommandParams* /*params*/) {
  RunCommand("ResumeMusic", [](CommandState& state, ControlHost& host) {
    ResumeMusic(state, host);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_9D_unk (src/SLUS_010.40/25AC.c:3722)
void LivePauseWorldSoundEffects(Audio_CommandParams* /*params*/) {
  RunCommand("PauseWorldSoundEffects", &PauseWorldSoundEffects);
}

// decomp: SLUS_010.40 Sound_Cmd_9C_unk (src/SLUS_010.40/25AC.c:3761)
void LiveResumeWorldSoundEffects(Audio_CommandParams* /*params*/) {
  RunCommand("ResumeWorldSoundEffects",
             [](CommandState& state, ControlHost& host) {
               ResumeWorldSoundEffects(state, host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_11_Nop (src/SLUS_010.40/25AC.c:3804)
void LiveIgnoreCommand(Audio_CommandParams* /*params*/) { IgnoreCommand(); }

// decomp: SLUS_010.40 Sound_SetReverbMode (src/SLUS_010.40/25AC.c:3806)
void LiveSetReverbMode(int mode) {
  LiveReverb host;
  SetReverbMode(host, mode);
}

// decomp: SLUS_010.40 func_80018C30 (src/SLUS_010.40/25AC.c:3818)
unsigned LiveDispatchCommand(unsigned command) {
  LiveDispatchHost host;
  DispatchState state = LoadDispatch();
  const std::uint32_t result = DispatchCommand(state, host, command);
  StoreDispatch(state);
  return result;
}

// LiveDispatchCommand for SoundPlaybackLive's kStartMusicPair.
unsigned LiveDispatchCommand(unsigned command, char* second_sequence) {
  LiveDispatchHost host(second_sequence);
  DispatchState state = LoadDispatch();
  const std::uint32_t result = DispatchCommand(state, host, command);
  StoreDispatch(state);
  return result;
}
}  // namespace VS::Audio::Mixer
