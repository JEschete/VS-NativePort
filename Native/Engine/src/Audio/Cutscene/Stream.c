// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"

// decomp: _isSpuTransfer (_isSpuTransfer@SLUS_010.40)
volatile int Audio_Cutscene_legacySpuTransferBusy;

// decomp: g_Sound_Cutscene_StreamState (g_Sound_Cutscene_StreamState@SLUS_010.40)
Audio_CutsceneStreamState Audio_Cutscene_streamState;

// decomp: SLUS_010.40 Sound_Cutscene_FindFreeVoicePair (src/SLUS_010.40/25AC.c:6254)
int Audio_Cutscene_FindFreeVoicePair(void)
{
    u_int BusyMask;
    int VoiceIndex;
    u_int Bit;

    do {
        Bit = 0x00C00000;
        VoiceIndex = 12;
        BusyMask = Audio_Mixer_voiceScheduler.activeVoices
                 | Audio_Mixer_voiceScheduler.pausedEffectVoices;

        while (VoiceIndex != 0 && (BusyMask & Bit)) {
            VoiceIndex -= 2;
            Bit >>= 2;
        }

        if (VoiceIndex != 0) {
            return VoiceIndex + 10;
        }

        // No free pair: evict the lowest priority SFX voice and retry
        Audio_SoundEffects_ReleaseMatchingVoices(0, 0x40000000);
    } while (BusyMask
             != (Audio_Mixer_voiceScheduler.activeVoices
                 | Audio_Mixer_voiceScheduler.pausedEffectVoices));

    return -1;
}

// decomp: SLUS_010.40 func_8001CE60 (src/SLUS_010.40/25AC.c:6284)
void Audio_Cutscene_UploadSecondMonoBuffer(void)
{
    u_char* second_buffer_data = Audio_Cutscene_streamState.currentChunk + 0x800;
    SpuSetTransferStartAddr(0x2100);
    SpuSetTransferCallback(Audio_Cutscene_BeginMonoPlayback);
    SpuWrite(second_buffer_data, 0x800);
}

// decomp: src/SLUS_010.40/25AC.c:173
#define SPU_PAN_TABLE_SIZE (0x100)
// decomp: src/SLUS_010.40/25AC.c:174
#define PAN_CENTER_INDEX (SPU_PAN_TABLE_SIZE / 2)
// decomp: SLUS_010.40 Sound_Cutscene_InitVoice (src/SLUS_010.40/25AC.c:6358)
void Audio_Cutscene_InitializeStreamVoice(
    int in_Voice, int in_PanMode, int in_StartAddr, int in_RepeatAddr)
{
    short VolR;
    short VolL;

    if (Audio_Driver_mixBehavior & 2) {
        int volume = (Audio_Cutscene_streamState.volume
                         * Audio_Mixer_kPanGains[PAN_CENTER_INDEX])
                  >> 0x10;
        VolR = volume;
        VolL = volume;
    } else {
        if (in_PanMode == 1) {
            VolR = 0;
            VolL = Audio_Cutscene_streamState.volume >> 1;
        } else if (in_PanMode == 2) {
            VolL = 0;
            VolR = Audio_Cutscene_streamState.volume >> 1;
        } else if (in_PanMode == 3) {
            int VolHalfQ16 = (Audio_Cutscene_streamState.volume >> 1) << 0x10;
            VolR = (VolHalfQ16 >> 0x11) + (VolHalfQ16 >> 0x12);
            VolL = VolR;
        } else {
            char Mask = 0xFF;
            VolL = ((Audio_Cutscene_streamState.volume
                        * Audio_Mixer_kPanGains[Audio_Cutscene_streamState
                                .pan.bytes[1]])
                    >> 0x10);
            VolR = ((Audio_Cutscene_streamState.volume
                        * Audio_Mixer_kPanGains
                            [Audio_Cutscene_streamState.pan.bytes[1] ^ Mask])
                    >> 0x10);
        }
    }
    Audio_Driver_SetVoiceVolume(in_Voice, VolL, VolR, 0);
    Audio_Driver_SetVoiceSampleRate(in_Voice, Audio_Cutscene_streamState.sampleRate);
    Audio_Driver_SetVoiceStartAddress(in_Voice, in_StartAddr);
    Audio_Driver_SetVoiceLoopAddress(in_Voice, in_RepeatAddr);
    Audio_Driver_SetVoiceAttackRateAndMode(in_Voice, 0, 1);
    Audio_Driver_SetVoiceDecayRate(in_Voice, 0xF);
    Audio_Driver_SetVoiceSustainLevel(in_Voice, 0xF);
    Audio_Driver_SetVoiceSustainRateAndMode(in_Voice, 0x7F, 3);
    Audio_Driver_SetVoiceReleaseRateAndMode(in_Voice, 6, 3);
}
#undef PAN_CENTER_INDEX
#undef SPU_PAN_TABLE_SIZE

