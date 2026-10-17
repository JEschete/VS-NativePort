// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/PlaybackReset.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Envelope.h"
#include "Audio/Seq/Sequencer.h"

// decomp: _soundEvent (_soundEvent@SLUS_010.40)
int Audio_Driver_legacySpuEvent; /* contents: module image (ModuleData.c) */

// decomp: D_8002F548 (D_8002F548@SLUS_010.40)
int Audio_Driver_recentTickCostTotal; /* contents: module image (ModuleData.c) */

// decomp: D_8002F550 (D_8002F550@SLUS_010.40)
int Audio_Driver_recentTickCosts[4]; /* contents: module image (ModuleData.c) */

// decomp: _soundFlush (_soundFlush@SLUS_010.40)
char Audio_Driver_legacySpuFlushBytes[64]; /* contents: module image (ModuleData.c) */

// decomp: _spuMemInfo (_spuMemInfo@SLUS_010.40)
char Audio_Driver_legacySpuAllocatorTable;

// decomp: D_800378C0 (D_800378C0@SLUS_010.40)
Audio_CommandArguments Audio_Driver_commandArguments;

// decomp: g_Sound_GlobalFlags (g_Sound_GlobalFlags@SLUS_010.40)
Audio_DriverFlags Audio_Driver_controlLatches;

// decomp: SLUS_010.40 _spuWriteComplete (src/SLUS_010.40/25AC.c:989)
void Audio_Driver_CompleteSampleTransfer(void)
{
    SpuSetTransferCallback(NULL);
    Audio_Cutscene_legacySpuTransferBusy = 0;
}

// decomp: SLUS_010.40 spuSetTransferCallback (src/SLUS_010.40/25AC.c:995)
void Audio_Driver_ArmSampleTransferCallback(void)
{
    Audio_Cutscene_legacySpuTransferBusy = 1;
    SpuSetTransferCallback(Audio_Driver_CompleteSampleTransfer);
}

// decomp: SLUS_010.40 _writeSpu (src/SLUS_010.40/25AC.c:1001)
void Audio_Driver_WriteSampleMemory(char* data, u_int len)
{
    Audio_Cutscene_legacySpuTransferBusy = 1;
    SpuSetTransferCallback(Audio_Driver_CompleteSampleTransfer);
    SpuWrite(data, len);
}

// decomp: SLUS_010.40 _readSpu (src/SLUS_010.40/25AC.c:1008)
void Audio_Driver_ReadSampleMemory(char* data, u_int len)
{
    Audio_Driver_ArmSampleTransferCallback();
    SpuRead(data, len);
}

// decomp: SLUS_010.40 _waitTransferAvailable (src/SLUS_010.40/25AC.c:1014)
void Audio_Driver_WaitForSampleTransfer(void)
{
    if (Audio_Cutscene_legacySpuTransferBusy == 1) {
        do {

        } while (Audio_Cutscene_legacySpuTransferBusy == 1);
    }
}

// decomp: SLUS_010.40 StartSound (src/SLUS_010.40/25AC.c:1060)
void Audio_Driver_Start(void)
{
    int event;

    SpuStart();
    SpuSetTransferStartAddr(0x1010);
    Audio_Driver_WriteSampleMemory(Audio_Driver_legacySpuFlushBytes, 64);
    Audio_Driver_WaitForSampleTransfer();
    Audio_Driver_ResetPlaybackState();
    while (SetRCnt(RCntCNT2, 0x44E8, RCntMdINTR) == 0)
        ;
    while (StartRCnt(RCntCNT2) == 0)
        ;
    do {
        event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, Audio_Seq_TickPlayback);
        Audio_Driver_legacySpuEvent = event;
    } while (event == -1);
    while (EnableEvent(Audio_Driver_legacySpuEvent) == 0)
        ;
}

// decomp: SLUS_010.40 StopSound (src/SLUS_010.40/25AC.c:1085)
void Audio_Driver_Stop(void)
{
    if (Audio_Cutscene_legacySpuTransferBusy == 1) {
        Audio_Driver_WriteSampleMemory(Audio_Driver_legacySpuFlushBytes, 64);
        Audio_Driver_WaitForSampleTransfer();
    }
    while (StopRCnt(RCntCNT2) == 0)
        ;
    while (DisableEvent(Audio_Driver_legacySpuEvent) == 0)
        ;
    while (CloseEvent(Audio_Driver_legacySpuEvent) == 0)
        ;
    Audio_Driver_KeyOffVoices(0xFFFFFF);
}

