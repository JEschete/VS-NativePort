// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_DRIVER_SOUNDDRIVER_H_TYPES
#define VS_AUDIO_DRIVER_SOUNDDRIVER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_CommandArguments Audio_CommandArguments;
typedef struct Audio_DriverFlags Audio_DriverFlags;

// decomp: D_80039AFC (D_80039AFC@SLUS_010.40): a view into another global
#define Audio_Driver_mixBehavior (*(int*)(&Audio_Driver_controlLatches.mixBehavior))

// decomp: D_80039B00 (D_80039B00@SLUS_010.40): a view into another global
#define Audio_Driver_pendingUpdates (*(int*)(&Audio_Driver_controlLatches.updateFlags))

// decomp: src/include/hw.h:7
#define VOICE_00_LEFT_RIGHT ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x000))

// decomp: src/include/hw.h:8
#define VOICE_00_ADPCM_SAMPLE_RATE ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x004))

// decomp: src/include/hw.h:9
#define VOICE_00_ADPCM_START_ADDR ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x006))

// decomp: src/include/hw.h:10
#define VOICE_00_ADPCM_ADSR_LOWER ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x008))

// decomp: src/include/hw.h:11
#define VOICE_00_ADPCM_ADSR_UPPER ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x00A))

// decomp: src/include/hw.h:12
#define VOICE_00_ADPCM_REPEAT_ADDR ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x00E))

// decomp: src/include/hw.h:13
#define SPU_VOICE_KEY_ON_LO ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x188))

// decomp: src/include/hw.h:14
#define SPU_VOICE_KEY_ON_HI ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x18A))

// decomp: src/include/hw.h:15
#define SPU_VOICE_KEY_OFF_LO ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x18C))

// decomp: src/include/hw.h:16
#define SPU_VOICE_KEY_OFF_HI ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x18E))

// decomp: src/include/hw.h:17
#define SPU_VOICE_CHN_FM_MODE_LO ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x190))

// decomp: src/include/hw.h:18
#define SPU_VOICE_CHN_FM_MODE_HI ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x192))

// decomp: src/include/hw.h:19
#define SPU_VOICE_CHN_NOISE_MODE_LO ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x194))

// decomp: src/include/hw.h:20
#define SPU_VOICE_CHN_NOISE_MODE_HI ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x196))

// decomp: src/include/hw.h:21
#define SPU_VOICE_CHN_REVERB_MODE_LO ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x198))

// decomp: src/include/hw.h:22
#define SPU_VOICE_CHN_REVERB_MODE_HI ((u_short*)((uintptr_t)LegacySpu_Registers() + 0x19A))

// decomp: src/include/sound.h:14
#define VOICE_PARAM_VOLUME_L (1 << 0) /* volume (left) */

// decomp: src/include/sound.h:15
#define VOICE_PARAM_VOLUME_R (1 << 1) /* volume (right) */

// decomp: src/include/sound.h:16
#define VOICE_PARAM_VOLUME (VOICE_PARAM_VOLUME_L | VOICE_PARAM_VOLUME_R)

// decomp: src/include/sound.h:19
#define VOICE_PARAM_SAMPLE_RATE (1 << 4) /* tone (pitch setting)  */

// decomp: src/include/sound.h:22
#define VOICE_PARAM_START_ADDR (1 << 7) /* waveform data start address */

// decomp: src/include/sound.h:31
#define VOICE_PARAM_ADSR_LOWER                                                           \
    (VOICE_PARAM_ADSR_AMODE | VOICE_PARAM_ADSR_AR | VOICE_PARAM_ADSR_DR                  \
        | VOICE_PARAM_ADSR_SL)

// decomp: src/include/sound.h:34
#define VOICE_PARAM_ADSR_UPPER                                                           \
    (VOICE_PARAM_ADSR_SMODE | VOICE_PARAM_ADSR_RMODE | VOICE_PARAM_ADSR_SR               \
        | VOICE_PARAM_ADSR_RR)

// decomp: src/include/sound.h:38
#define VOICE_PARAM_LOOP_ADDR (1 << 16) /* start address for loop */