// decomp: SLUS_010.40 Sound_Cutscene_BeginPlayback (src/SLUS_010.40/25AC.c:6404)
void Audio_Cutscene_BeginStreamPlayback(
    int in_SomeIndex, int in_SampleAddr, void (*in_Callback)())
{
    int irq_sample_address = in_SampleAddr;
    if (Audio_Cutscene_streamState.voiceMask != 0) {
        SpuSetTransferCallback(NULL);
        Audio_Cutscene_legacySpuTransferBusy = 0;
        if (Audio_Cutscene_streamState.totalPages >= 0x1001) {
            Audio_Cutscene_streamState.totalPages -= 0x1000;
            Audio_Cutscene_streamState.currentChunk += in_SomeIndex;
        } else {
            irq_sample_address = 0x1030;
            Audio_Cutscene_streamState.totalPages = 0;
        }
        Audio_Driver_KeyOnVoices(Audio_Cutscene_streamState.voiceMask);
    }
}

// decomp: SLUS_010.40 Sound_Cutscene_OnInitialTransferComplete (src/SLUS_010.40/25AC.c:6426)
void Audio_Cutscene_BeginMonoPlayback(void)
{
    Audio_Cutscene_InitializeStreamVoice(Audio_Cutscene_streamState.voiceIndex, 0, 0x1100, 0x2100);
    Audio_Cutscene_InitializeStreamVoice(
        Audio_Cutscene_streamState.voiceIndex + 1, 0, 0x1100, 0x2100);
    Audio_Cutscene_BeginStreamPlayback(0x1000, 0x2100, Audio_Cutscene_RefillMonoFirstBuffer);
}

// decomp: SLUS_010.40 func_8001D3D4 (src/SLUS_010.40/25AC.c:6436)
void Audio_Cutscene_BeginStereoPlayback(void)
{
    Audio_Cutscene_InitializeStreamVoice(Audio_Cutscene_streamState.voiceIndex, 1, 0x1100, 0x2100);
    Audio_Cutscene_InitializeStreamVoice(
        Audio_Cutscene_streamState.voiceIndex + 1, 2, 0x1900, 0x2900);
    Audio_Cutscene_BeginStreamPlayback(0x2000, 0x2100, Audio_Cutscene_RefillStereoFirstBuffer);
}

// decomp: SLUS_010.40 Sound_Cutscene_LoadNextBuffer (src/SLUS_010.40/25AC.c:6444)
void Audio_Cutscene_RefillStreamBuffer(
    int in_RepeatAddressL, int in_RepeatAddressR, int in_Param3, void (*in_IrqCallback)())
{
    if ((Audio_Cutscene_streamState.voiceMask != 0)
        && (Audio_Cutscene_streamState.totalPages != 0)) {
        SpuSetTransferStartAddr(in_RepeatAddressL);
        Audio_Driver_ArmSampleTransferCallback();
        SpuWrite((u_char*)Audio_Cutscene_streamState.currentChunk, in_Param3);
        if (Audio_Cutscene_streamState.totalPages > 0x800) {
            Audio_Cutscene_streamState.totalPages -= 0x800;
            Audio_Cutscene_streamState.currentChunk += in_Param3;
        } else if (Audio_Cutscene_streamState.auxiliaryMask != 0) {
            Audio_Cutscene_streamState.currentChunk =
                Audio_Cutscene_streamState.auxiliaryMask;
            Audio_Cutscene_streamState.totalPages =
                Audio_Cutscene_streamState.reserved1C;
        } else {
            in_RepeatAddressR = 0x1030;
            in_RepeatAddressL = 0x1030;
            Audio_Cutscene_streamState.totalPages = 0;
        }
        Audio_Driver_SetVoiceLoopAddress(Audio_Cutscene_streamState.voiceIndex, in_RepeatAddressL);
        Audio_Driver_SetVoiceLoopAddress(
            Audio_Cutscene_streamState.voiceIndex + 1, in_RepeatAddressR);
    }
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferAComplete (src/SLUS_010.40/25AC.c:6477)
void Audio_Cutscene_RefillMonoFirstBuffer(void)
{
    Audio_Cutscene_RefillStreamBuffer(
        0x1100, 0x1100, 0x800, Audio_Cutscene_RefillMonoSecondBuffer);
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferBComplete (src/SLUS_010.40/25AC.c:6483)
void Audio_Cutscene_RefillMonoSecondBuffer(void)
{
    Audio_Cutscene_RefillStreamBuffer(
        0x2100, 0x2100, 0x800, Audio_Cutscene_RefillMonoFirstBuffer);
}

// decomp: SLUS_010.40 func_8001D5E4 (src/SLUS_010.40/25AC.c:6491)
void Audio_Cutscene_RefillStereoFirstBuffer(void)
{
    Audio_Cutscene_RefillStreamBuffer(0x1100, 0x1900, 0x1000, ((void (*)())Audio_Cutscene_RefillStereoSecondBuffer));
}

// decomp: SLUS_010.40 func_8001D614 (src/SLUS_010.40/25AC.c:6496)
void Audio_Cutscene_RefillStereoSecondBuffer(void)
{
    Audio_Cutscene_RefillStreamBuffer(0x2100, 0x2900, 0x1000, Audio_Cutscene_RefillStereoFirstBuffer);
}