// decomp: SLUS_010.40 SetVoiceKeyOn (src/SLUS_010.40/25AC.c:1102)
void Audio_Driver_KeyOnVoices(u_int in_KeyOn)
{
    *SPU_VOICE_KEY_ON_LO = in_KeyOn;
    *SPU_VOICE_KEY_ON_HI = (in_KeyOn >> 0x10);
}

// decomp: SLUS_010.40 SetVoiceKeyOff (src/SLUS_010.40/25AC.c:1108)
void Audio_Driver_KeyOffVoices(u_int in_KeyOff)
{
    *SPU_VOICE_KEY_OFF_LO = in_KeyOff;
    *SPU_VOICE_KEY_OFF_HI = (in_KeyOff >> 0x10);
}

// decomp: SLUS_010.40 SetVoiceReverbMode (src/SLUS_010.40/25AC.c:1114)
void Audio_Driver_SetVoiceReverbMask(u_int in_ReverbMode)
{
    *SPU_VOICE_CHN_REVERB_MODE_LO = in_ReverbMode;
    *SPU_VOICE_CHN_REVERB_MODE_HI = (in_ReverbMode >> 0x10);
}

// decomp: SLUS_010.40 SetVoiceNoiseMode (src/SLUS_010.40/25AC.c:1120)
void Audio_Driver_SetVoiceNoiseMask(u_int in_NoiseMode)
{
    *SPU_VOICE_CHN_NOISE_MODE_LO = in_NoiseMode;
    *SPU_VOICE_CHN_NOISE_MODE_HI = (in_NoiseMode >> 0x10);
}