// decomp: D_800378C0_t (src/SLUS_010.40/25AC.c:65)
typedef struct Audio_CommandArguments {
    void* param1;
    int param2;
    int param3;
    int param4;
    int extParam1;
    int extParam2;
} Audio_CommandArguments;

// decomp: FSoundGlobalFlags (src/include/sound.h:203)
typedef struct Audio_DriverFlags {
    u_int controlLatches; // one-shot / transactional engine state flags
    u_int mixBehavior; // global mixing & music-stack behavior flags
    u_int updateFlags; // deferred SPU / voice-mode update flags
} Audio_DriverFlags;
_Static_assert(sizeof(Audio_DriverFlags) == 0xC, "Audio_DriverFlags must keep its PS1 layout");

#endif // VS_AUDIO_DRIVER_SOUNDDRIVER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_DRIVER_SOUNDDRIVER_H_DECLS)
#define VS_AUDIO_DRIVER_SOUNDDRIVER_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"

extern int Audio_Driver_legacySpuEvent;
extern int Audio_Driver_recentTickCostTotal;
extern int Audio_Driver_recentTickCosts[4];
extern char Audio_Driver_legacySpuFlushBytes[64];
extern char Audio_Driver_legacySpuAllocatorTable;
extern Audio_CommandArguments Audio_Driver_commandArguments;
extern Audio_DriverFlags Audio_Driver_controlLatches;
void Audio_Driver_CompleteSampleTransfer(void);
void Audio_Driver_ArmSampleTransferCallback(void);
void Audio_Driver_WriteSampleMemory(char* data, u_int len);
void Audio_Driver_ReadSampleMemory(char* data, u_int len);
void Audio_Driver_WaitForSampleTransfer(void);
void Audio_Driver_Start(void);
void Audio_Driver_Stop(void);
void Audio_Driver_KeyOnVoices(u_int in_KeyOn);
void Audio_Driver_KeyOffVoices(u_int in_KeyOff);
void Audio_Driver_SetVoiceReverbMask(u_int in_ReverbMode);
void Audio_Driver_SetVoiceNoiseMask(u_int in_NoiseMode);
void Audio_Driver_SetVoiceFrequencyModulationMask(u_int in_FmMode);
void Audio_Driver_SetVoiceVolume(int in_VoiceIndex, u_int in_VolL, u_int in_VolR, u_int in_VolumeScale);
void Audio_Driver_SetVoiceSampleRate(int in_VoiceIndex, int in_SampleRate);
void Audio_Driver_SetVoiceStartAddress(int in_VoiceIndex, u_int in_Addr);
void Audio_Driver_SetVoiceLoopAddress(int in_VoiceIndex, u_int in_Addr);
void Audio_Driver_SetVoiceEnvelopeLower(int in_VoiceIndex, u_short in_Register);
void Audio_Driver_SetVoiceEnvelopeUpper(int in_VoiceIndex, u_short in_Register);
void Audio_Driver_SetVoiceAttackRateAndMode(
    int in_VoiceIndex, int in_AttackStep, u_int in_AttackMode);
void Audio_Driver_SetVoiceDecayRate(int in_VoiceIndex, int in_DecayRate);
void Audio_Driver_SetVoiceSustainLevel(int in_VoiceIndex, int in_SustainLevel);
void Audio_Driver_SetVoiceSustainRateAndMode(
    int in_VoiceIndex, int in_SustainRate, u_int in_SustainDirection);
void Audio_Driver_SetVoiceReleaseRateAndMode(
    int in_VoiceIndex, int in_ReleaseRate, u_int in_ReleaseMode);
void Audio_Driver_WriteVoiceParameters(
    int in_VoiceIndex, Audio_VoiceParams* in_VoiceParams, int in_VolumeScale);
void Audio_Driver_WriteDirtyVoiceParameters(u_int in_VoiceIndex, Audio_VoiceParams* in_VoiceParams,
    int arg2 __attribute__((unused)));

#endif // VS_AUDIO_DRIVER_SOUNDDRIVER_H_DECLS
