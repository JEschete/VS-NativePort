// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Akao.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"

// decomp: g_Sound_SfxProgramOffsets (g_Sound_SfxProgramOffsets@SLUS_010.40)
u_int* Audio_Playback_soundEffectProgramOffsets;

// decomp: g_Sound_SfxMetadataTable (g_Sound_SfxMetadataTable@SLUS_010.40)
u_short* Audio_Playback_soundEffectBankSelectors;

// decomp: g_Sound_SfxProgramData (g_Sound_SfxProgramData@SLUS_010.40)
char* Audio_Playback_soundEffectProgramData;

// decomp: SLUS_010.40 InitSound (src/SLUS_010.40/25AC.c:195)
int Audio_Playback_Initialize(void)
{
    Audio_Driver_Start();
    return 0;
}

// decomp: SLUS_010.40 TeardownSound (src/SLUS_010.40/25AC.c:201)
int Audio_Playback_Shutdown(void)
{
    Audio_Driver_Stop();
    return 0;
}

// decomp: SLUS_010.40 Sound_BindAkaoSfxBlob (src/SLUS_010.40/25AC.c:207)
int Audio_Playback_BindSoundEffectPrograms(void* in_Blob)
{
    int notAkao = Audio_Samples_SequenceMagicDifference(in_Blob);

    in_Blob += sizeof(Audio_AkaoFileHeader);

    if (notAkao == 0) {
        Audio_Playback_soundEffectProgramOffsets = in_Blob;
        in_Blob += 0x600;
        Audio_Playback_soundEffectBankSelectors = in_Blob;
        in_Blob += 0x300;
        Audio_Playback_soundEffectProgramData = in_Blob;
    }
    return notAkao;
}

// decomp: SLUS_010.40 func_80011E40 (src/SLUS_010.40/25AC.c:223)
int Audio_Playback_ActivePlaybackFlags(void)
{
    int var_v1 = Audio_Seq_activeMusicConfig->activeChannels != 0;
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (Audio_Seq_secondaryMusicConfig->activeChannels != 0) {
            var_v1 |= 2;
        }
    }
    if (Audio_Seq_pendingMusicData.param1 != 0) {
        var_v1 |= 0x10;
    }
    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        var_v1 |= 0x100;
    }
    if (Audio_Cutscene_streamVoices != 0) {
        var_v1 |= 0x10000;
    }
    return var_v1;
}

// decomp: SLUS_010.40 func_80011EBC (src/SLUS_010.40/25AC.c:243)
int Audio_Playback_MusicVolume(u_int arg0)
{
    Audio_MusicConfig* var_v1 = Audio_Seq_activeMusicConfig;
    int var_a1 = 0;

    if ((var_v1->musicId == arg0)
        || ((var_v1 = Audio_Seq_secondaryMusicConfig, (var_v1 != NULL))
            && (var_v1->musicId == arg0))) {
        var_a1 = (var_v1->masterVolume >> 0x10);
    }
    return var_a1;
}

// decomp: SLUS_010.40 func_80011F08 (src/SLUS_010.40/25AC.c:256)
int Audio_Playback_PackedMusicSampleBankIds(void)
{
    return (u_short)Audio_Samples_musicSampleBankIds.bankId | ((u_short)Audio_Samples_musicSampleBankIds.alternateBankId << 0x10);
}

// decomp: SLUS_010.40 func_80011F24 (src/SLUS_010.40/25AC.c:261)
int Audio_Playback_LongestMusicFade(int arg0)
{
    int var_a1 = 0;
    if (arg0 & 1) {
        var_a1 = Audio_Seq_activeMusicConfig->masterVolumeSlideLength;
    }
    if (arg0 & 2) {
        if (var_a1 < Audio_Mixer_tempoScaleSlideLength) {
            var_a1 = Audio_Mixer_tempoScaleSlideLength;
        }
    }
    if ((arg0 & 4) && (var_a1 < Audio_Mixer_musicPitchScaleSlideLength)) {
        var_a1 = Audio_Mixer_musicPitchScaleSlideLength;
    }
    return var_a1;
}

// decomp: SLUS_010.40 func_80011F90 (src/SLUS_010.40/25AC.c:278)
int Audio_Playback_MusicPosition(void)
{
    int upper = Audio_Seq_activeMusicConfig->timerUpperCount;
    int top = Audio_Seq_activeMusicConfig->timerTop;
    top = (top << 16) | (upper + 1);
    return top;
}

// decomp: SLUS_010.40 func_80011FB4 (src/SLUS_010.40/25AC.c:286)
u_int Audio_Playback_StartMusic(void* arg0)
{
    Audio_Driver_commandArguments.param1 = arg0;
    return Audio_Mixer_DispatchCommand(0x10);
}

// decomp: SLUS_010.40 func_80011FDC (src/SLUS_010.40/25AC.c:292)
void Audio_Playback_StopMusic(u_int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Mixer_DispatchCommand(0x11);
}

// decomp: SLUS_010.40 func_80012004 (src/SLUS_010.40/25AC.c:298)
void Audio_Playback_SaveMusicState(void) { Audio_Mixer_DispatchCommand(0x40); }

// decomp: SLUS_010.40 func_80012024 (src/SLUS_010.40/25AC.c:300)
int Audio_Playback_InvalidateSavedMusicId(void)
{
    Audio_Seq_savedMusicId = 0;
    return 0;
}

// decomp: SLUS_010.40 func_80012034 (src/SLUS_010.40/25AC.c:308)
u_int Audio_Playback_StartLayeredMusic(void* arg0, int arg1)
{
    u_int temp_v0;

    Audio_Driver_commandArguments.param1 = arg0;
    temp_v0 = Audio_Mixer_DispatchCommand(0x19);
    Audio_Control_SetMusicVolume(temp_v0, arg1);
    return temp_v0;
}

