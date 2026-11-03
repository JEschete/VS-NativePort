// Hand-maintained (tools/port/owned.csv).
#include "Audio/MixControl.hpp"  // NOLINT(build/include): native C++ interface.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceMixer.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Mixer {
namespace {
// The dispatcher's result for a command that starts no track.
constexpr std::uint32_t kNoTrack = 0xFFFFFFFFU;
constexpr std::uint32_t kVoicesBelowEffects = kFirstEffectVoiceBit - 1U;
constexpr std::uint32_t kByteMask = 0xFFU;
constexpr std::uint32_t kByteBits = 8U;
constexpr std::uint32_t kHalfBits = 16U;
// An effect pitch scale's integer part starts at bit 8.
constexpr std::uint32_t kEffectPitchShift = 8U;
// Tempo and music pitch scales are 16.16 fixed point.
constexpr std::int32_t kScaleOne = 0x10000;
// AKAO header fields (AkaoSeqHeader.id and .reverb_type).
constexpr std::size_t kAkaoSequenceIdAt = 4;
constexpr std::size_t kAkaoReverbModeAt = 8;

// func_80018C30's command bytes (Sound_Cmd_10_StartFieldMusic,
// Sound_Cmd_12_unk, Sound_Cmd_19_Unk and Sound_Cmd_1A_Unk start tracks).
constexpr std::uint32_t kStartMusic = 0x10;
constexpr std::uint32_t kStartMusicWithLoopCounter = 0x12;
constexpr std::uint32_t kQueueMusic = 0x14;
constexpr std::uint32_t kStartMusicPair = 0x16;
constexpr std::uint32_t kResumeMusicLayer = 0x17;
constexpr std::uint32_t kStartMusicLayer = 0x19;
constexpr std::uint32_t kCrossfadeMusicLayers = 0x1A;
constexpr std::uint32_t kResumeAll = 0x98;
constexpr std::uint32_t kPauseAll = 0x99;
constexpr std::uint32_t kSetScales = 0xD8;
constexpr std::uint32_t kSlideScales = 0xD9;
constexpr std::uint32_t kRampScales = 0xDA;
// The D_800301E0 handlers the composite commands run.
constexpr std::uint32_t kResumeMusicHandler = 0x9A;
constexpr std::uint32_t kPauseMusicHandler = 0x9B;
constexpr std::uint32_t kResumeEffectsHandler = 0x9C;
constexpr std::uint32_t kPauseEffectsHandler = 0x9D;
constexpr std::uint32_t kResumeStreamHandler = 0x9E;
constexpr std::uint32_t kPauseStreamHandler = 0x9F;
constexpr std::uint32_t kSetTempoHandler = 0xD0;
constexpr std::uint32_t kSlideTempoHandler = 0xD1;
constexpr std::uint32_t kRampTempoHandler = 0xD2;
constexpr std::uint32_t kSetPitchHandler = 0xD4;
constexpr std::uint32_t kSlidePitchHandler = 0xD5;
constexpr std::uint32_t kRampPitchHandler = 0xD6;

bool IsSelected(const EffectControl& effect, EffectSelection selection) {
  if (selection.tags != 0) {
    return (static_cast<std::uint32_t>(effect.tags) & selection.tags) != 0;
  }
  return effect.effect_id == selection.effect;
}

bool IsWorldEffect(const EffectControl& effect) {
  return (effect.tags & kFixedMixEffect) == 0;
}

// A command byte as an effect pitch scale: the byte is unsigned (lbu).
std::int32_t EffectPitchOf(std::uint32_t word) {
  return static_cast<std::int32_t>((word & kByteMask) << kEffectPitchShift);
}

// The slide toward `target_word`'s pitch over `length_word` ticks, at the
// original's 16-bit widths (Sound_Cmd_A5_unk, Sound_Cmd_AD_unk).
void SlideEffectPitch(EffectControl& effect, std::uint32_t length_word,
                      std::uint32_t target_word, CommandRefusalSink& sink) {
  const std::uint16_t ticks =
      length_word != 0 ? static_cast<std::uint16_t>(length_word) : 1;
  const auto distance = static_cast<std::int16_t>(
      static_cast<std::uint32_t>(EffectPitchOf(target_word)) -
      static_cast<std::uint16_t>(effect.pitch_scale));
  auto divisor = static_cast<std::int16_t>(ticks);
  if (divisor == 0) {
    sink.Refuse(CommandRefusal::kZeroLengthSlide);
    divisor = 1;
  }
  const auto step =
      static_cast<std::int16_t>(std::int32_t{distance} / std::int32_t{divisor});
  effect.pitch_scale_step =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(step));
  effect.pitch_scale_length = ticks;
}

