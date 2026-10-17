// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_MIXER_VOICEMIXER_H_TYPES
#define VS_AUDIO_MIXER_VOICEMIXER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_VoiceEnvelope Audio_VoiceEnvelope;
typedef struct Audio_VoiceModeMasks Audio_VoiceModeMasks;
typedef struct Audio_VoiceParams Audio_VoiceParams;
typedef struct Audio_VoiceScheduler Audio_VoiceScheduler;

// decomp: D_8002F89C (D_8002F89C@SLUS_010.40): a view into another global
#define Audio_Mixer_kCenterPanGain (*(short*)((u_char*)&Audio_Mixer_kPanGains + 256))

// decomp: D_80037894 (D_80037894@SLUS_010.40): a view into another global
#define Audio_Mixer_pendingKeyOnVoices (*(int*)(&Audio_Mixer_voiceScheduler.pendingKeyOn))

// decomp: D_80037898 (D_80037898@SLUS_010.40): a view into another global
#define Audio_Mixer_keyedVoices (*(int*)(&Audio_Mixer_voiceScheduler.keyedVoices))

// decomp: D_8003789C (D_8003789C@SLUS_010.40): a view into another global
#define Audio_Mixer_pendingKeyOffVoices (*(int*)(&Audio_Mixer_voiceScheduler.pendingKeyOff))

// decomp: D_800378A0 (D_800378A0@SLUS_010.40): a view into another global
#define Audio_Mixer_pausedSoundEffectVoices (*(int*)(&Audio_Mixer_voiceScheduler.pausedEffectVoices))

// decomp: D_80039AF2 (D_80039AF2@SLUS_010.40): a view into another global
#define Audio_Mixer_tempoScaleLevel (*(char*)((u_char*)&Audio_Mixer_tempoScale + 2))

// decomp: src/include/sound.h:7
#define VOICE_COUNT 24

// decomp: src/include/sound.h:10
#define SOUND_CHANNEL_COUNT 0x20

// decomp: src/include/sound.h:66
#define SOUND_UPDATE_VIBRATO (1 << 0)

// decomp: src/include/sound.h:67
#define SOUND_UPDATE_TREMOLO (1 << 1)

// decomp: src/include/sound.h:68
#define SOUND_UPDATE_PAN_LFO (1 << 2)

// decomp: src/include/sound.h:70
#define SOUND_UPDATE_SIDE_CHAIN_PITCH (1 << 4)

// decomp: src/include/sound.h:71
#define SOUND_UPDATE_SIDE_CHAIN_VOL (1 << 5)

// decomp: src/include/sound.h:87
#define SOUND_GLOBAL_UPDATE_04 (1 << 4)

// decomp: src/include/sound.h:91
#define SOUND_GLOBAL_UPDATE_08 (1 << 8)

// decomp: FSoundVoiceModeFlags (src/include/sound.h:520)
typedef struct Audio_VoiceModeMasks {
    int reverb;
    int noise;
    int fm;
} Audio_VoiceModeMasks;
_Static_assert(sizeof(Audio_VoiceModeMasks) == 0xC, "Audio_VoiceModeMasks must keep its PS1 layout");

// decomp: FSoundVoiceParams (src/include/sound.h:316)
typedef struct Audio_VoiceParams {
    /* 0x00 */ u_int assignedVoice;
    /* 0x04 */ u_int dirtyFlags;
    /* 0x08 */ u_int sampleStart;
    /* 0x0C */ u_int loopStart;
    /* 0x10 */ u_short sampleRate;
    /* 0x12 */ u_short adsrLower;
    /* 0x14 */ u_short adsrUpper;
    /* 0x16 */ u_short volumeScale;
    /* 0x18 */ SpuVolume volume;
} Audio_VoiceParams;
_Static_assert(sizeof(Audio_VoiceParams) == 0x1C, "Audio_VoiceParams must keep its PS1 layout");

// decomp: FSoundVoiceSchedulerState (src/include/sound.h:274)
typedef struct Audio_VoiceScheduler {
    u_int activeVoices;
    u_int pendingKeyOn;
    u_int keyedVoices;
    u_int pendingKeyOff;
    u_int pausedEffectVoices;
    u_int effectTempo;
    u_int tempoAccumulator;
    u_int noiseVoices;
    u_int reverbVoices;
    u_int fmVoices;
    u_short noiseClock;
    char pad2A;
    char pad2B;
    u_int reserved2C;
} Audio_VoiceScheduler;
_Static_assert(sizeof(Audio_VoiceScheduler) == 0x30, "Audio_VoiceScheduler must keep its PS1 layout");

