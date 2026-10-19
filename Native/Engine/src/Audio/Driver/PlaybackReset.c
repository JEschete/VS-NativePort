// Hand-maintained (tools/port/owned.csv).
// The sound driver's reset, written from its assembly: the decomp keeps
// func_8001369C as INCLUDE_ASM
// (build/src/SLUS_010.40/nonmatchings/25AC/func_8001369C.s, 0x29C bytes). Each
// store is written against the named field the PS1 offset lands on
// (FIELD_LEDGER via the type layouts), so it holds on x64, where these structs
// hold pointers and their offsets differ. The comment on a store is its PS1
// offset.
#include "Audio/Driver/PlaybackReset.h"

#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Legacy/Psx.h"

// SPU registers the reset writes (LibSpu.h register block; offsets from
// 0x1F801C00)
#define SPU_MAIN_VOLUME_LEFT \
  ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x180))
#define SPU_MAIN_VOLUME_RIGHT \
  ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x182))
#define SPU_CONTROL ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x1AA))

// decomp: SLUS_010.40 func_8001369C (src/SLUS_010.40/25AC.c:1058)
void Audio_Driver_ResetPlaybackState(void) {
  Audio_SeqChannel* channel;
  u_short spu_control;
  int i;

  Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
  Audio_Seq_secondaryMusicConfig = NULL;
  // the original also stores &musicChannels in D_800377FC, which nothing reads
  Audio_Seq_secondaryMusicChannels = NULL;
  Audio_Seq_randomPitchPhase = 0;
  Audio_Seq_pendingMusicData.param1 = 0;
  Audio_Driver_controlLatches.controlLatches = 0;
  Audio_Samples_musicSampleBankIds.alternateBankId = 0;
  Audio_Samples_musicSampleBankIds.bankId = 0;
  Audio_Samples_soundEffectBankIds[2] = 0;
  Audio_Samples_soundEffectBankIds[1] = 0;
  Audio_Driver_controlLatches.mixBehavior = 1;
  Audio_Samples_soundEffectBankIds[0] = 0;
  Audio_Mixer_voiceScheduler.activeVoices = 0;
  Audio_Seq_primaryMusicConfig.activeChannels = 0;       // +0x04
  Audio_Seq_primaryMusicConfig.keyedVoices = 0;          // +0x08
  Audio_Seq_primaryMusicConfig.musicId = 0;              // +0x6E
  Audio_Mixer_voiceScheduler.pausedEffectVoices = 0;     // +0x10
  Audio_Seq_primaryMusicConfig.pausedChannels = 0;       // +0x1C
  Audio_Seq_savedMusicConfig.musicId = 0;                // +0x6E
  Audio_Seq_savedMusicConfig.activeChannels = 0;         // +0x04
  Audio_Seq_primaryMusicConfig.masterVolume = 0x7F0000;  // +0x60
  Audio_Seq_discVolume = 0x7FFF0000;
  Audio_Mixer_musicPitchScaleSlideLength = 0;
  Audio_Mixer_musicPitchScale = 0;
  Audio_Mixer_tempoScaleSlideLength = 0;
  Audio_Mixer_tempoScale = 0;
  Audio_Seq_primaryMusicConfig.masterVolumeSlideLength = 0;  // +0x68
  Audio_Seq_discVolumeFadeLength = 0;
  Audio_Mixer_voiceScheduler.noiseVoices = 0;       // +0x1C
  Audio_Seq_primaryMusicConfig.noiseChannels = 0;   // +0x4C
  Audio_Mixer_voiceScheduler.reverbVoices = 0;      // +0x20
  Audio_Seq_primaryMusicConfig.reverbChannels = 0;  // +0x50
  Audio_Mixer_voiceScheduler.fmVoices = 0;          // +0x24

  spu_control = *SPU_CONTROL;
  *SPU_MAIN_VOLUME_LEFT = 0x3FFF;
  *SPU_MAIN_VOLUME_RIGHT = 0x3FFF;
  *CD_VOL_L = 0x7FFF;
  *CD_VOL_R = 0x7FFF;

  Audio_Seq_musicLoopsRemaining = 0;
  Audio_Mixer_mutedMusicChannels = 0;
  Audio_Cutscene_streamControlFlags = 0;
  Audio_Seq_primaryMusicConfig.fmChannels = 0;         // +0x54
  Audio_Seq_primaryMusicConfig.timerLower = 0;         // +0x78
  Audio_Seq_primaryMusicConfig.timerUpperCount = 0;    // +0x76
  Audio_Seq_primaryMusicConfig.timerUpper = 0;         // +0x74
  Audio_Seq_primaryMusicConfig.timerTop = 0;           // +0x7C
  Audio_Cutscene_streamState.volume = 0x7F00;          // +0x40
  Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;  // +0x48
  Audio_Mixer_voiceModes.fm = 0;
  Audio_Mixer_voiceModes.noise = 0;
  Audio_Mixer_voiceModes.reverb = 0;
  // SPU control: CD audio on (bit 0), CD audio reverb off (bit 2)
  enum { kCdAudioResetMask = 0xFFFA, kCdAudioEnable = 1 };
  *SPU_CONTROL = (spu_control & kCdAudioResetMask) | kCdAudioEnable;

  // the 32 music channels: no voice assigned (24 = none)
  for (i = 0, channel = Audio_Seq_musicChannels; i < 32; ++i, ++channel) {
    channel->updateFlags = 0;           // +0x34
    channel->voice.assignedVoice = 24;  // +0xF4
    channel->type = 0;                  // +0x64
    channel->priority = 0;              // +0x58
  }
  // the 12 sound effect channels own SPU voices 12-23
  for (i = 12, channel = Audio_SoundEffects_channels; i < 24; ++i, ++channel) {
    channel->updateFlags = 0;                  // +0x34
    channel->voice.assignedVoice = i;          // +0xF4
    channel->type = 1;                         // +0x64
    channel->priority = 0;                     // +0x58
    channel->effectVolume = 0x7F00;            // +0xDC
    channel->effectVolumeSlideLength = 0;      // +0x8C
    channel->effectPitchScaleSlideLength = 0;  // +0x86
    channel->effectPitchScale = 0;             // +0x40
    channel->keyOnVolumeSlideLength = 0;       // +0x8A
  }

  Audio_Seq_activeMusicConfig->pendingKeyOff = 0;           // +0x18
  Audio_Seq_activeMusicConfig->activeNotes = 0;             // +0x14
  Audio_Seq_activeMusicConfig->pendingKeyOn = 0;            // +0x10
  Audio_Mixer_voiceScheduler.tempoAccumulator = 1;          // +0x18
  Audio_Mixer_voiceScheduler.effectTempo = 0x66A80000;      // +0x14
  Audio_Mixer_voiceScheduler.pendingKeyOff = 0;             // +0x0C
  Audio_Mixer_voiceScheduler.keyedVoices = 0;               // +0x08
  Audio_Mixer_voiceScheduler.pendingKeyOn = 0;              // +0x04
  Audio_Seq_activeMusicConfig->reverbDepth = 0x3FFF000;     // +0x58
  Audio_Seq_activeMusicConfig->reverbDepthSlideStep = 0;    // +0x5C
  Audio_Seq_activeMusicConfig->reverbDepthSlideLength = 0;  // +0x6A
  Audio_Driver_controlLatches.updateFlags |= 0x80;
  Audio_Mixer_SetReverbMode(4);
  SpuSetReverb(1);
}
