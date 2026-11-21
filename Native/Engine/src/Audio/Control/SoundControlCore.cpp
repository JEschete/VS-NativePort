// Hand-maintained (tools/port/owned.csv).
// Typed sound control requests (SoundControl.hpp).
#include <cstddef>
#include <cstdint>
#include <span>

#include "Audio/SoundControl.hpp"  // NOLINT(build/include): typed API
#include "Audio/SoundPlayback.hpp"

namespace VS::Audio::Control {
namespace {
using Playback::CommandArguments;
using Playback::EffectSelector;
using Playback::kEffectTagMask;
using Playback::kPanMask;
using Playback::kVolumeMask;

std::uint32_t Masked(std::int32_t value, std::uint32_t mask) {
  return static_cast<std::uint32_t>(value) & mask;
}

std::uint32_t Word(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

// The first word as the original's (void*)int: sign-extended.
std::intptr_t Signed(std::int32_t value) { return value; }

// The first word as the original's (void*)(u_long) or a masked value:
// zero-extended.
std::intptr_t Unsigned(std::uint32_t value) {
  return static_cast<std::intptr_t>(value);
}

std::intptr_t Selector(EffectSelector effect) {
  return static_cast<std::intptr_t>(effect);
}

// The stream's 16-bit volume and pan levels keep their value in the high
// byte.
std::uint32_t StreamLevel(std::int32_t value, std::uint32_t mask) {
  return Masked(value, mask) << kStreamLevelShift;
}

void Send(ControlHost& host, Command command,
          const CommandArguments& arguments = {}) {
  host.Dispatch(command, arguments);
}

void SetScene(ControlHost& host, Command command, std::int32_t value,
              std::uint32_t mask) {
  Send(host, command, {.first = Unsigned(Masked(value, mask))});
}

void FadeScene(ControlHost& host, Command command, Fade fade,
               std::uint32_t mask) {
  Send(host, command,
       {.first = Signed(fade.ticks), .second = Masked(fade.target, mask)});
}

void FadeFrom(ControlHost& host, Command command, Ramp ramp,
              std::uint32_t mask) {
  Send(host, command,
       {.first = Signed(ramp.ticks),
        .second = Masked(ramp.start, mask),
        .third = Masked(ramp.target, mask)});
}

void SetEffect(ControlHost& host, Command command, EffectTarget target,
               std::int32_t value, std::uint32_t mask) {
  Send(host, command,
       {.first = Selector(target.effect),
        .second = Masked(target.tags, kEffectTagMask),
        .third = Masked(value, mask)});
}

void FadeEffect(ControlHost& host, Command command, EffectTarget target,
                Fade fade, std::uint32_t mask) {
  Send(host, command,
       {.first = Selector(target.effect),
        .second = Masked(target.tags, kEffectTagMask),
        .third = Word(fade.ticks),
        .fourth = Masked(fade.target, mask)});
}

Command PauseCommand(PlaybackGroup group) {
  switch (group) {
    case PlaybackGroup::kMusic:
      return Command::kPauseMusic;
    case PlaybackGroup::kWorldEffects:
      return Command::kPauseWorldEffects;
    case PlaybackGroup::kStream:
      return Command::kPauseStream;
    case PlaybackGroup::kAll:
      break;
  }
  return Command::kPauseAll;
}

Command ResumeCommand(PlaybackGroup group) {
  switch (group) {
    case PlaybackGroup::kMusic:
      return Command::kResumeMusic;
    case PlaybackGroup::kWorldEffects:
      return Command::kResumeWorldEffects;
    case PlaybackGroup::kStream:
      return Command::kResumeStream;
    case PlaybackGroup::kAll:
      break;
  }
  return Command::kResumeAll;
}
}  // namespace

// decomp: SLUS_010.40 Sound_SetMutedMusicChannelMask
// (src/SLUS_010.40/25AC.c:460; was func_80012440)
void SetMusicMuteMask(ControlHost& host, std::uint32_t mute_mask) {
  Send(host, Command::kSetMusicMuteMask, {.first = Unsigned(mute_mask)});
}

// decomp: SLUS_010.40 Sound_SetMusicJumpThreshold (src/SLUS_010.40/25AC.c:466;
// was func_80012468)
void SetMusicBranchThreshold(ControlHost& host, std::int32_t threshold) {
  Send(host, Command::kSetMusicBranchThreshold, {.first = Signed(threshold)});
}

// decomp: SLUS_010.40 Sound_SuspendChannelsByType (src/SLUS_010.40/25AC.c:472;
// was func_80012490)
void PausePlaybackGroup(ControlHost& host, PlaybackGroup group) {
  Send(host, PauseCommand(group));
}

// decomp: SLUS_010.40 Sound_RestoreChannelsByType (src/SLUS_010.40/25AC.c:490;
// was func_800124FC)
void ResumePlaybackGroup(ControlHost& host, PlaybackGroup group) {
  Send(host, ResumeCommand(group));
}

// decomp: SLUS_010.40 func_80012568 (src/SLUS_010.40/25AC.c:508)
void SetMixOptionLatch(ControlHost& host, bool enabled) {
  const std::uint32_t behavior = host.MixBehavior();
  host.SetMixBehavior(enabled ? behavior | kMixOptionLatch
                              : behavior & ~kMixOptionLatch);
}

// decomp: SLUS_010.40 Sound_SetAllSfxVolumeMod (src/SLUS_010.40/25AC.c:518; was
// func_800125A8)
void SetSceneSoundEffectVolume(ControlHost& host, std::uint32_t volume) {
  Send(host, Command::kSetSceneEffectVolume,
       {.first = Unsigned(volume & kVolumeMask)});
}

// decomp: SLUS_010.40 Sound_FadeAllSfxVolumeMod (src/SLUS_010.40/25AC.c:524;
// was func_800125D4)
void FadeSceneSoundEffectVolume(ControlHost& host, Fade fade) {
  FadeScene(host, Command::kFadeSceneEffectVolume, fade, kVolumeMask);
}

// decomp: SLUS_010.40 Sound_SetSfxVolumeMod (src/SLUS_010.40/25AC.c:531; was
// func_80012608)
void SetSoundEffectVolume(ControlHost& host, EffectTarget target,
                          std::int32_t volume) {
  SetEffect(host, Command::kSetEffectVolume, target, volume, kVolumeMask);
}

// decomp: SLUS_010.40 Sound_FadeSfxVolumeMod (src/SLUS_010.40/25AC.c:539; was
// func_8001264C)
void FadeSoundEffectVolume(ControlHost& host, EffectTarget target, Fade fade) {
  FadeEffect(host, Command::kFadeEffectVolume, target, fade, kVolumeMask);
}

// decomp: SLUS_010.40 Sound_SetAllSfxPanMod (src/SLUS_010.40/25AC.c:548; was
// func_80012694)
void SetSceneSoundEffectPan(ControlHost& host, std::int32_t pan) {
  SetScene(host, Command::kSetSceneEffectPan, pan, kPanMask);
}

// decomp: SLUS_010.40 Sound_FadeAllSfxPanMod (src/SLUS_010.40/25AC.c:554; was
// func_800126C0)
void FadeSceneSoundEffectPan(ControlHost& host, Fade fade) {
  FadeScene(host, Command::kFadeSceneEffectPan, fade, kPanMask);
}

// decomp: SLUS_010.40 Sound_SetSfxPanMod (src/SLUS_010.40/25AC.c:561; was
// func_800126F4)
void SetSoundEffectPan(ControlHost& host, EffectTarget target,
                       std::int32_t pan) {
  SetEffect(host, Command::kSetEffectPan, target, pan, kPanMask);
}

// decomp: SLUS_010.40 Sound_FadeSfxPanMod (src/SLUS_010.40/25AC.c:569; was
// func_80012738)
void FadeSoundEffectPan(ControlHost& host, EffectTarget target, Fade fade) {
  FadeEffect(host, Command::kFadeEffectPan, target, fade, kPanMask);
}

// decomp: SLUS_010.40 Sound_SetAllSfxPitchMod (src/SLUS_010.40/25AC.c:578; was
// func_80012780)
void SetSceneSoundEffectPitch(ControlHost& host, std::int32_t pitch_scale) {
  SetScene(host, Command::kSetSceneEffectPitch, pitch_scale, kByteMask);
}

// decomp: SLUS_010.40 Sound_FadeAllSfxPitchMod (src/SLUS_010.40/25AC.c:584; was
// func_800127AC)
void FadeSceneSoundEffectPitch(ControlHost& host, Fade fade) {
  FadeScene(host, Command::kFadeSceneEffectPitch, fade, kByteMask);
}

// decomp: SLUS_010.40 Sound_SetSfxPitchMod (src/SLUS_010.40/25AC.c:591; was
// func_800127E0)
void SetSoundEffectPitch(ControlHost& host, EffectTarget target,
                         std::int32_t pitch_scale) {
  SetEffect(host, Command::kSetEffectPitch, target, pitch_scale, kByteMask);
}

// decomp: SLUS_010.40 Sound_FadeSfxPitchMod (src/SLUS_010.40/25AC.c:599; was
// func_80012824)
void FadeSoundEffectPitch(ControlHost& host, EffectTarget target, Fade fade) {
  FadeEffect(host, Command::kFadeEffectPitch, target, fade, kByteMask);
}

// decomp: SLUS_010.40 Sound_SetMasterVolumeByMusicId
// (src/SLUS_010.40/25AC.c:608; was func_8001286C)
void SetMusicVolume(ControlHost& host, std::uint32_t music_id,
                    std::int32_t volume) {
  Send(host, Command::kSetMusicVolume,
       {.first = Unsigned(music_id), .second = Masked(volume, kVolumeMask)});
}

// decomp: SLUS_010.40 Sound_FadeMasterVolumeByMusicId
// (src/SLUS_010.40/25AC.c:615; was func_800128A0)
void FadeMusicVolume(ControlHost& host, std::uint32_t music_id, Fade fade) {
  Send(host, Command::kFadeMusicVolume,
       {.first = Unsigned(music_id),
        .second = Word(fade.ticks),
        .third = Masked(fade.target, kVolumeMask)});
}

// decomp: SLUS_010.40 Sound_FadeMasterVolumeFromByMusicId
// (src/SLUS_010.40/25AC.c:623; was func_800128D8)
void FadeMusicVolumeFrom(ControlHost& host, std::uint32_t music_id, Ramp ramp) {
  Send(host, Command::kFadeMusicVolumeFrom,
       {.first = Unsigned(music_id),
        .second = Word(ramp.ticks),
        .third = Masked(ramp.start, kVolumeMask),
        .fourth = Masked(ramp.target, kVolumeMask)});
}

// decomp: SLUS_010.40 Sound_SetCdVolume (src/SLUS_010.40/25AC.c:632; was
// func_80012918)
void SetDiscVolume(ControlHost& host, std::int32_t volume) {
  Send(host, Command::kSetDiscVolume, {.first = Signed(volume)});
}

// decomp: SLUS_010.40 Sound_FadeCdVolume (src/SLUS_010.40/25AC.c:638; was
// func_80012940)
void FadeDiscVolume(ControlHost& host, Fade fade) {
  Send(host, Command::kFadeDiscVolume,
       {.first = Signed(fade.ticks), .second = Word(fade.target)});
}

// decomp: SLUS_010.40 Sound_FadeCdVolumeFrom (src/SLUS_010.40/25AC.c:645; was
// func_80012970)
void FadeDiscVolumeFrom(ControlHost& host, Ramp ramp) {
  Send(host, Command::kFadeDiscVolumeFrom,
       {.first = Signed(ramp.ticks),
        .second = Word(ramp.start),
        .third = Word(ramp.target)});
}

// decomp: SLUS_010.40 Sound_SetTempoScale (src/SLUS_010.40/25AC.c:653; was
// func_800129A4)
void SetMusicTempoScale(ControlHost& host, std::int32_t tempo_scale) {
  SetScene(host, Command::kSetTempoScale, tempo_scale, kByteMask);
}

// decomp: SLUS_010.40 Sound_FadeTempoScale (src/SLUS_010.40/25AC.c:659; was
// func_800129D0)
void FadeMusicTempoScale(ControlHost& host, Fade fade) {
  FadeScene(host, Command::kFadeTempoScale, fade, kByteMask);
}

// decomp: SLUS_010.40 Sound_FadeTempoScaleFrom (src/SLUS_010.40/25AC.c:666; was
// func_80012A04)
void FadeMusicTempoScaleFrom(ControlHost& host, Ramp ramp) {
  FadeFrom(host, Command::kFadeTempoScaleFrom, ramp, kByteMask);
}

// decomp: SLUS_010.40 Sound_SetMasterPitchScale (src/SLUS_010.40/25AC.c:674;
// was func_80012A40)
void SetMusicPitchScale(ControlHost& host, std::int32_t pitch_scale) {
  SetScene(host, Command::kSetPitchScale, pitch_scale, kByteMask);
}

// decomp: SLUS_010.40 Sound_FadeMasterPitchScale (src/SLUS_010.40/25AC.c:680;
// was func_80012A6C)
std::int32_t FadeMusicPitchScale(ControlHost& host, Fade fade) {
  return static_cast<std::int32_t>(host.Dispatch(
      Command::kFadePitchScale,
      {.first = Signed(fade.ticks), .second = Masked(fade.target, kByteMask)}));
}

// decomp: SLUS_010.40 Sound_FadeMasterPitchScaleFrom
// (src/SLUS_010.40/25AC.c:687; was func_80012AA0)
void FadeMusicPitchScaleFrom(ControlHost& host, Ramp ramp) {
  FadeFrom(host, Command::kFadePitchScaleFrom, ramp, kByteMask);
}

// decomp: SLUS_010.40 func_80012ADC (src/SLUS_010.40/25AC.c:695)
void SetMusicTempoAndPitch(ControlHost& host, std::int32_t scale) {
  SetScene(host, Command::kSetTempoAndPitch, scale, kByteMask);
}

// decomp: SLUS_010.40 func_80012B08 (src/SLUS_010.40/25AC.c:701)
void FadeMusicTempoAndPitch(ControlHost& host, Fade fade) {
  FadeScene(host, Command::kFadeTempoAndPitch, fade, kByteMask);
}

// decomp: SLUS_010.40 func_80012B3C (src/SLUS_010.40/25AC.c:708)
void FadeMusicTempoAndPitchFrom(ControlHost& host, Ramp ramp) {
  FadeFrom(host, Command::kFadeTempoAndPitchFrom, ramp, kByteMask);
}

// decomp: SLUS_010.40 Sound_StopMusic (src/SLUS_010.40/25AC.c:716; was
// func_80012B78)
void StopAllMusic(ControlHost& host) { Send(host, Command::kStopAllMusic); }

// decomp: SLUS_010.40 Sound_StopSfx (src/SLUS_010.40/25AC.c:718; was
// func_80012B98)
void StopSceneSoundEffects(ControlHost& host) {
  Send(host, Command::kStopSceneEffects);
}

// decomp: SLUS_010.40 Sound_StartCutsceneStream (src/SLUS_010.40/25AC.c:931;
// was func_800132C4)
void StartCutsceneStream(ControlHost& host, const StreamStart& start) {
  if (!host.IsSequence(start.stream)) return;
  Send(host, Command::kStartStream,
       {.first = start.stream,
        .second = StreamLevel(start.pan, kPanMask),
        .third = Word(start.reverb)});
}

// decomp: SLUS_010.40 Sound_StopCutsceneStream (src/SLUS_010.40/25AC.c:941; was
// func_80013328)
void StopCutsceneStream(ControlHost& host) { Send(host, Command::kStopStream); }

// decomp: SLUS_010.40 Sound_SetCutsceneVolume (src/SLUS_010.40/25AC.c:943; was
// func_80013348)
void SetCutsceneVolume(ControlHost& host, std::int32_t volume) {
  Send(host, Command::kSetStreamVolume,
       {.first = Unsigned(StreamLevel(volume, kVolumeMask))});
}

// decomp: SLUS_010.40 Sound_FadeOutCutscene (src/SLUS_010.40/25AC.c:949; was
// func_80013378)
void FadeCutsceneVolume(ControlHost& host, Fade fade) {
  Send(host, Command::kFadeStreamVolume,
       {.first = Signed(fade.ticks),
        .second = StreamLevel(fade.target, kVolumeMask)});
}

// decomp: SLUS_010.40 func_800133B0 (src/SLUS_010.40/25AC.c:956)
void SetCutscenePan(ControlHost& host, std::int32_t pan) {
  Send(host, Command::kSetStreamPan,
       {.first = Unsigned(StreamLevel(pan, kPanMask))});
}

// decomp: SLUS_010.40 func_800133E0 (src/SLUS_010.40/25AC.c:962)
std::uint32_t SoundEffectChannelIds(
    ControlHost& host,
    std::span<EffectSelector, Playback::kEffectVoiceCount> effects) {
  for (std::size_t voice = 0; voice < effects.size(); ++voice) {
    effects[voice] = host.EffectAt(voice);
  }
  return host.ActiveVoices();
}
}  // namespace VS::Audio::Control