// decomp: FSpuVoiceInfo (src/include/sound.h:507)
typedef struct Audio_VoiceEnvelope {
    /* 0x0 */ int reserved0; // Seems unused
    /* 0x4 */ short envelopeLevel;
    /* 0x6 */ short reserved6;
} Audio_VoiceEnvelope;
_Static_assert(sizeof(Audio_VoiceEnvelope) == 0x8, "Audio_VoiceEnvelope must keep its PS1 layout");

#endif // VS_AUDIO_MIXER_VOICEMIXER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_MIXER_VOICEMIXER_H_DECLS)
#define VS_AUDIO_MIXER_VOICEMIXER_H_DECLS
#include "Audio/Mixer/MixControl.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

// decomp: src/SLUS_010.40/25AC.c:173
#define SPU_PAN_TABLE_SIZE (0x100)
extern short Audio_Mixer_kPanGains[SPU_PAN_TABLE_SIZE];
extern Audio_CommandHandler Audio_Mixer_kCommandHandlers[392];
extern Audio_VoiceEnvelope Audio_Mixer_voiceEnvelopes[VOICE_COUNT];
extern int Audio_Mixer_musicPitchScaleStep;
extern int Audio_Mixer_tempoScaleStep;
extern short Audio_Mixer_musicPitchScaleSlideLength;
extern short Audio_Mixer_tempoScaleSlideLength;
extern Audio_CommandParams Audio_Mixer_commandParams;
extern Audio_VoiceScheduler Audio_Mixer_voiceScheduler;
extern int Audio_Mixer_mutedMusicChannels;
extern int Audio_Mixer_musicPitchScale;
extern int Audio_Mixer_tempoScale;
extern Audio_MusicConfig* Audio_Mixer_voiceOwners[VOICE_COUNT];
extern Audio_VoiceModeMasks Audio_Mixer_voiceModes;
void Audio_Mixer_UpdateChannelSlidesAndDelays(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags, int in_Arg2);
void Audio_Mixer_UpdateMusicChannelModulation(Audio_SeqChannel* in_pChannel, int in_VoiceFlags __attribute__((unused)));
void Audio_Mixer_UpdateSoundEffectModulation(Audio_SeqChannel* in_pChannel, int in_VoiceFlags __attribute__((unused)));
int Audio_Mixer_StealQuietestVoice(int in_bForceFullScan);
int Audio_Mixer_FindFreeVoice(int arg0);
void Audio_Mixer_ApplyMusicChannelVoices(
    Audio_SeqChannel* in_pChannel, u_int in_Flags1, u_int in_Flags2, u_int* out_KeyOnFlags);
void Audio_Mixer_UnassignVoice(Audio_SeqChannel* in_pChannel, int arg1);
int Audio_Mixer_IsVoiceAssignedToChannels(Audio_SeqChannel* channels, int mask, int voiceNumber);
void Audio_Mixer_RefreshVoiceAvailability(u_int in_ReservedVoices);
void Audio_Mixer_ApplySavedMusicMasterFade(
    Audio_MusicConfig* in_Context, Audio_SeqChannel* arg1 __attribute__((unused)));
void Audio_Mixer_RestoreSavedMusicVolume(
    Audio_MusicConfig* in_Config, Audio_SeqChannel* arg1 __attribute__((unused)));
void Audio_Mixer_CommitPlaybackVoices(void);
void Audio_Mixer_AccumulateFilteredVoiceMask(Audio_SeqChannel* in_Channel, int* io_VoiceMask,
    int in_ChannelMask, int in_VoiceMaskFilter);
void Audio_Mixer_ReleasePendingVoices(void);
void Audio_Mixer_BuildVoiceModeMask(int* out_VoiceMask, int in_SavedChannelMask, int in_ActiveChannelMask,
    int in_ExtraVoiceMask);

#endif // VS_AUDIO_MIXER_VOICEMIXER_H_DECLS