// A command byte as a tempo or music pitch scale: the byte is signed (lb).
std::int32_t ScaleOf(std::uint32_t word) {
  return std::int32_t{static_cast<std::int8_t>(word & kByteMask)} * kScaleOne;
}

// (target - from) / ticks at 32 bits.
std::int32_t SlideStep(std::int32_t target, std::int32_t from,
                       std::int32_t ticks, CommandRefusalSink& sink) {
  const auto distance = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(target) - static_cast<std::uint32_t>(from));
  if (ticks == 0) {
    sink.Refuse(CommandRefusal::kZeroLengthSlide);
    ticks = 1;
  }
  if (ticks == -1 && distance == std::numeric_limits<std::int32_t>::min()) {
    sink.Refuse(CommandRefusal::kSlideStepOverflow);
    return distance;
  }
  return distance / ticks;
}

// func_80018358 / func_80018450: from the current scale to `command.param2`
// over `command.param1` ticks (one when zero).
void SlideScale(Seq::LevelSlide& scale, const Seq::MusicCommand& command,
                CommandRefusalSink& sink) {
  const std::int32_t ticks =
      command.param1 != 0 ? static_cast<std::int32_t>(command.param1) : 1;
  scale.step = SlideStep(ScaleOf(command.param2), scale.value, ticks, sink);
  scale.length = static_cast<std::int16_t>(ticks);
}

// func_800183C0 / func_800184B8: from `command.param2` to `command.param3`
// over `command.param1` ticks; one tick when `command.param2` is zero.
void RampScale(Seq::LevelSlide& scale, const Seq::MusicCommand& command,
               CommandRefusalSink& sink) {
  const std::int32_t ticks =
      command.param2 != 0 ? static_cast<std::int32_t>(command.param1) : 1;
  scale.value = ScaleOf(command.param2);
  scale.step = SlideStep(ScaleOf(command.param3), scale.value, ticks, sink);
  scale.length = static_cast<std::int16_t>(ticks);
}

// The release the music and sound-effect pauses give a voice.
void SilenceVoice(ControlHost& host, std::uint32_t voice) {
  host.SetVoiceVolume(voice, 0, 0, 0);
  host.SetVoiceSampleRate(voice, 0);
  host.SetVoiceAttack(voice, kPauseRate, kPauseAttackMode);
  host.SetVoiceSustain(voice, kPauseRate, kPauseSustainMode);
}

void SetOutputMode(CommandState& state, ControlHost& host, std::uint32_t mode) {
  state.mix_behavior = mode;
  if (state.active) {
    host.InvalidateMusicVolumes(state, Seq::ChannelGroup::kMusic);
  } else {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
  }
  if (state.has_secondary) {
    host.InvalidateMusicVolumes(state, Seq::ChannelGroup::kSecondaryMusic);
  }
  host.InvalidateSoundEffectVolumes(state);
}

std::uint16_t HalfAt(std::span<const std::uint8_t> bytes, std::size_t at) {
  return static_cast<std::uint16_t>(
      std::uint32_t{bytes[at]} | (std::uint32_t{bytes[at + 1]} << kByteBits));
}

std::uint32_t WordAt(std::span<const std::uint8_t> bytes, std::size_t at) {
  return std::uint32_t{HalfAt(bytes, at)} |
         (std::uint32_t{HalfAt(bytes, at + 2)} << kHalfBits);
}

// func_80018C30's track starts (0x10, 0x12, 0x19 and 0x1A).
std::uint32_t StartTrack(DispatchState& state, DispatchHost& host,
                         std::uint32_t& command) {
  const std::optional<AkaoHeader> header =
      host.SequenceAt(ArgumentSlot::kFirst);
  if (!header) {
    command = 0;
    return kNoTrack;
  }
  if (!state.active) {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
    command = 0;
    return kNoTrack;
  }
  state.control_latches &= ~kTransitionLatches;
  const bool replaces =
      state.active->music_id != header->sequence_id ||
      (state.secondary && state.secondary->music_id != state.active->music_id);
  if (!replaces) {
    command = 0;
    return 0;
  }
  SetReverbMode(host, header->reverb_mode);
  host.CopyFirstArgument();
  state.parameters.param3 = header->sequence_id;
  if (command == kStartMusicWithLoopCounter) {
    state.parameters.param4 = state.arguments.param2;
  } else if (command == kStartMusicLayer) {
    state.parameters.ext_param1 = state.arguments.param2;
  } else if (command == kCrossfadeMusicLayers) {
    state.parameters.ext_param1 =
        state.arguments.param2 != 0 ? state.arguments.param2 : 1;
    state.parameters.ext_param2 = state.arguments.param3;
  }
  return header->sequence_id;
}

