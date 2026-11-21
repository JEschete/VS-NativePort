// Hand-maintained (tools/port/owned.csv).
// Live binding for SoundControl.hpp: D_800378C0 and func_80018C30, the
// driver flags, the voice scheduler and the sound-effect channels.
#include "Audio/SoundControlLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Audio/SoundPlaybackLive.hpp"

namespace VS::Audio::Control {
namespace {
using Playback::CommandArguments;
using Playback::DataRef;
using Playback::EffectSelector;
using Playback::kEffectVoiceCount;

static_assert(std::size(VS::Audio::SoundEffects::channels) >=
              kEffectVoiceCount);
static_assert(
    std::is_same_v<decltype(VS::Audio::SeqChannel::effect_id), std::uintptr_t>);
static_assert(std::is_same_v<decltype(VS::Audio::DriverFlags::mix_behavior),
                             unsigned int>);

void* Pointer(DataRef data) {
  return std::bit_cast<void*>(static_cast<std::uintptr_t>(data));
}

DataRef Reference(const void* data) {
  return static_cast<DataRef>(std::bit_cast<std::uintptr_t>(data));
}

class LiveControl final : public ControlHost {
 public:
  std::uint32_t Dispatch(Command command,
                         const CommandArguments& arguments) override {
    Playback::LiveWriteCommandArguments(arguments);
    return Mixer::LiveDispatchCommand(static_cast<unsigned>(command));
  }

  std::uint32_t MixBehavior() override {
    return VS::Audio::Driver::controlLatches.mix_behavior;
  }

  void SetMixBehavior(std::uint32_t behavior) override {
    VS::Audio::Driver::controlLatches.mix_behavior = behavior;
  }

  bool IsSequence(DataRef data) override {
    return Samples::LiveSequenceMagicDifference(Pointer(data)) == 0;
  }

  std::uint32_t ActiveVoices() override {
    return VS::Audio::Mixer::voiceScheduler.active_voices;
  }

