// Hand-maintained (tools/port/owned.csv).
// The sound driver's reset, written from its assembly: the decomp keeps
// func_8001369C as INCLUDE_ASM
// (build/src/SLUS_010.40/nonmatchings/25AC/func_8001369C.s, 0x29C bytes).
// Every store in the original targets an independent driver field; the reset
// keeps the original's order for the calls that have effects beyond those
// stores (the output reset first, reverb last) and groups the stores by owner.
#include "Audio/PlaybackReset.hpp"

namespace VS::Audio::Driver {
namespace {
// 127 in 16.16: the music master volume and full-scale reverb depth.
constexpr std::int32_t kMusicMasterVolume = 0x7F0000;
constexpr std::int32_t kMusicReverbDepth = 0x3FFF000;
constexpr std::uint32_t kEffectTempo = 0x66A80000U;
constexpr std::uint32_t kTempoAccumulator = 1;
constexpr std::uint32_t kMixBehavior = 1;
constexpr std::int32_t kDiscVolume = 0x7FFF0000;
constexpr std::int32_t kStreamVolume = 0x7F00;
constexpr std::int16_t kEffectVolume = 0x7F00;
constexpr std::uint32_t kVoiceModeUpdate = 0x80;
constexpr int kReverbMode = 4;
}  // namespace

PlaybackReset DefaultPlaybackReset() {
  return PlaybackReset{
      .music = {.master_volume = kMusicMasterVolume,
                .reverb_depth = kMusicReverbDepth},
      .voices = {.effect_tempo = kEffectTempo,
                 .tempo_accumulator = kTempoAccumulator},
      .controls = {.mix_behavior = kMixBehavior,
                   .disc_volume = kDiscVolume,
                   .stream_volume = kStreamVolume},
      .voice_mode_update = kVoiceModeUpdate,
      .reverb_mode = kReverbMode,
  };
}

bool ChannelResetFor(ChannelKind kind, std::size_t index, ChannelReset& reset) {
  if (kind == ChannelKind::kMusic) {
    if (index >= kMusicChannelCount) return false;
    reset = ChannelReset{.kind = kind, .voice = kNoVoice, .effect_volume = 0};
    return true;
  }
  if (index >= kSoundEffectChannelCount) return false;
  reset = ChannelReset{
      .kind = kind,
      .voice = kFirstSoundEffectVoice + static_cast<std::uint32_t>(index),
      .effect_volume = kEffectVolume};
  return true;
}

// decomp: SLUS_010.40 func_8001369C (src/SLUS_010.40/25AC.c:1058)
void ResetPlaybackState(PlaybackResetHost& host) {
  const PlaybackReset reset = DefaultPlaybackReset();
  host.ResetOutput();
  host.ResetMusicLayers(reset.music);
  host.ResetVoiceScheduler(reset.voices);
  host.ResetDriverControls(reset.controls);
  ChannelReset channel;
  for (std::size_t index = 0; index < kMusicChannelCount; ++index) {
    if (ChannelResetFor(ChannelKind::kMusic, index, channel)) {
      host.ResetChannel(index, channel);
    }
  }
  for (std::size_t index = 0; index < kSoundEffectChannelCount; ++index) {
    if (ChannelResetFor(ChannelKind::kSoundEffect, index, channel)) {
      host.ResetChannel(index, channel);
    }
  }
  host.RequestVoiceModeUpdate(reset.voice_mode_update);
  host.SetReverbMode(reset.reverb_mode);
  host.EnableReverb(true);
}
}  // namespace VS::Audio::Driver