// func_80018C30's command 0x14: queues the track to start when the active
// one ends. Its handler runs either way.
std::uint32_t QueueTrack(DispatchState& state, DispatchHost& host,
                         std::uint32_t& command) {
  const std::optional<AkaoHeader> header =
      host.SequenceAt(ArgumentSlot::kFirst);
  if (!header) return kNoTrack;
  if (!state.active) {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
    command = 0;
    return kNoTrack;
  }
  state.active->status_flags |= kMusicQueued;
  host.QueueFirstSequence();
  return header->sequence_id;
}

// func_80018C30's command 0x16: plays the second sequence as the saved
// layer and the first as the active one. The first argument becomes the
// first track's id and the second the second track's.
void StartTrackPair(DispatchState& state, DispatchHost& host) {
  const std::optional<AkaoHeader> first = host.SequenceAt(ArgumentSlot::kFirst);
  if (!first) return;
  const std::optional<AkaoHeader> second =
      host.SequenceAt(ArgumentSlot::kSecond);
  if (!second) return;
  if (!state.active) {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
    return;
  }
  state.control_latches &= ~kTransitionLatches;
  SetReverbMode(host, second->reverb_mode);
  state.arguments.param2 = second->sequence_id;
  host.LoadSequence(state, ArgumentSlot::kSecond);
  if (state.active) state.active->music_id = second->sequence_id;
  host.SaveActiveLayer(state);
  host.ReplaceFirstArgument(first->sequence_id);
  host.LoadSequence(state, ArgumentSlot::kFirst);
  host.AttachSavedLayer(state);
  if (state.active) state.active->music_id = first->sequence_id;
  // The saved layer is now the secondary one; when the active layer is
  // itself the saved record, the state holds it as the active layer.
  std::optional<Seq::MusicLayerState>& saved =
      state.secondary ? state.secondary : state.active;
  if (saved) saved->active_channels &= ~saved->push_release_mask;
  state.control_latches |= kSecondarySilenced;
}
}  // namespace

// decomp: SLUS_010.40 Sound_Cmd_A4_unk (src/SLUS_010.40/25AC.c:3427)
void SetSoundEffectPitchScale(CommandState& state, EffectSelection selection,
                              std::uint32_t pitch_word) {
  for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
    EffectControl& effect = state.effects[i];
    if ((state.scheduler.active_voices & (kFirstEffectVoiceBit << i)) == 0 ||
        !IsSelected(effect, selection)) {
      continue;
    }
    effect.pitch_scale = EffectPitchOf(pitch_word);
    effect.pitch_scale_length = 0;
    effect.dirty_flags |= Seq::kSampleRateDirty;
  }
}

// decomp: SLUS_010.40 Sound_Cmd_A5_unk (src/SLUS_010.40/25AC.c:3453)
void SlideSoundEffectPitchScale(CommandState& state, EffectSelection selection,
                                std::uint32_t length_word,
                                std::uint32_t target_word,
                                CommandRefusalSink& sink) {
  for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
    EffectControl& effect = state.effects[i];
    if ((state.scheduler.active_voices & (kFirstEffectVoiceBit << i)) != 0 &&
        IsSelected(effect, selection)) {
      SlideEffectPitch(effect, length_word, target_word, sink);
    }
  }
}

// decomp: SLUS_010.40 Sound_Cmd_AC_unk (src/SLUS_010.40/25AC.c:3481)
void SetWorldSoundEffectPitchScale(CommandState& state,
                                   const Seq::MusicCommand& command) {
  for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
    EffectControl& effect = state.effects[i];
    if (!IsWorldEffect(effect)) continue;
    effect.pitch_scale = EffectPitchOf(command.param1);
    effect.pitch_scale_length = 0;
    effect.dirty_flags |= Seq::kSampleRateDirty;
  }
}

