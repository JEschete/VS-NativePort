#pragma once
// The voice mixer's records and state: each voice's parameters and envelope,
// the voice scheduler, the voice modes and owners, the pan gains, the pitch
// and tempo scales and the command table.
#include <cstddef>
#include <cstdint>

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/VoiceMixer.hpp"
#include "Legacy/Audio/LibSpu.hpp"

namespace VS::Audio {
struct MusicConfig;

// decomp: FSoundVoiceModeFlags (src/include/sound.h:520)
struct VoiceModeMasks {
  std::int32_t reverb;
  std::int32_t noise;
  std::int32_t fm;
};

// decomp: FSoundVoiceParams (src/include/sound.h:316)
struct VoiceParams {
  /* 0x00 */ std::uint32_t assigned_voice;
  /* 0x04 */ std::uint32_t dirty_flags;
  /* 0x08 */ std::uint32_t sample_start;
  /* 0x0C */ std::uint32_t loop_start;
  /* 0x10 */ std::uint16_t sample_rate;
  /* 0x12 */ std::uint16_t adsr_lower;
  /* 0x14 */ std::uint16_t adsr_upper;
  /* 0x16 */ std::uint16_t volume_scale;
  /* 0x18 */ VS::Legacy::SpuVolume volume;
};

// decomp: FSoundVoiceSchedulerState (src/include/sound.h:274)
struct VoiceScheduler {
  std::uint32_t active_voices;
  std::uint32_t pending_key_on;
  std::uint32_t keyed_voices;
  std::uint32_t pending_key_off;
  std::uint32_t paused_effect_voices;
  std::uint32_t effect_tempo;
  std::uint32_t tempo_accumulator;
  std::uint32_t noise_voices;
  std::uint32_t reverb_voices;
  std::uint32_t fm_voices;
  std::uint16_t noise_clock;
  std::uint8_t pad2a;
  std::uint8_t pad2b;
  std::uint32_t reserved2c;
};

// decomp: FSpuVoiceInfo (src/include/sound.h:507)
struct LegacyVoiceEnvelope {
  /* 0x0 */ std::int32_t reserved0;  // Seems unused
  /* 0x4 */ std::int16_t envelope_level;
  /* 0x6 */ std::int16_t reserved6;
};

namespace layout {
inline constexpr std::size_t kVoiceModeMasksBytes = 0xC;
static_assert(sizeof(VoiceModeMasks) == kVoiceModeMasksBytes);
inline constexpr std::size_t kVoiceParamsBytes = 0x1C;
static_assert(sizeof(VoiceParams) == kVoiceParamsBytes);
inline constexpr std::size_t kVoiceSchedulerBytes = 0x30;
static_assert(sizeof(VoiceScheduler) == kVoiceSchedulerBytes);
inline constexpr std::size_t kLegacyVoiceEnvelopeBytes = 0x8;
static_assert(sizeof(LegacyVoiceEnvelope) == kLegacyVoiceEnvelopeBytes);
}  // namespace layout
}  // namespace VS::Audio

namespace VS::Audio::Mixer {
inline constexpr std::size_t kCommandHandlerCount = 0x100;

// kPanGains[0x80] is the decomp's D_8002F89C view.
// decomp: g_Sound_StereoPanGainTableQ15
// (g_Sound_StereoPanGainTableQ15@SLUS_010.40)
extern std::int16_t kPanGains[kPanGainCount];
// decomp: D_800301E0 (D_800301E0@SLUS_010.40)
extern CommandHandler kCommandHandlers[kCommandHandlerCount];
// decomp: g_SpuVoiceInfo (g_SpuVoiceInfo@SLUS_010.40)
extern LegacyVoiceEnvelope voiceEnvelopes[kVoiceCount];
// decomp: D_80036778 (D_80036778@SLUS_010.40)
extern std::int32_t musicPitchScaleStep;
// decomp: D_8003677C (D_8003677C@SLUS_010.40)
extern std::int32_t tempoScaleStep;
// decomp: D_80036780 (D_80036780@SLUS_010.40)
extern std::int16_t musicPitchScaleSlideLength;
// decomp: D_80036782 (D_80036782@SLUS_010.40)
extern std::int16_t tempoScaleSlideLength;
// decomp: D_80037790 (D_80037790@SLUS_010.40)
extern CommandParams commandParams;
// The decomp's views into it: D_80037894 (pending_key_on), D_80037898
// (keyed_voices), D_8003789C (pending_key_off) and D_800378A0
// (paused_effect_voices).
// decomp: g_Sound_VoiceSchedulerState (g_Sound_VoiceSchedulerState@SLUS_010.40)
extern VoiceScheduler voiceScheduler;
// decomp: D_800378E4 (D_800378E4@SLUS_010.40)
extern std::int32_t mutedMusicChannels;
// decomp: g_Sound_MasterPitchScaleQ16_16
// (g_Sound_MasterPitchScaleQ16_16@SLUS_010.40)
extern std::int32_t musicPitchScale;
// Its byte 2 is the decomp's D_80039AF2 view.
// decomp: g_Sound_TempoMultiplier (g_Sound_TempoMultiplier@SLUS_010.40)
extern std::int32_t tempoScale;
// decomp: g_Sound_VoiceChannelConfigs (g_Sound_VoiceChannelConfigs@SLUS_010.40)
extern MusicConfig* voiceOwners[kVoiceCount];
// decomp: g_Sound_VoiceModeFlags (g_Sound_VoiceModeFlags@SLUS_010.40)
extern VoiceModeMasks voiceModes;
}  // namespace VS::Audio::Mixer
