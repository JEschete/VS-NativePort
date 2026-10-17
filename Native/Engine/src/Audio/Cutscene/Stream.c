// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"

// decomp: _isSpuTransfer (_isSpuTransfer@SLUS_010.40)
volatile int Audio_Cutscene_legacySpuTransferBusy;

// decomp: g_Sound_Cutscene_StreamState (g_Sound_Cutscene_StreamState@SLUS_010.40)
Audio_CutsceneStreamState Audio_Cutscene_streamState;

// decomp: D_80039B14 (D_80039B14@SLUS_010.40)
int Audio_Cutscene_streamVoices;

// decomp: D_80039B48 (D_80039B48@SLUS_010.40)
int Audio_Cutscene_streamVolume;

// decomp: D_80039B64 (D_80039B64@SLUS_010.40)
int Audio_Cutscene_streamControlFlags;

// decomp: SLUS_010.40 Sound_Cmd_9F_unk (src/SLUS_010.40/25AC.c:3786)
void Audio_Cutscene_PauseStream(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Cutscene_streamState.voiceMask != 0) {
        Audio_Driver_SetVoiceSampleRate(Audio_Cutscene_streamState.voiceIndex, 0);
        Audio_Driver_SetVoiceSampleRate(Audio_Cutscene_streamState.voiceIndex + 1, 0);
    }
}

// decomp: SLUS_010.40 Sound_Cmd_9E_unk (src/SLUS_010.40/25AC.c:3794)
void Audio_Cutscene_ResumeStream(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Cutscene_streamState.voiceMask != 0) {
        Audio_Driver_SetVoiceSampleRate(Audio_Cutscene_streamState.voiceIndex,
            Audio_Cutscene_streamState.sampleRate);
        Audio_Driver_SetVoiceSampleRate(Audio_Cutscene_streamState.voiceIndex + 1,
            Audio_Cutscene_streamState.sampleRate);
    }
}

// decomp: SLUS_010.40 IRQCallbackProc (src/SLUS_010.40/25AC.c:6238)
void Audio_Cutscene_StopStreamVoices(void)
{
    int temp_a2;

    if (Audio_Cutscene_streamState.voiceMask != 0) {
        Audio_Driver_KeyOffVoices(Audio_Cutscene_streamState.voiceMask);
        temp_a2 = Audio_Cutscene_streamState.voiceMask;
        Audio_Cutscene_streamState.voiceMask = 0;
        Audio_Mixer_voiceScheduler.reverbVoices &= ~temp_a2;
        Audio_Driver_controlLatches.updateFlags |= 0x100;
    }
}

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
    u_char* temp_s0 = Audio_Cutscene_streamState.currentChunk + 0x800;
    SpuSetTransferStartAddr(0x2100);
    SpuSetTransferCallback(Audio_Cutscene_BeginMonoPlayback);
    SpuWrite(temp_s0, 0x800);
}