// decomp: SLUS_010.40 Sound_Cmd_AD_unk (src/SLUS_010.40/25AC.c:3494)
void SlideWorldSoundEffectPitchScale(CommandState& state,
                                     const Seq::MusicCommand& command,
                                     CommandRefusalSink& sink) {
  for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
    EffectControl& effect = state.effects[i];
    if ((state.scheduler.active_voices & (kFirstEffectVoiceBit << i)) != 0 &&
        IsWorldEffect(effect)) {
      SlideEffectPitch(effect, command.param1, command.param2, sink);
    }
  }
}

// decomp: SLUS_010.40 func_8001833C (src/SLUS_010.40/25AC.c:3515)
void SetTempoScale(Seq::LevelSlide& tempo_scale, std::int8_t scale) {
  tempo_scale.value = std::int32_t{scale} * kScaleOne;
  tempo_scale.length = 0;
}

// decomp: SLUS_010.40 func_80018358 (src/SLUS_010.40/25AC.c:3521)
void SlideTempoScale(Seq::LevelSlide& tempo_scale,
                     const Seq::MusicCommand& command,
                     CommandRefusalSink& sink) {
  SlideScale(tempo_scale, command, sink);
}

// decomp: SLUS_010.40 func_800183C0 (src/SLUS_010.40/25AC.c:3532)
void SetTempoScaleRamp(Seq::LevelSlide& tempo_scale,
                       const Seq::MusicCommand& command,
                       CommandRefusalSink& sink) {
  RampScale(tempo_scale, command, sink);
}

// decomp: SLUS_010.40 func_80018434 (src/SLUS_010.40/25AC.c:3546)
void SetMusicPitchScale(Seq::LevelSlide& pitch_scale, std::int8_t scale) {
  pitch_scale.value = std::int32_t{scale} * kScaleOne;
  pitch_scale.length = 0;
}

// decomp: SLUS_010.40 func_80018450 (src/SLUS_010.40/25AC.c:3552)
void SlideMusicPitchScale(Seq::LevelSlide& pitch_scale,
                          const Seq::MusicCommand& command,
                          CommandRefusalSink& sink) {
  SlideScale(pitch_scale, command, sink);
}

// decomp: SLUS_010.40 func_800184B8 (src/SLUS_010.40/25AC.c:3568)
void SetMusicPitchScaleRamp(Seq::LevelSlide& pitch_scale,
                            const Seq::MusicCommand& command,
                            CommandRefusalSink& sink) {
  RampScale(pitch_scale, command, sink);
}

// decomp: SLUS_010.40 func_8001852C (src/SLUS_010.40/25AC.c:3582)
void StopMusicAndCancelReplacement(CommandState& state, ControlHost& host) {
  if (state.active) {
    host.ReleaseMusic(state, Seq::ChannelGroup::kMusic, 0);
  } else {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
  }
  if (state.has_secondary) {
    host.ReleaseMusic(state, Seq::ChannelGroup::kSecondaryMusic, 0);
  }
  if (state.active) state.active->status_flags &= ~kMusicQueued;
  state.music_queued = false;
}

// decomp: SLUS_010.40 Sound_Cmd_11_StopAllMusic (src/SLUS_010.40/25AC.c:3594)
void StopMatchingMusic(CommandState& state, const Seq::MusicCommand& command,
                       ControlHost& host) {
  if (state.active) {
    host.ReleaseMusic(state, Seq::ChannelGroup::kMusic, command.param1);
  } else {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
  }
  if (state.has_secondary && command.param1 != 0) {
    host.ReleaseMusic(state, Seq::ChannelGroup::kSecondaryMusic,
                      command.param1);
  }
}

// decomp: SLUS_010.40 func_80018608 (src/SLUS_010.40/25AC.c:3607)
void StopWorldSoundEffects(CommandState& state) {
  for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
    EffectControl& effect = state.effects[i];
    const std::uint32_t voice_bit = kFirstEffectVoiceBit << i;
    if ((state.scheduler.active_voices & voice_bit) == 0 ||
        !IsWorldEffect(effect)) {
      continue;
    }
    state.scheduler.pending_key_off |= voice_bit;
    Seq::EffectIdentity identity{effect.tags, effect.effect_id != EffectId{}};
    Seq::ReleaseSoundEffectVoice(state.scheduler, identity, voice_bit);
    effect.tags = identity.tags;
    if (!identity.has_effect) effect.effect_id = EffectId{};
    effect.update_flags = 0;
  }
  state.driver_update_flags |= Seq::kVoiceModeUpdate | Seq::kNoiseClockUpdate;
}

