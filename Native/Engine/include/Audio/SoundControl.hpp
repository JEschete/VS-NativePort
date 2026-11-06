#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Audio/SoundPlayback.hpp"

// The sound control requests (func_80012440 through func_800133E0,
// SLUS_010.40/25AC.c:459-967): the music mute mask and branch threshold, the
// playback pauses and resumes, the sound effects' volume, pan and pitch, the
// music, disc and cutscene stream volumes, the music tempo and pitch scales,
// the music and scene stops, the mix option latch and the sound-effect
// voices' effects. Each request writes the command words it passes and
// dispatches its mixer command through ControlHost.
namespace VS::Audio::Control {
// The mixer commands the requests issue.
enum class Command : std::uint8_t {
  kSetMusicMuteMask = 0x90,
  kSetMusicBranchThreshold = 0x92,
  kResumeAll = 0x98,
  kPauseAll = 0x99,
  kResumeMusic = 0x9A,
  kPauseMusic = 0x9B,
  kResumeWorldEffects = 0x9C,
  kPauseWorldEffects = 0x9D,
  kResumeStream = 0x9E,
  kPauseStream = 0x9F,
  kSetEffectVolume = 0xA0,
  kFadeEffectVolume = 0xA1,
  kSetEffectPan = 0xA2,
  kFadeEffectPan = 0xA3,
  kSetEffectPitch = 0xA4,
  kFadeEffectPitch = 0xA5,
  kSetSceneEffectVolume = 0xA8,
  kFadeSceneEffectVolume = 0xA9,
  kSetSceneEffectPan = 0xAA,
  kFadeSceneEffectPan = 0xAB,
  kSetSceneEffectPitch = 0xAC,
  kFadeSceneEffectPitch = 0xAD,
  kSetMusicVolume = 0xC0,
  kFadeMusicVolume = 0xC1,
  kFadeMusicVolumeFrom = 0xC2,
  kSetDiscVolume = 0xC8,
  kFadeDiscVolume = 0xC9,
  kFadeDiscVolumeFrom = 0xCA,
  kSetTempoScale = 0xD0,
  kFadeTempoScale = 0xD1,
  kFadeTempoScaleFrom = 0xD2,
  kSetPitchScale = 0xD4,
  kFadePitchScale = 0xD5,
  kFadePitchScaleFrom = 0xD6,
  kSetTempoAndPitch = 0xD8,
  kFadeTempoAndPitch = 0xD9,
  kFadeTempoAndPitchFrom = 0xDA,
  kStartStream = 0xE0,
  kStopStream = 0xE2,
  kSetStreamVolume = 0xE4,
  kFadeStreamVolume = 0xE5,
  kSetStreamPan = 0xE6,
  kStopAllMusic = 0xF0,
  kStopSceneEffects = 0xF1,
};

// The playback a pause or resume names; any other value names all of it.
enum class PlaybackGroup : std::uint32_t {
  kAll = 0,
  kMusic = 1,
  kWorldEffects = 2,
  kStream = 3,
};

// FSoundGlobalFlags.mixBehavior's option latch.
inline constexpr std::uint32_t kMixOptionLatch = 0x10;
// The scale and pan words keep their low byte; the stream's volume and pan
// move to the high byte of a 16-bit level.
inline constexpr std::uint32_t kByteMask = 0xFF;
inline constexpr unsigned kStreamLevelShift = 8;

// The sound effects a request selects: those carrying one of `tags` (its
// low 24 bits), or, without tags, those playing `effect`.
struct EffectTarget {
  Playback::EffectSelector effect = Playback::EffectSelector::kNone;
  std::int32_t tags = 0;
};

// A slide toward `target` over `ticks`.
struct Fade {
  std::int32_t ticks = 0;
  std::int32_t target = 0;
};

// A slide from `start` toward `target` over `ticks`.
struct Ramp {
  std::int32_t ticks = 0;
  std::int32_t start = 0;
  std::int32_t target = 0;
};

// The cutscene stream a start request plays.
struct StreamStart {
  Playback::DataRef stream = Playback::DataRef::kNone;
  std::int32_t pan = 0;
  std::int32_t reverb = 0;
};

class ControlHost {
 public:
  virtual ~ControlHost() = default;
  // Writes the words `arguments` passes into D_800378C0 and runs
  // func_80018C30 on `command`; returns its result.
  virtual std::uint32_t Dispatch(
      Command command, const Playback::CommandArguments& arguments) = 0;
  // FSoundGlobalFlags.mixBehavior.
  virtual std::uint32_t MixBehavior() = 0;
  virtual void SetMixBehavior(std::uint32_t behavior) = 0;
  // Whether `data` is an AKAO sequence (its magic difference is 0).
  virtual bool IsSequence(Playback::DataRef data) = 0;
  // The voice scheduler's active voices.
  virtual std::uint32_t ActiveVoices() = 0;
  // The effect sound-effect voice `voice` (below kEffectVoiceCount) plays.
  virtual Playback::EffectSelector EffectAt(std::size_t voice) = 0;
};

// func_80012440 (command 0x90).
void SetMusicMuteMask(ControlHost& host, std::uint32_t mute_mask);
// func_80012468 (command 0x92).
void SetMusicBranchThreshold(ControlHost& host, std::int32_t threshold);
// func_80012490.
void PausePlaybackGroup(ControlHost& host, PlaybackGroup group);
// func_800124FC.
void ResumePlaybackGroup(ControlHost& host, PlaybackGroup group);
// func_80012568: sets or clears the mix option latch (the original's result
// is always 0).
void SetMixOptionLatch(ControlHost& host, bool enabled);

// func_800125A8 (command 0xA8): the scene effects' volume (low 7 bits).
void SetSceneSoundEffectVolume(ControlHost& host, std::uint32_t volume);
// func_800125D4 (command 0xA9).
void FadeSceneSoundEffectVolume(ControlHost& host, Fade fade);
// func_80012608 (command 0xA0).
void SetSoundEffectVolume(ControlHost& host, EffectTarget target,
                          std::int32_t volume);
// func_8001264C (command 0xA1).
void FadeSoundEffectVolume(ControlHost& host, EffectTarget target, Fade fade);
// func_80012694 (command 0xAA): the scene effects' pan (low byte).
void SetSceneSoundEffectPan(ControlHost& host, std::int32_t pan);
// func_800126C0 (command 0xAB).
void FadeSceneSoundEffectPan(ControlHost& host, Fade fade);
// func_800126F4 (command 0xA2).
void SetSoundEffectPan(ControlHost& host, EffectTarget target,
                       std::int32_t pan);
// func_80012738 (command 0xA3).
void FadeSoundEffectPan(ControlHost& host, EffectTarget target, Fade fade);
// func_80012780 (command 0xAC): the scene effects' pitch scale (low byte).
void SetSceneSoundEffectPitch(ControlHost& host, std::int32_t pitch_scale);
// func_800127AC (command 0xAD).
void FadeSceneSoundEffectPitch(ControlHost& host, Fade fade);
// func_800127E0 (command 0xA4).
void SetSoundEffectPitch(ControlHost& host, EffectTarget target,
                         std::int32_t pitch_scale);
// func_80012824 (command 0xA5).
void FadeSoundEffectPitch(ControlHost& host, EffectTarget target, Fade fade);

// func_8001286C (command 0xC0): the music's volume (low 7 bits).
void SetMusicVolume(ControlHost& host, std::uint32_t music_id,
                    std::int32_t volume);
// func_800128A0 (command 0xC1).
void FadeMusicVolume(ControlHost& host, std::uint32_t music_id, Fade fade);
// func_800128D8 (command 0xC2).
void FadeMusicVolumeFrom(ControlHost& host, std::uint32_t music_id, Ramp ramp);
// func_80012918 (command 0xC8): the disc audio's volume, whole.
void SetDiscVolume(ControlHost& host, std::int32_t volume);
// func_80012940 (command 0xC9).
void FadeDiscVolume(ControlHost& host, Fade fade);
// func_80012970 (command 0xCA).
void FadeDiscVolumeFrom(ControlHost& host, Ramp ramp);
// func_800129A4 (command 0xD0): the tempo scale (low byte).
void SetMusicTempoScale(ControlHost& host, std::int32_t tempo_scale);
// func_800129D0 (command 0xD1).
void FadeMusicTempoScale(ControlHost& host, Fade fade);
// func_80012A04 (command 0xD2).
void FadeMusicTempoScaleFrom(ControlHost& host, Ramp ramp);
// func_80012A40 (command 0xD4): the music pitch scale (low byte).
void SetMusicPitchScale(ControlHost& host, std::int32_t pitch_scale);
// func_80012A6C (command 0xD5): returns the dispatch's result.
std::int32_t FadeMusicPitchScale(ControlHost& host, Fade fade);
// func_80012AA0 (command 0xD6).
void FadeMusicPitchScaleFrom(ControlHost& host, Ramp ramp);
// func_80012ADC (command 0xD8): the tempo and pitch scales (low byte).
void SetMusicTempoAndPitch(ControlHost& host, std::int32_t scale);
// func_80012B08 (command 0xD9).
void FadeMusicTempoAndPitch(ControlHost& host, Fade fade);
// func_80012B3C (command 0xDA).
void FadeMusicTempoAndPitchFrom(ControlHost& host, Ramp ramp);
// func_80012B78 (command 0xF0).
void StopAllMusic(ControlHost& host);
// func_80012B98 (command 0xF1).
void StopSceneSoundEffects(ControlHost& host);

// func_800132C4 (command 0xE0): starts the stream when it is an AKAO
// sequence, at its pan's low byte as the high byte of a 16-bit pan.
void StartCutsceneStream(ControlHost& host, const StreamStart& start);
// func_80013328 (command 0xE2).
void StopCutsceneStream(ControlHost& host);
// func_80013348 (command 0xE4).
void SetCutsceneVolume(ControlHost& host, std::int32_t volume);
// func_80013378 (command 0xE5).
void FadeCutsceneVolume(ControlHost& host, Fade fade);
// func_800133B0 (command 0xE6).
void SetCutscenePan(ControlHost& host, std::int32_t pan);
// func_800133E0: copies the sound-effect voices' effects into `effects` and
// returns the active voices.
std::uint32_t SoundEffectChannelIds(
    ControlHost& host,
    std::span<Playback::EffectSelector, Playback::kEffectVoiceCount> effects);
}  // namespace VS::Audio::Control