// decomp: SLUS_010.40 func_80012080 (src/SLUS_010.40/25AC.c:318)
u_int Audio_Playback_CrossfadeMusic(void* arg0, u_int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Driver_commandArguments.param3 = arg2 & 0x7F;
    return Audio_Mixer_DispatchCommand(0x1A);
}

// decomp: SLUS_010.40 func_800120B8 (src/SLUS_010.40/25AC.c:326)
void Audio_Playback_StartMusicAtLoop(void* arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Mixer_DispatchCommand(0x12);
}

// decomp: SLUS_010.40 func_800120E8 (src/SLUS_010.40/25AC.c:333)
u_int Audio_Playback_QueueMusicTransition(void* arg0)
{
    Audio_Driver_commandArguments.param1 = arg0;
    return Audio_Mixer_DispatchCommand(0x14);
}

// decomp: SLUS_010.40 func_80012110 (src/SLUS_010.40/25AC.c:339)
void* Audio_Playback_StartMusicPair(void* arg0, int arg1)
{
    uintptr_t v0;
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Mixer_DispatchCommand(0x16);
    v0 = Audio_Driver_commandArguments.param2 << 0x10;
    v0 += (uintptr_t)Audio_Driver_commandArguments.param1;
    return (void*)v0;
}

// decomp: SLUS_010.40 func_8001215C (src/SLUS_010.40/25AC.c:350)
void Audio_Playback_RequestMusicLayerSwap(void) { Audio_Mixer_DispatchCommand(0x17); }

// decomp: SLUS_010.40 func_8001217C (src/SLUS_010.40/25AC.c:352)
int Audio_Playback_PlaySoundEffect(u_long arg0, int arg1, int arg2, int arg3)
{
    int temp_s0 = Audio_Playback_soundEffectProgramOffsets[arg0 & 0x3FF];
    if (temp_s0 != -1) {
        Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0x3FF);
        Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
        Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
        Audio_Driver_commandArguments.param4 = arg3 & 0x7F;
        Audio_Mixer_DispatchCommand(0x20);
    }
    return temp_s0;
}

// decomp: SLUS_010.40 func_800121F0 (src/SLUS_010.40/25AC.c:365)
uintptr_t Audio_Playback_PlaySoundEffectBlob(void* arg0, int arg1, int arg2, int arg3)
{
    void* var_v0;

    var_v0 = (void*)-1;
    if (Audio_Samples_SequenceMagicDifference(arg0) == 0) {
        Audio_Driver_commandArguments.param1 = arg0;
        Audio_Driver_commandArguments.param2 = (int)(arg1 & 0xFFFFFF);
        Audio_Driver_commandArguments.param3 = (int)(arg2 & 0xFF);
        Audio_Driver_commandArguments.param4 = (int)(arg3 & 0x7F);
        Audio_Mixer_DispatchCommand(0x24);
        var_v0 = arg0;
    }
    return (uintptr_t)var_v0;
}

// decomp: SLUS_010.40 func_80012288 (src/SLUS_010.40/25AC.c:381)
void Audio_Playback_StopSoundEffects(intptr_t arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Mixer_DispatchCommand(0x21);
}

// decomp: SLUS_010.40 func_800122C4 (src/SLUS_010.40/25AC.c:388)
void Audio_Playback_PlayInterfaceSound(u_long arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0x3FF);
    Audio_Mixer_DispatchCommand(0x30);
}

// decomp: SLUS_010.40 func_800122F0 (src/SLUS_010.40/25AC.c:394)
int Audio_Playback_ActiveSoundEffectTags(void)
{
    Audio_SeqChannel* var_a1;
    int var_a0;
    int var_v1;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;

    var_a0 = mask == 0;
    if (var_a0) {
        return 0;
    }

    var_a1 = Audio_SoundEffects_channels;
    var_a0 = 0;
    var_v1 = 0x1000;

    do {
        if (mask & var_v1) {
            var_a0 |= var_a1->effectTags;
        }
        var_v1 *= 2;
        ++var_a1;
    } while (var_v1 & 0xFFFFFF);

    return var_a0 & 0xFFFFFF;
}

// decomp: SLUS_010.40 func_80012358 (src/SLUS_010.40/25AC.c:421)
int Audio_Playback_SoundEffectIdForTag(int arg0)
{
    Audio_SeqChannel* var_a1;
    int var_v1;
    int a2;
    if ((arg0 == 0) || ((a2 = Audio_Mixer_voiceScheduler.activeVoices) == 0)) {
        return 0;
    }
    for (var_a1 = Audio_SoundEffects_programPairChannels - 10, var_v1 = 0x1000; var_v1 & 0xFFFFFF;
        var_v1 *= 2, ++var_a1) {
        if ((a2 & var_v1) && (arg0 == var_a1->effectTags)) {
            return var_a1->effectId;
        }
    }

    return 0;
}

// decomp: SLUS_010.40 func_800123C8 (src/SLUS_010.40/25AC.c:439)
int Audio_Playback_UsesSoundEffectTags(Audio_SfxProgramHeader* arg0)
{
    int ret = 0;
    if ((intptr_t)arg0 >= 0x80000000) {
        ret = arg0->flags >> 7;
    } else if (Audio_Playback_soundEffectBankSelectors[(long)arg0] & 0x8000) {
        ret = 1;
    }
    return ret;
}

// decomp: SLUS_010.40 vs_sound_setMonoSound (src/SLUS_010.40/25AC.c:450)
void Audio_Playback_SetMonoOutput(int arg0)
{
    if (arg0 == 1) {
        Audio_Mixer_DispatchCommand(0x81);
    } else {
        Audio_Mixer_DispatchCommand(0x80);
    }
}