// decomp: SLUS_010.40 Sound_Cmd_80_unk (src/SLUS_010.40/25AC.c:3626)
void EnableStereoOutput(CommandState& state, ControlHost& host) {
  SetOutputMode(state, host, kStereoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_81_unk (src/SLUS_010.40/25AC.c:3637)
void EnableMonoOutput(CommandState& state, ControlHost& host) {
  SetOutputMode(state, host, kMonoOutput);
}

// decomp: SLUS_010.40 Sound_Cmd_90_unk (src/SLUS_010.40/25AC.c:3648)
void SetMusicMuteMask(CommandState& state, const Seq::MusicCommand& command) {
  state.muted_music_channels = static_cast<std::int32_t>(command.param1);
  for (std::uint32_t& dirty : state.music_dirty_flags) {
    dirty |= Seq::kVolumeDirty;
  }
}

// decomp: SLUS_010.40 Sound_Cmd_92_unk (src/SLUS_010.40/25AC.c:3660)
void SetMusicJumpThreshold(CommandState& state,
                           const Seq::MusicCommand& command,
                           CommandRefusalSink& sink) {
  if (!state.active) {
    sink.Refuse(CommandRefusal::kActiveLayerMissing);
    return;
  }
  state.active->jump_threshold = static_cast<std::uint16_t>(command.param1);
}

// decomp: SLUS_010.40 Sound_Cmd_9B_unk (src/SLUS_010.40/25AC.c:3665)
void PauseMusic(CommandState& state, ControlHost& host) {
  if (!state.active) {
    host.Refuse(CommandRefusal::kActiveLayerMissing);
  } else if (state.active->active_channels != 0) {
    std::uint32_t voices =
        ~(state.scheduler.active_voices | state.stream_voices) &
        Driver::kAllVoices;
    for (std::uint32_t voice = 0; voices != 0; ++voice) {
      const std::uint32_t voice_bit = 1U << voice;
      if ((voices & voice_bit) == 0) continue;
      SilenceVoice(host, voice);
      voices &= ~voice_bit;
    }
    state.active->paused_channels =
        static_cast<std::uint32_t>(state.active->active_channels);
    state.active->active_channels = 0;
  }
  state.stream_control_flags |= kMusicPaused;
}

// decomp: SLUS_010.40 Sound_Cmd_9A_unk (src/SLUS_010.40/25AC.c:3698)
void ResumeMusic(CommandState& state, CommandRefusalSink& sink) {
  if (!state.active) {
    sink.Refuse(CommandRefusal::kActiveLayerMissing);
  } else if (state.active->paused_channels != 0) {
    const std::uint32_t paused = state.active->paused_channels;
    for (std::size_t i = 0; i < Seq::kMusicChannelCount; ++i) {
      if ((paused & (1U << i)) != 0) {
        state.music_dirty_flags[i] |= kResumeDirty;
      }
    }
    state.active->paused_channels = 0;
    state.active->active_channels = static_cast<std::int32_t>(paused);
    state.driver_update_flags |= Seq::kVoiceModeUpdate;
  }
  state.stream_control_flags &= ~kMusicPaused;
}

// decomp: SLUS_010.40 Sound_Cmd_9D_unk (src/SLUS_010.40/25AC.c:3722)
void PauseWorldSoundEffects(CommandState& state, ControlHost& host) {
  if (state.scheduler.active_voices != 0) {
    std::uint32_t voices = state.scheduler.active_voices;
    for (std::size_t i = 0; i < kWorldEffectChannelCount; ++i) {
      if (!IsWorldEffect(state.effects[i])) {
        voices &= ~(kFirstEffectVoiceBit << i);
      }
    }
    state.scheduler.paused_effect_voices = voices;
    state.scheduler.active_voices &= ~voices;
    if ((voices & kVoicesBelowEffects) != 0) {
      host.Refuse(CommandRefusal::kEffectVoiceOutsideChannels);
      voices &= ~kVoicesBelowEffects;
    }
    for (std::uint32_t voice = Seq::kFirstEffectVoice; voices != 0; ++voice) {
      const std::uint32_t voice_bit = 1U << voice;
      if ((voices & voice_bit) == 0) continue;
      SilenceVoice(host, voice);
      voices &= ~voice_bit;
    }
  }
  state.stream_control_flags |= kEffectsPaused;
}

// decomp: SLUS_010.40 Sound_Cmd_9C_unk (src/SLUS_010.40/25AC.c:3761)
void ResumeWorldSoundEffects(CommandState& state, CommandRefusalSink& sink) {
  const std::uint32_t paused = state.scheduler.paused_effect_voices;
  if (paused != 0) {
    if ((paused & kVoicesBelowEffects) != 0) {
      sink.Refuse(CommandRefusal::kEffectVoiceOutsideChannels);
    }
    for (std::size_t i = 0; i < state.effects.size(); ++i) {
      if ((paused & (kFirstEffectVoiceBit << i)) != 0) {
        state.effects[i].dirty_flags |= kResumeDirty;
      }
    }
    state.scheduler.paused_effect_voices = 0;
    state.scheduler.active_voices |= paused;
    state.driver_update_flags |= Seq::kVoiceModeUpdate;
  }
  state.stream_control_flags &= ~kEffectsPaused;
}

// decomp: SLUS_010.40 Sound_Cmd_11_Nop (src/SLUS_010.40/25AC.c:3804)
void IgnoreCommand() {}

// decomp: SLUS_010.40 Sound_SetReverbMode (src/SLUS_010.40/25AC.c:3806)
void SetReverbMode(ReverbHost& host, std::int32_t mode) {
  if (host.ReverbMode() == mode) return;
  host.EnableReverb(false);
  host.SetReverbMode(mode);
  host.EnableReverb(true);
}

std::optional<AkaoHeader> ReadAkaoHeader(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kAkaoHeaderBytes ||
      WordAt(bytes, 0) != Samples::kAkaoMagic) {
    return std::nullopt;
  }
  return AkaoHeader{HalfAt(bytes, kAkaoSequenceIdAt),
                    HalfAt(bytes, kAkaoReverbModeAt)};
}

// decomp: SLUS_010.40 func_80018C30 (src/SLUS_010.40/25AC.c:3818)
std::uint32_t DispatchCommand(DispatchState& state, DispatchHost& host,
                              std::uint32_t command) {
  std::uint32_t result = 0;
  host.BeginCommand();
  command &= kByteMask;
  switch (command) {
    case kStartMusic:
    case kStartMusicWithLoopCounter:
    case kStartMusicLayer:
    case kCrossfadeMusicLayers:
      result = StartTrack(state, host, command);
      break;
    case kQueueMusic:
      result = QueueTrack(state, host, command);
      break;
    case kStartMusicPair:
      StartTrackPair(state, host);
      command = 0;
      break;
    case kResumeMusicLayer:
      if ((state.control_latches & (kSecondarySilenced | kActiveSilenced)) !=
          0) {
        state.control_latches |=
            (state.control_latches & kSecondarySilenced) != 0
                ? kSecondaryResumePending
                : kActiveResumePending;
      }
      command = 0;
      break;
    case kSetScales:
      host.CopyFirstArgument();
      host.RunHandler(state, kSetTempoHandler);
      command = kSetPitchHandler;
      break;
    case kSlideScales:
      host.CopyFirstArgument();
      state.parameters.param2 = state.arguments.param2;
      host.RunHandler(state, kSlideTempoHandler);
      command = kSlidePitchHandler;
      break;
    case kRampScales:
      host.CopyFirstArgument();
      state.parameters.param2 = state.arguments.param2;
      state.parameters.param3 = state.arguments.param3;
      host.RunHandler(state, kRampTempoHandler);
      command = kRampPitchHandler;
      break;
    case kPauseAll:
      host.RunHandler(state, kPauseMusicHandler);
      host.RunHandler(state, kPauseEffectsHandler);
      command = kPauseStreamHandler;
      break;
    case kResumeAll:
      host.RunHandler(state, kResumeMusicHandler);
      host.RunHandler(state, kResumeEffectsHandler);
      command = kResumeStreamHandler;
      break;
    default:
      host.CopyFirstArgument();
      state.parameters = state.arguments;
      break;
  }
  host.RunHandler(state, command);
  host.EndCommand();
  return result;
}
}  // namespace VS::Audio::Mixer