  EffectSelector EffectAt(std::size_t voice) override {
    if (voice >= kEffectVoiceCount) return EffectSelector::kNone;
    return static_cast<EffectSelector>(static_cast<std::intptr_t>(
        VS::Audio::SoundEffects::channels[voice].effect_id));
  }
};

EffectTarget Target(std::intptr_t effect, int tag_mask) {
  return {.effect = static_cast<EffectSelector>(effect), .tags = tag_mask};
}
}  // namespace

// decomp: SLUS_010.40 Sound_SetMusicJumpThreshold (src/SLUS_010.40/25AC.c:466;
// was func_80012468)
void LiveSetMusicBranchThreshold(void* threshold) {
  LiveControl host;
  SetMusicBranchThreshold(
      host, static_cast<std::int32_t>(std::bit_cast<std::intptr_t>(threshold)));
}

// decomp: SLUS_010.40 Sound_SetMasterVolumeByMusicId
// (src/SLUS_010.40/25AC.c:608; was func_8001286C)
void LiveSetMusicVolume(std::uint32_t music_id, int volume) {
  LiveControl host;
  SetMusicVolume(host, music_id, volume);
}

// decomp: SLUS_010.40 Sound_FadeMasterVolumeByMusicId
// (src/SLUS_010.40/25AC.c:615; was func_800128A0)
void LiveFadeMusicVolume(std::uint32_t music_id, int fade_ticks,
                         int target_volume) {
  LiveControl host;
  FadeMusicVolume(host, music_id,
                  {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 Sound_FadeSfxVolumeMod (src/SLUS_010.40/25AC.c:539; was
// func_8001264C)
void LiveFadeSoundEffectVolume(std::intptr_t effect, int tag_mask,
                               int fade_ticks, int target_volume) {
  LiveControl host;
  FadeSoundEffectVolume(host, Target(effect, tag_mask),
                        {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 Sound_FadeSfxPanMod (src/SLUS_010.40/25AC.c:569; was
// func_80012738)
void LiveFadeSoundEffectPan(std::intptr_t effect, int tag_mask, int fade_ticks,
                            int target_pan) {
  LiveControl host;
  FadeSoundEffectPan(host, Target(effect, tag_mask),
                     {.ticks = fade_ticks, .target = target_pan});
}

// decomp: SLUS_010.40 Sound_FadeSfxPitchMod (src/SLUS_010.40/25AC.c:599; was
// func_80012824)
void LiveFadeSoundEffectPitch(std::intptr_t effect, int tag_mask,
                              int fade_ticks, int target_pitch_scale) {
  LiveControl host;
  FadeSoundEffectPitch(host, Target(effect, tag_mask),
                       {.ticks = fade_ticks, .target = target_pitch_scale});
}

// decomp: SLUS_010.40 Sound_SetCdVolume (src/SLUS_010.40/25AC.c:632; was
// func_80012918)
void LiveSetDiscVolume(int volume) {
  LiveControl host;
  SetDiscVolume(host, volume);
}

// decomp: SLUS_010.40 Sound_FadeCdVolume (src/SLUS_010.40/25AC.c:638; was
// func_80012940)
void LiveFadeDiscVolume(int fade_ticks, int target_volume) {
  LiveControl host;
  FadeDiscVolume(host, {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 Sound_FadeCdVolumeFrom (src/SLUS_010.40/25AC.c:645; was
// func_80012970)
void LiveFadeDiscVolumeFrom(int fade_ticks, int start_volume,
                            int target_volume) {
  LiveControl host;
  FadeDiscVolumeFrom(
      host,
      {.ticks = fade_ticks, .start = start_volume, .target = target_volume});
}

// decomp: SLUS_010.40 Sound_FadeTempoScale (src/SLUS_010.40/25AC.c:659; was
// func_800129D0)
void LiveFadeMusicTempoScale(int fade_ticks, int target_tempo_scale) {
  LiveControl host;
  FadeMusicTempoScale(host,
                      {.ticks = fade_ticks, .target = target_tempo_scale});
}

// decomp: SLUS_010.40 Sound_FadeMasterPitchScale (src/SLUS_010.40/25AC.c:680;
// was func_80012A6C)
int LiveFadeMusicPitchScale(int fade_ticks, int target_pitch_scale) {
  LiveControl host;
  return FadeMusicPitchScale(
      host, {.ticks = fade_ticks, .target = target_pitch_scale});
}

// decomp: SLUS_010.40 Sound_StopMusic (src/SLUS_010.40/25AC.c:716; was
// func_80012B78)
void LiveStopAllMusic() {
  LiveControl host;
  StopAllMusic(host);
}

// decomp: SLUS_010.40 Sound_StopSfx (src/SLUS_010.40/25AC.c:718; was
// func_80012B98)
void LiveStopSceneSoundEffects() {
  LiveControl host;
  StopSceneSoundEffects(host);
}

// decomp: SLUS_010.40 Sound_StartCutsceneStream (src/SLUS_010.40/25AC.c:931;
// was func_800132C4)
void LiveStartCutsceneStream(void* stream, int pan, int reverb) {
  LiveControl host;
  StartCutsceneStream(
      host, {.stream = Reference(stream), .pan = pan, .reverb = reverb});
}

// decomp: SLUS_010.40 Sound_StopCutsceneStream (src/SLUS_010.40/25AC.c:941; was
// func_80013328)
void LiveStopCutsceneStream() {
  LiveControl host;
  StopCutsceneStream(host);
}

// decomp: SLUS_010.40 Sound_SetCutsceneVolume (src/SLUS_010.40/25AC.c:943; was
// func_80013348)
void LiveSetCutsceneVolume(int volume) {
  LiveControl host;
  SetCutsceneVolume(host, volume);
}

// decomp: SLUS_010.40 Sound_FadeOutCutscene (src/SLUS_010.40/25AC.c:949; was
// func_80013378)
void LiveFadeCutsceneVolume(int fade_ticks, int target_volume) {
  LiveControl host;
  FadeCutsceneVolume(host, {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 func_800133B0 (src/SLUS_010.40/25AC.c:956)
void LiveSetCutscenePan(int pan) {
  LiveControl host;
  SetCutscenePan(host, pan);
}

// decomp: SLUS_010.40 func_800133E0 (src/SLUS_010.40/25AC.c:962)
unsigned LiveSoundEffectChannelIds(std::intptr_t* effects) {
  if (effects == nullptr) {
    Seq::ReportRefusal("SoundEffectChannelIds", "no effect id buffer");
    return 0;
  }
  LiveControl host;
  std::array<EffectSelector, kEffectVoiceCount> ids{};
  const std::uint32_t active = SoundEffectChannelIds(host, ids);
  const std::span<std::intptr_t, kEffectVoiceCount> out(effects,
                                                        kEffectVoiceCount);
  for (std::size_t voice = 0; voice < kEffectVoiceCount; ++voice) {
    out[voice] = static_cast<std::intptr_t>(ids[voice]);
  }
  return active;
}
}  // namespace VS::Audio::Control