// decomp: SLUS_010.40 Sound_Cutscene_StartStream (src/SLUS_010.40/25AC.c:6294)
void Audio_Cutscene_StartStream(u_int* arg0, int arg1, int arg2)
{
    int VoiceIndex;
    u_int transferSize;

    VoiceIndex = Audio_Cutscene_FindFreeVoicePair();
    if (VoiceIndex != -1) {
        u_int* s3;
        Audio_Cutscene_streamState.pan.word = arg1;
        s3 = arg0;
        Audio_Cutscene_streamState.currentChunk = s3 + 0x10;
        Audio_Cutscene_streamState.totalPages = s3[4];
        Audio_Cutscene_streamState.currentPage = s3[8];
        Audio_Cutscene_streamState.voiceIndex = VoiceIndex;
        Audio_Cutscene_streamState.voiceMask =
            (1 << VoiceIndex) | (1 << (VoiceIndex + 1));
        Audio_Driver_KeyOffVoices(Audio_Cutscene_streamState.voiceMask);
        Audio_Cutscene_streamState.formatFlags = s3[6];
        Audio_Cutscene_streamState.sampleRate = (u_short)s3[7];
        Audio_Cutscene_streamState.voiceIndex = VoiceIndex;
        Audio_Cutscene_streamState.voiceMask =
            (1 << VoiceIndex) | (1 << (VoiceIndex + 1));
        Audio_Driver_KeyOffVoices(Audio_Cutscene_streamState.voiceMask);
        SpuSetTransferStartAddr(0x1100U);
        Audio_Cutscene_legacySpuTransferBusy = 1;

        Audio_Cutscene_streamState.auxiliaryMask =
            (Audio_Cutscene_streamState.formatFlags & 2)
                ? Audio_Cutscene_streamState.currentChunk + s3[5]
                : NULL;
        if (Audio_Cutscene_streamState.formatFlags & 1) {
            Audio_Cutscene_streamState.reserved1C =
                (Audio_Cutscene_streamState.formatFlags & 2)
                    ? Audio_Cutscene_streamState.totalPages - (s3[5] >> 1)
                    : 0;
            SpuSetTransferCallback(Audio_Cutscene_BeginStereoPlayback);
            transferSize = 0x2000;
        } else {
            Audio_Cutscene_streamState.reserved1C =
                (Audio_Cutscene_streamState.formatFlags & 2)
                    ? Audio_Cutscene_streamState.totalPages - s3[5]
                    : 0;
            SpuSetTransferCallback(Audio_Cutscene_UploadSecondMonoBuffer);
            transferSize = 0x800;
        }
        SpuWrite((u_char*)Audio_Cutscene_streamState.currentChunk, transferSize);
        if (arg2 != 0) {
            Audio_Mixer_voiceScheduler.reverbVoices |=
                Audio_Cutscene_streamState.voiceMask;
        } else {
            Audio_Mixer_voiceScheduler.reverbVoices &=
                ~Audio_Cutscene_streamState.voiceMask;
        }
        Audio_Mixer_voiceScheduler.fmVoices &=
            ~Audio_Cutscene_streamState.voiceMask;
        Audio_Mixer_voiceScheduler.noiseVoices &=
            ~Audio_Cutscene_streamState.voiceMask;
        Audio_Driver_controlLatches.updateFlags |= 0x100;
    }
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
    int var_s2 = in_SampleAddr;
    if (Audio_Cutscene_streamState.voiceMask != 0) {
        SpuSetTransferCallback(NULL);
        Audio_Cutscene_legacySpuTransferBusy = 0;
        if (Audio_Cutscene_streamState.totalPages >= 0x1001) {
            Audio_Cutscene_streamState.totalPages -= 0x1000;
            Audio_Cutscene_streamState.currentChunk += in_SomeIndex;
        } else {
            var_s2 = 0x1030;
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

// decomp: SLUS_010.40 func_8001D644 (src/SLUS_010.40/25AC.c:6503)
void Audio_Cutscene_ReplaceStream(int* arg0)
{
    Audio_Cutscene_StopStreamVoices();
    Audio_Cutscene_StartStream((u_int*)arg0[0], arg0[1], arg0[2]);
    Audio_Mixer_voiceScheduler.activeVoices &=
        ~Audio_Cutscene_streamState.voiceMask;
}

// decomp: SLUS_010.40 func_8001D698 (src/SLUS_010.40/25AC.c:6511)
void Audio_Cutscene_StopStream(void) { Audio_Cutscene_StopStreamVoices(); }

// decomp: SLUS_010.40 Sound_Cmd_E4_SetCutsceneVolume (src/SLUS_010.40/25AC.c:6513)
void Audio_Cutscene_SetStreamVolume(Audio_CommandParams* in_Params)
{
    Audio_Cutscene_streamState.volume = in_Params->param1;
    Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;
    if (Audio_Cutscene_streamState.voiceMask != 0) {
        Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex,
            ((Audio_Cutscene_streamState.volume << 15) >> 16), 0, 0);
        Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex + 1, 0,
            ((Audio_Cutscene_streamState.volume << 15) >> 16), 0);
    }
}

// decomp: SLUS_010.40 Sound_Cmd_E5_FadeOutCutscene (src/SLUS_010.40/25AC.c:6525)
void Audio_Cutscene_SlideStreamVolume(Audio_CommandParams* in_Params)
{
    short var_a1 = 1;
    if (in_Params->param1 != 0) {
        var_a1 = in_Params->param1;
    }
    Audio_Cutscene_streamState.volumeFadeStep =
        (short)((short)(in_Params->param2 - Audio_Cutscene_streamState.volume)
                / var_a1);
    Audio_Cutscene_streamState.volumeFadeTicksLeft = var_a1;
}

// decomp: SLUS_010.40 func_8001D7A8 (src/SLUS_010.40/25AC.c:6537)
void Audio_Cutscene_SetStreamPan(int* arg0)
{
    Audio_Cutscene_streamState.pan.word = *arg0;

    if (Audio_Cutscene_streamState.voiceMask != 0) {
        if (Audio_Driver_mixBehavior & 2) {
            int var_s0 = (Audio_Cutscene_streamState.volume * Audio_Mixer_kCenterPanGain) >> 0x10;
            Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex, var_s0, var_s0, 0);
            Audio_Driver_SetVoiceVolume(
                Audio_Cutscene_streamState.voiceIndex + 1, var_s0, var_s0, 0);
        } else if (Audio_Cutscene_streamState.formatFlags & 1) {
            int var_s0 = Audio_Cutscene_streamState.volume;
            var_s0 <<= 0xF;
            var_s0 >>= 0x10;
            Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex, var_s0, 0, 0);
            Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex + 1, 0, var_s0, 0);
        } else {
            int var_s0;
            int v1 = (Audio_Cutscene_streamState.pan.word >> 8) & 0xFF;
            short temp_s1 = Audio_Mixer_kPanGains[v1];
            temp_s1 = (Audio_Cutscene_streamState.volume * temp_s1) >> 0x10;
            var_s0 =
                (Audio_Cutscene_streamState.volume
                    * *(((((Audio_Cutscene_streamState.pan.word >> 8) & 0xFF)
                            ^ 0xFF))
                        + Audio_Mixer_kPanGains))
                >> 0x10;
            Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex, temp_s1, var_s0, 0);
            Audio_Driver_SetVoiceVolume(
                Audio_Cutscene_streamState.voiceIndex + 1, temp_s1, var_s0, 0);
        }
    }
}

