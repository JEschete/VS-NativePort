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
    int playback_flags = Audio_Seq_activeMusicConfig->activeChannels != 0;
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (Audio_Seq_secondaryMusicConfig->activeChannels != 0) {
            playback_flags |= 2;
        }
    }
    if (Audio_Seq_pendingMusicData.param1 != 0) {
        playback_flags |= 0x10;
    }
    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        playback_flags |= 0x100;
    }
    if (Audio_Cutscene_streamVoices != 0) {
        playback_flags |= 0x10000;
    }
    return playback_flags;
}

// decomp: SLUS_010.40 func_80011EBC (src/SLUS_010.40/25AC.c:243)
int Audio_Playback_MusicVolume(u_int music_id)
{
    Audio_MusicConfig* music_config = Audio_Seq_activeMusicConfig;
    int volume = 0;

    if ((music_config->musicId == music_id)
        || ((music_config = Audio_Seq_secondaryMusicConfig, (music_config != NULL))
            && (music_config->musicId == music_id))) {
        volume = (music_config->masterVolume >> 0x10);
    }
    return volume;
}

// decomp: SLUS_010.40 func_80011F08 (src/SLUS_010.40/25AC.c:256)
int Audio_Playback_PackedMusicSampleBankIds(void)
{
    return (u_short)Audio_Samples_musicSampleBankIds.bankId | ((u_short)Audio_Samples_musicSampleBankIds.alternateBankId << 0x10);
}