// decomp: SLUS_010.40 SetVoiceFmMode (src/SLUS_010.40/25AC.c:1126)
void Audio_Driver_SetVoiceFrequencyModulationMask(u_int in_FmMode)
{
    *SPU_VOICE_CHN_FM_MODE_LO = in_FmMode;
    *SPU_VOICE_CHN_FM_MODE_HI = (in_FmMode >> 0x10);
}

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceVolume (src/SLUS_010.40/25AC.c:1132)
void Audio_Driver_SetVoiceVolume(int in_VoiceIndex, u_int in_VolL, u_int in_VolR, u_int in_VolumeScale)
{
    SpuVolume* pVolume;

    if (in_VolumeScale != 0) {
        in_VolL = in_VolL * in_VolumeScale;
        in_VolR = in_VolR * in_VolumeScale;
        in_VolL = in_VolL >> 7;
        in_VolR = in_VolR >> 7;
    }

    pVolume = (SpuVolume*)&VOICE_00_LEFT_RIGHT[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    pVolume->left = in_VolL & 0x7FFF;
    pVolume->right = in_VolR & 0x7FFF;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceSampleRate (src/SLUS_010.40/25AC.c:1148)
void Audio_Driver_SetVoiceSampleRate(int in_VoiceIndex, int in_SampleRate)
{
    VOICE_00_ADPCM_SAMPLE_RATE[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE] = in_SampleRate;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceStartAddr (src/SLUS_010.40/25AC.c:1153)
void Audio_Driver_SetVoiceStartAddress(int in_VoiceIndex, u_int in_Addr)
{
    VOICE_00_ADPCM_START_ADDR[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE] = (in_Addr >> 3);
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceRepeatAddr (src/SLUS_010.40/25AC.c:1158)
void Audio_Driver_SetVoiceLoopAddress(int in_VoiceIndex, u_int in_Addr)
{
    VOICE_00_ADPCM_REPEAT_ADDR[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE] = (in_Addr >> 3);
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceAdsrLower (src/SLUS_010.40/25AC.c:1163)
void Audio_Driver_SetVoiceEnvelopeLower(int in_VoiceIndex, u_short in_Register)
{
    VOICE_00_ADPCM_ADSR_LOWER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE] = in_Register;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceAdsrUpper (src/SLUS_010.40/25AC.c:1168)
void Audio_Driver_SetVoiceEnvelopeUpper(int in_VoiceIndex, u_short in_Register)
{
    VOICE_00_ADPCM_ADSR_UPPER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE] = in_Register;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: src/SLUS_010.40/25AC.c:1178
#define ADSR_ATTACK_MODE_BIT 15
// decomp: src/SLUS_010.40/25AC.c:1180
#define ADSR_ATTACK_STEP_POS 8 // 2 bits (9-8)
// decomp: src/SLUS_010.40/25AC.c:1202
#define ADSR_ATTACK_MODE(mode) ((mode) << ADSR_ATTACK_MODE_BIT)
// decomp: SLUS_010.40 SetVoiceAdsrAttackRateAndMode (src/SLUS_010.40/25AC.c:1262)
void Audio_Driver_SetVoiceAttackRateAndMode(
    int in_VoiceIndex, int in_AttackStep, u_int in_AttackMode)
{
    u_short* AdsrLower =
        &VOICE_00_ADPCM_ADSR_LOWER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    // Extract Attack Mode bit (bit 2 of in_AttackRate -> bit 15 of ADSR)
    u_short AttackMode = ADSR_ATTACK_MODE(in_AttackMode >> 2);
    // Position Attack Step (0-3) at bits 8-9
    u_short AttackStep = in_AttackStep << ADSR_ATTACK_STEP_POS;
    *AdsrLower = AttackMode | AttackStep | *(char*)AdsrLower;
}
#undef ADSR_ATTACK_MODE
#undef ADSR_ATTACK_STEP_POS
#undef ADSR_ATTACK_MODE_BIT
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceAdsrDecayRate (src/SLUS_010.40/25AC.c:1274)
void Audio_Driver_SetVoiceDecayRate(int in_VoiceIndex, int in_DecayRate)
{
    u_short* AdsrLower =
        &VOICE_00_ADPCM_ADSR_LOWER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    u_short AttackStep = in_DecayRate * 0x10;
    u_short Masked = (*AdsrLower & 0xFF0F);
    *AdsrLower = Masked | AttackStep;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceAdsrSustainLevel (src/SLUS_010.40/25AC.c:1283)
void Audio_Driver_SetVoiceSustainLevel(int in_VoiceIndex, int in_SustainLevel)
{
    u_short* AdsrLower =
        &VOICE_00_ADPCM_ADSR_LOWER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    *AdsrLower = (*AdsrLower & 0xFFF0) | in_SustainLevel;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: src/SLUS_010.40/25AC.c:1214
#define ADSR_SUSTAIN_DIRECTION_BIT 14
// decomp: src/SLUS_010.40/25AC.c:1217
#define ADSR_SUSTAIN_STEP_POS 6 // 2 bits (7-6)
// decomp: src/SLUS_010.40/25AC.c:1256
#define ADSR_SUSTAIN_DIRECTION(dir) ((dir) << ADSR_SUSTAIN_DIRECTION_BIT)
// decomp: SLUS_010.40 SetVoiceAdsrSustainRateAndDirection (src/SLUS_010.40/25AC.c:1290)
void Audio_Driver_SetVoiceSustainRateAndMode(
    int in_VoiceIndex, int in_SustainRate, u_int in_SustainDirection)
{
    u_short* AdsrUpper =
        &VOICE_00_ADPCM_ADSR_UPPER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    u_short SustainDirection = ADSR_SUSTAIN_DIRECTION(in_SustainDirection >> 1);
    u_short SustainRate = in_SustainRate << ADSR_SUSTAIN_STEP_POS;
    u_short Masked = *AdsrUpper & 0x3F;
    *AdsrUpper = Masked | (SustainDirection | SustainRate);
}
#undef ADSR_SUSTAIN_DIRECTION
#undef ADSR_SUSTAIN_STEP_POS
#undef ADSR_SUSTAIN_DIRECTION_BIT
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: src/SLUS_010.40/25AC.c:1218
#define ADSR_RELEASE_MODE_BIT 5
// decomp: src/SLUS_010.40/25AC.c:1219
#define ADSR_RELEASE_SHIFT_POS 0 // 5 bits (4-0)
// decomp: SLUS_010.40 SetVoiceAdsrReleaseRateAndMode (src/SLUS_010.40/25AC.c:1301)
void Audio_Driver_SetVoiceReleaseRateAndMode(
    int in_VoiceIndex, int in_ReleaseRate, u_int in_ReleaseMode)
{
    u_short* AdsrUpper =
        &VOICE_00_ADPCM_ADSR_UPPER[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    u_short ReleaseMode = (in_ReleaseMode >> 2) << ADSR_RELEASE_MODE_BIT;
    u_short ReleaseRate = in_ReleaseRate << ADSR_RELEASE_SHIFT_POS;
    u_short Masked = *AdsrUpper & 0xFFC0;
    *AdsrUpper = Masked | (ReleaseMode | ReleaseRate);
}
#undef ADSR_RELEASE_SHIFT_POS
#undef ADSR_RELEASE_MODE_BIT
#undef SPU_VOICE_INDEX_STRIDE

// decomp: src/SLUS_010.40/25AC.c:14
#define SPU_VOICE_INDEX_STRIDE                                                           \
    8 // u16s to skip per voice when indexing voice register arrays
// decomp: SLUS_010.40 SetVoiceParams (src/SLUS_010.40/25AC.c:1312)
void Audio_Driver_WriteVoiceParameters(
    int in_VoiceIndex, Audio_VoiceParams* in_VoiceParams, int in_VolumeScale)
{
    int left;
    int right;
    short* p;

    in_VoiceParams->dirtyFlags = 0;
    p = (short*)&VOICE_00_LEFT_RIGHT[in_VoiceIndex * SPU_VOICE_INDEX_STRIDE];
    if (in_VolumeScale == 0) {
        left = in_VoiceParams->volume.left;
        right = in_VoiceParams->volume.right;
    } else {
        left = in_VoiceParams->volume.left * in_VolumeScale;
        right = in_VoiceParams->volume.right * in_VolumeScale;
        left >>= 7;
        right >>= 7;
    }

    // This is the dumbest shit, but I can't find any other way that compiles correctly
    *p++ = left & 0x7FFF;
    *p++ = right & 0x7FFF;
    *p++ = in_VoiceParams->sampleRate;
    *p++ = in_VoiceParams->sampleStart >> 3;
    *p++ = in_VoiceParams->adsrLower;
    *p++ = in_VoiceParams->adsrUpper;
    p++;
    *p = in_VoiceParams->loopStart >> 3;
}
#undef SPU_VOICE_INDEX_STRIDE

// decomp: SLUS_010.40 SetVoiceParamsByFlags (src/SLUS_010.40/25AC.c:1342)
void Audio_Driver_WriteDirtyVoiceParameters(u_int in_VoiceIndex, Audio_VoiceParams* in_VoiceParams,
    int arg2 __attribute__((unused)))
{
    int flags;

    flags = in_VoiceParams->dirtyFlags;
    if (flags == 0) {
        return;
    }

    in_VoiceParams->dirtyFlags = 0;

    if (flags & VOICE_PARAM_SAMPLE_RATE) {
        flags &= ~VOICE_PARAM_SAMPLE_RATE;
        Audio_Driver_SetVoiceSampleRate(in_VoiceIndex, in_VoiceParams->sampleRate);
        if (flags == 0) {
            return;
        }
    }

    if (flags & VOICE_PARAM_VOLUME) {
        flags &= ~VOICE_PARAM_VOLUME;
        Audio_Driver_SetVoiceVolume(in_VoiceIndex, in_VoiceParams->volume.left,
            in_VoiceParams->volume.right, in_VoiceParams->volumeScale);
        if (flags == 0) {
            return;
        }
    }

    if (flags & VOICE_PARAM_START_ADDR) {
        flags &= ~VOICE_PARAM_START_ADDR;
        Audio_Driver_SetVoiceStartAddress(in_VoiceIndex, in_VoiceParams->sampleStart);
        if (flags == 0) {
            return;
        }
    }

    if (flags & VOICE_PARAM_LOOP_ADDR) {
        flags &= ~VOICE_PARAM_LOOP_ADDR;
        Audio_Driver_SetVoiceLoopAddress(in_VoiceIndex, in_VoiceParams->loopStart);
        if (flags == 0) {
            return;
        }
    }

    if (flags & VOICE_PARAM_ADSR_UPPER) {
        flags &= ~VOICE_PARAM_ADSR_UPPER;
        Audio_Driver_SetVoiceEnvelopeUpper(in_VoiceIndex, in_VoiceParams->adsrUpper);
        if (flags == 0) {
            return;
        }
    }

    if (flags & VOICE_PARAM_ADSR_LOWER) {
        Audio_Driver_SetVoiceEnvelopeLower(in_VoiceIndex, in_VoiceParams->adsrLower);
    }
}