// decomp: SLUS_010.40 func_80011F24 (src/SLUS_010.40/25AC.c:261)
int Audio_Playback_LongestMusicFade(int fade_selector_mask)
{
    int longest_fade_ticks = 0;
    if (fade_selector_mask & 1) {
        longest_fade_ticks = Audio_Seq_activeMusicConfig->masterVolumeSlideLength;
    }
    if (fade_selector_mask & 2) {
        if (longest_fade_ticks < Audio_Mixer_tempoScaleSlideLength) {
            longest_fade_ticks = Audio_Mixer_tempoScaleSlideLength;
        }
    }
    if ((fade_selector_mask & 4) && (longest_fade_ticks < Audio_Mixer_musicPitchScaleSlideLength)) {
        longest_fade_ticks = Audio_Mixer_musicPitchScaleSlideLength;
    }
    return longest_fade_ticks;
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
u_int Audio_Playback_StartMusic(void* sequence_data)
{
    Audio_Driver_commandArguments.param1 = sequence_data;
    return Audio_Mixer_DispatchCommand(0x10);
}

// decomp: SLUS_010.40 func_80011FDC (src/SLUS_010.40/25AC.c:292)
void Audio_Playback_StopMusic(u_int music_id)
{
    Audio_Driver_commandArguments.param1 = (void*)music_id;
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
u_int Audio_Playback_StartLayeredMusic(void* sequence_data, int volume)
{
    u_int music_id;

    Audio_Driver_commandArguments.param1 = sequence_data;
    music_id = Audio_Mixer_DispatchCommand(0x19);
    Audio_Control_SetMusicVolume(music_id, volume);
    return music_id;
}

// decomp: SLUS_010.40 func_80012080 (src/SLUS_010.40/25AC.c:318)
u_int Audio_Playback_CrossfadeMusic(void* sequence_data, u_int fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = sequence_data;
    Audio_Driver_commandArguments.param2 = fade_ticks;
    Audio_Driver_commandArguments.param3 = target_volume & 0x7F;
    return Audio_Mixer_DispatchCommand(0x1A);
}

// decomp: SLUS_010.40 func_800120B8 (src/SLUS_010.40/25AC.c:326)
void Audio_Playback_StartMusicAtLoop(void* sequence_data, int loop_count)
{
    Audio_Driver_commandArguments.param1 = sequence_data;
    Audio_Driver_commandArguments.param2 = loop_count;
    Audio_Mixer_DispatchCommand(0x12);
}

// decomp: SLUS_010.40 func_800120E8 (src/SLUS_010.40/25AC.c:333)
u_int Audio_Playback_QueueMusicTransition(void* sequence_data)
{
    Audio_Driver_commandArguments.param1 = sequence_data;
    return Audio_Mixer_DispatchCommand(0x14);
}

// decomp: SLUS_010.40 func_80012110 (src/SLUS_010.40/25AC.c:339)
void* Audio_Playback_StartMusicPair(void* primary_sequence, int secondary_sequence_address)
{
    uintptr_t packed_music_ids;
    Audio_Driver_commandArguments.param1 = primary_sequence;
    Audio_Driver_commandArguments.param2 = secondary_sequence_address;
    Audio_Mixer_DispatchCommand(0x16);
    packed_music_ids = Audio_Driver_commandArguments.param2 << 0x10;
    packed_music_ids += (uintptr_t)Audio_Driver_commandArguments.param1;
    return (void*)packed_music_ids;
}

// decomp: SLUS_010.40 func_8001215C (src/SLUS_010.40/25AC.c:350)
void Audio_Playback_RequestMusicLayerSwap(void) { Audio_Mixer_DispatchCommand(0x17); }

// decomp: SLUS_010.40 func_8001217C (src/SLUS_010.40/25AC.c:352)
int Audio_Playback_PlaySoundEffect(u_long effect_id, int effect_tag, int pan, int volume)
{
    int program_offset = Audio_Playback_soundEffectProgramOffsets[effect_id & 0x3FF];
    if (program_offset != -1) {
        Audio_Driver_commandArguments.param1 = (void*)(effect_id & 0x3FF);
        Audio_Driver_commandArguments.param2 = effect_tag & 0xFFFFFF;
        Audio_Driver_commandArguments.param3 = pan & 0xFF;
        Audio_Driver_commandArguments.param4 = volume & 0x7F;
        Audio_Mixer_DispatchCommand(0x20);
    }
    return program_offset;
}

// decomp: SLUS_010.40 func_800121F0 (src/SLUS_010.40/25AC.c:365)
uintptr_t Audio_Playback_PlaySoundEffectBlob(void* program_blob, int effect_tag, int pan, int volume)
{
    void* program_or_failure;

    program_or_failure = (void*)-1;
    if (Audio_Samples_SequenceMagicDifference(program_blob) == 0) {
        Audio_Driver_commandArguments.param1 = program_blob;
        Audio_Driver_commandArguments.param2 = (int)(effect_tag & 0xFFFFFF);
        Audio_Driver_commandArguments.param3 = (int)(pan & 0xFF);
        Audio_Driver_commandArguments.param4 = (int)(volume & 0x7F);
        Audio_Mixer_DispatchCommand(0x24);
        program_or_failure = program_blob;
    }
    return (uintptr_t)program_or_failure;
}

// decomp: SLUS_010.40 func_80012288 (src/SLUS_010.40/25AC.c:381)
void Audio_Playback_StopSoundEffects(intptr_t effect_selector, int effect_tag_mask)
{
    Audio_Driver_commandArguments.param1 = (void*)effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Mixer_DispatchCommand(0x21);
}

// decomp: SLUS_010.40 func_800122C4 (src/SLUS_010.40/25AC.c:388)
void Audio_Playback_PlayInterfaceSound(u_long effect_id)
{
    Audio_Driver_commandArguments.param1 = (void*)(effect_id & 0x3FF);
    Audio_Mixer_DispatchCommand(0x30);
}

// decomp: SLUS_010.40 func_800122F0 (src/SLUS_010.40/25AC.c:394)
int Audio_Playback_ActiveSoundEffectTags(void)
{
    Audio_SeqChannel* channel;
    int empty_or_effect_tags;
    int voice_bit;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;

    empty_or_effect_tags = mask == 0;
    if (empty_or_effect_tags) {
        return 0;
    }

    channel = Audio_SoundEffects_channels;
    empty_or_effect_tags = 0;
    voice_bit = 0x1000;

    do {
        if (mask & voice_bit) {
            empty_or_effect_tags |= channel->effectTags;
        }
        voice_bit *= 2;
        ++channel;
    } while (voice_bit & 0xFFFFFF);

    return empty_or_effect_tags & 0xFFFFFF;
}

// decomp: SLUS_010.40 func_80012358 (src/SLUS_010.40/25AC.c:421)
intptr_t Audio_Playback_SoundEffectIdForTag(int effect_tag)
{
    Audio_SeqChannel* channel;
    int voice_bit;
    int active_voice_mask;
    if ((effect_tag == 0) || ((active_voice_mask = Audio_Mixer_voiceScheduler.activeVoices) == 0)) {
        return 0;
    }
    for (channel = Audio_SoundEffects_programPairChannels - 10, voice_bit = 0x1000; voice_bit & 0xFFFFFF;
        voice_bit *= 2, ++channel) {
        if ((active_voice_mask & voice_bit) && (effect_tag == channel->effectTags)) {
            return channel->effectId;
        }
    }

    return 0;
}

// decomp: SLUS_010.40 func_800123C8 (src/SLUS_010.40/25AC.c:439)
int Audio_Playback_UsesSoundEffectTags(Audio_SfxProgramHeader* program_or_effect_id)
{
    int ret = 0;
    if ((intptr_t)program_or_effect_id >= 0x80000000) {
        ret = program_or_effect_id->flags >> 7;
    } else if (Audio_Playback_soundEffectBankSelectors[(long)program_or_effect_id] & 0x8000) {
        ret = 1;
    }
    return ret;
}

// decomp: SLUS_010.40 vs_sound_setMonoSound (src/SLUS_010.40/25AC.c:450)
void Audio_Playback_SetMonoOutput(int mono_enabled)
{
    if (mono_enabled == 1) {
        Audio_Mixer_DispatchCommand(0x81);
    } else {
        Audio_Mixer_DispatchCommand(0x80);
    }
}

