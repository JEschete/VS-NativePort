// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/LegacyFiles.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "State/PlayTime.h"

// decomp: _sfxData (_sfxData@SLUS_010.40)
u_short Audio_menuSoundEffects[11264];

// decomp: vs_main_soundData (vs_main_soundData@SLUS_010.40)
Audio_SoundState Audio_soundState;

// decomp: _soundFileMap (_soundFileMap@SLUS_010.40)
char Audio_waveBankFileMap[] = { 0, 65, 66, 66, 68, 69, 67, 85, 68, 93, 69, 68, 0, 69, 69, 0, 69,
    69, 69, 69, 69, 68, 62, 61, 68, 68, 68, 68, 68, 57, 56, 72, 74, 77, 76, 86, 87, 80,
    88, 55, 70, 81, 70, 73, 75, 78, 89, 90, 79, 84, 10, 10, 10, 10, 11, 11, 11, 11, 12,
    12, 12, 12, 13, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 18, 19, 19,
    19, 19, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 25, 25, 25,
    25, 0, 0, 0, 0, 0, 85, 0, 0, 91, 91, 91, 91, 91, 91, 91, 0, 60, 94, 98, 95, 96, 97,
    97, 82, 83, 0, 0, 59, 58, 72, 74, 77, 76, 86, 87, 80, 88, 0, 0, 0, 0, 0, 0, 78, 89,
    90, 79, 84, 0, 99, 99, 99, 99, 92, 64, 64, 64, 54, 53, 53, 53, 53, 53, 63, 63, 63, 52,
    51, 50, 52, 49, 47, 46, 45, 45, 45, 44, 44, 43, 42, 41, 40, 39, 38, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 };

// decomp: SLUS_010.40 vs_main_loadSfxSlot (src/SLUS_010.40/main.c:10045)
int Audio_LoadSfxBankSlot(int id, u_int slot)
{
    Content_DiscFile cdFile;

    if (slot - 1 < 3) {
        if (Audio_soundState.sfxBankData[slot - 1] == NULL) {
            u_int index = slot - 1;
            cdFile.lba = Audio_legacySfxBankDiscOffsets[id];
            cdFile.size = Audio_legacySfxBankFileSizes[id];

            if ((Audio_soundState.sfxBankRequests[index] != NULL)
                && (Audio_soundState.sfxBankRequests[index] != (Content_DiscQueueSlot*)-1)) {
                Core_IgnoreDiagnostic(0x8F, 0);
            }

            Audio_soundState.sfxBankRequests[index] = Content_AllocateDiscRequest(&cdFile);

            if (Audio_soundState.sfxBankData[index] != NULL) {
                Core_IgnoreDiagnostic(0x90, 0);
            }

            Audio_soundState.sfxBankIds[index] = id;
            Audio_soundState.sfxBankData[index] = Core_Memory_AllocateFromHighEnd(cdFile.size);
            Content_EnqueueDiscRequest(
                Audio_soundState.sfxBankRequests[index], Audio_soundState.sfxBankData[index]);

            return slot;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 _loadSfx (src/SLUS_010.40/main.c:10077)
int Audio_LoadSfxBankIntoFreeSlot(int id)
{
    int i;

    for (i = 0; i < 3; ++i) {
        if (Audio_soundState.sfxBankData[i] == NULL) {
            return Audio_LoadSfxBankSlot(id, i + 1);
        }
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_freeSfxQueueSlot (src/SLUS_010.40/main.c:10089)
int Audio_PollSfxBankSlotLoad(u_int slot)
{
    if ((slot - 1) < 3) {
        if (Audio_soundState.sfxBankRequests[slot - 1] != NULL) {
            if (Audio_soundState.sfxBankRequests[slot - 1]->state
                == Content_DiscRequestState_Loaded) {
                Content_ReleaseDiscRequest(Audio_soundState.sfxBankRequests[slot - 1]);
                Audio_soundState.sfxBankRequests[slot - 1] = NULL;
                return 0;
            }
            if (Audio_soundState.sfxBankRequests[slot - 1]->state
                == Content_DiscRequestState_Error) {
                Content_ReleaseDiscRequest(Audio_soundState.sfxBankRequests[slot - 1]);
                Audio_soundState.sfxBankRequests[slot - 1] = (Content_DiscQueueSlot*)-1;
                return -1;
            }
            return 1;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 _freeSfxSlots (src/SLUS_010.40/main.c:10111)
int Audio_PollSfxBankLoads(void)
{
    int i;
    int ret = 0;
    for (i = 0; i < 3; ++i) {
        ret |= Audio_PollSfxBankSlotLoad(i + 1);
    }
    return ret;
}

// decomp: SLUS_010.40 _loadAndFreeSfx (src/SLUS_010.40/main.c:10121)
int Audio_LoadSfxBankBlocking(int id)
{
    int slot;

    slot = Audio_LoadSfxBankIntoFreeSlot(id);
    if (slot != 0) {
        while (Audio_PollSfxBankLoads() != 0) {
            State_UpdatePlayTimeAndLoading(0);
        }
    }
    return slot;
}

// decomp: SLUS_010.40 vs_main_setCurrentSfx (src/SLUS_010.40/main.c:10134)
int Audio_SelectSfxBankSlot(int id)
{
    if ((id - 1u < 3) && Audio_soundState.sfxBankData[id - 1] != NULL) {
        Audio_soundState.currentSfxBank = id;
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_freeSfx (src/SLUS_010.40/main.c:10143)
int Audio_FreeSfxBankSlot(u_int id)
{
    if ((id - 1) < 3) {
        if (Audio_soundState.sfxBankData[id - 1] != NULL) {
            if (Audio_soundState.currentSfxBank == id) {
                Audio_soundState.currentSfxBank = 0;
            }
            Audio_Playback_StopSoundEffects(-2, 0);
            Audio_Playback_StopSoundEffects(0, 0xFF000);
            Core_Memory_FreeFromHighEnd(Audio_soundState.sfxBankData[id - 1]);
            Audio_soundState.sfxBankIds[id - 1] = 0;
            Audio_soundState.sfxBankData[id - 1] = 0;
            return 1;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 func_80046168 (src/SLUS_010.40/main.c:10161)
int Audio_BindRoomSfxBank(uintptr_t arg0)
{
    if (Audio_soundState.musicHandles[16] == 0) {
        Audio_soundState.musicHandles[16] = arg0;
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80046194 (src/SLUS_010.40/main.c:10170)
int Audio_UnbindRoomSfxBank(void)
{
    Audio_Playback_StopSoundEffects(-2, 0);
    Audio_Playback_StopSoundEffects(0, 0xF00000);
    Audio_soundState.sfxBankData[2] = NULL;
    return 1;
}

// decomp: SLUS_010.40 _loadSoundFile (src/SLUS_010.40/main.c:10359)
void Audio_LoadWaveBankFile(int file)
{
    Content_DiscFile cdFile;
    int new_var;
    int temp_v1;
    int var_a0;

    temp_v1 = Audio_Playback_PackedMusicSampleBankIds();
    var_a0 = temp_v1 >> 0x10;
    new_var = temp_v1;

    if (temp_v1 < 0) {
        var_a0 = (temp_v1 + 0xFFFF) >> 0x10;
    }
    if (((new_var - (var_a0 << 0x10)) == file) || (var_a0 == file)) {
        return;
    }
    cdFile.lba = Audio_legacyWaveBankDiscOffsets[file];
    cdFile.size = Audio_legacyWaveBankFileSizes[file];

    if (Audio_soundState.waveBankRequest) {
        if ((intptr_t)Audio_soundState.waveBankRequest != -1) {
            Core_IgnoreDiagnostic(0x93 & 0xFFu, 0);
        }
    }
    Audio_soundState.waveBankRequest = Content_AllocateDiscRequest(&cdFile);
    if (Audio_soundState.waveBankData != 0) {
        Core_IgnoreDiagnostic(0x94, 0);
    }
    new_var = 1;
    Audio_soundState.waveBankData = Core_Memory_AllocateFromHighEnd(cdFile.size);
    Audio_soundState.waveBankId = file;
    Content_EnqueueDiscRequest(Audio_soundState.waveBankRequest, Audio_soundState.waveBankData);
}

// decomp: SLUS_010.40 vs_main_loadSoundFile (src/SLUS_010.40/main.c:10394)
void Audio_LoadMappedWaveBank(int id) { Audio_LoadWaveBankFile(Audio_waveBankFileMap[id]); }

// decomp: SLUS_010.40 vs_main_processSoundQueue (src/SLUS_010.40/main.c:10396)
int Audio_UpdateWaveBankLoad(void)
{
    int temp_v0;

    if (Audio_soundState.waveBankRequest != NULL) {
        if (Audio_soundState.waveBankRequest->state == Content_DiscRequestState_Dispatched) {
            temp_v0 = Audio_Samples_TransferStatus();
            if (temp_v0 == 0) {
                Content_ReleaseDiscRequest(Audio_soundState.waveBankRequest);
                Audio_soundState.waveBankRequest = NULL;
                Core_Memory_FreeFromHighEnd(Audio_soundState.waveBankData);
                Audio_soundState.waveBankData = NULL;
                Audio_soundState.waveBankId = 0;
                return 0;
            }
            if (temp_v0 == -1) {
                Content_ReleaseDiscRequest(Audio_soundState.waveBankRequest);
                Audio_soundState.waveBankRequest = NULL;
                Core_Memory_FreeFromHighEnd(Audio_soundState.waveBankData);
                Audio_soundState.waveBankData = NULL;
                Audio_soundState.waveBankId = 0;
                return -1;
            }
            return 1;
        }
        if (Audio_soundState.waveBankRequest->state == Content_DiscRequestState_Loaded) {
            Audio_soundState.waveBankRequest->state = Content_DiscRequestState_Dispatched;
            Audio_Samples_LoadMusicBank(Audio_soundState.waveBankData, 0);
            return 1;
        }
        if (Audio_soundState.waveBankRequest->state == Content_DiscRequestState_Error) {
            Content_ReleaseDiscRequest(Audio_soundState.waveBankRequest);
            Audio_soundState.waveBankRequest = NULL;
            Core_Memory_FreeFromHighEnd(Audio_soundState.waveBankData);
            Audio_soundState.waveBankData = NULL;
            Audio_soundState.waveBankId = 0;
            return -1;
        }
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_loadAndWaitSoundSlot (src/SLUS_010.40/main.c:10439)
void Audio_LoadWaveBankBlocking(int id)
{
    Audio_LoadMappedWaveBank(id);

    while (Audio_UpdateWaveBankLoad() != 0) {
        State_UpdatePlayTimeAndLoading(0);
    }
}

// decomp: SLUS_010.40 vs_main_setMonoSound (src/SLUS_010.40/main.c:10448)
void Audio_SetMonoOutput(int set) { Audio_Playback_SetMonoOutput(set); }

// decomp: SLUS_010.40 _loadMenuSound (src/SLUS_010.40/main.c:10452)
void Audio_InitializeMenuSoundBanks(void)
{
    void* buf;

    Audio_Playback_Initialize();
    Audio_Control_SetDiscMixVolume(0x7F);
    buf = Core_Memory_AllocateFromHighEnd(VS_WAVE0000_DAT_SIZE);
    Content_ReadDiscFileBlocking(VS_WAVE0000_DAT_LBA, VS_WAVE0000_DAT_SIZE, buf);
    Audio_Samples_LoadBank(buf, 1);
    Core_Memory_FreeFromHighEnd(buf);
    buf = Core_Memory_AllocateFromHighEnd(VS_WAVE0005_DAT_SIZE);
    Content_ReadDiscFileBlocking(VS_WAVE0005_DAT_LBA, VS_WAVE0005_DAT_SIZE, buf);
    Audio_Samples_LoadBank(buf, 1);
    Core_Memory_FreeFromHighEnd(buf);
    buf = Core_Memory_AllocateFromHighEnd(VS_WAVE0200_DAT_SIZE);
    Content_ReadDiscFileBlocking(VS_WAVE0200_DAT_LBA, VS_WAVE0200_DAT_SIZE, buf);
    Audio_Samples_LoadSoundEffectBank(buf, 0, 1);
    Core_Memory_FreeFromHighEnd(buf);
    Content_ReadDiscFileBlocking(VS_EFFECT00_DAT_LBA, VS_EFFECT00_DAT_SIZE, Audio_menuSoundEffects);
    Audio_Playback_BindSoundEffectPrograms(Audio_menuSoundEffects);
    Audio_nextSfxChannelGroup1 = 1;
    Audio_nextSfxChannelGroup7E = 2;
    Audio_nextSfxChannelGroup180 = 0x80;
    Audio_nextSfxChannelGroup200 = 0x200;
    Audio_nextSfxChannelGroupFF000 = 0x1000;
    Audio_nextSfxChannelGroupF00000 = 0x100000;
}

// decomp: SLUS_010.40 vs_main_resetSound (src/SLUS_010.40/main.c:10480)
void Audio_ResetSoundSlots(void)
{
    int i;

    Audio_soundState.currentMusicSlot = 0;
    Audio_soundState.currentMusicData = NULL;
    Audio_soundState.currentMusicId = 0xFFFF;
    Audio_soundState.playingMusicHandle = 0xFFFF;
    Audio_soundState.currentSfxBank = 0;

    for (i = 0; i < 4; ++i) {
        Audio_soundState.musicIds[i] = 0;
        Audio_soundState.musicHandles[i] = 0xFFFF;
        Audio_soundState.musicData[i] = NULL;
        Audio_soundState.musicRequests[i] = NULL;
    }

    for (i = 0; i < 3; ++i) {
        Audio_soundState.sfxBankIds[i] = 0;
        Audio_soundState.sfxBankData[i] = NULL;
        Audio_soundState.sfxBankRequests[i] = NULL;
    }

    for (i = 0; i < 3; ++i) {
        Audio_soundState.sfxStreamData[i] = NULL;
        Audio_soundState.reserved70[i] = NULL;
    }

    Audio_soundState.waveBankId = 0;
    Audio_soundState.waveBankData = NULL;
    Audio_soundState.waveBankRequest = NULL;
    Audio_nextSfxChannelGroup1 = 1;
    Audio_nextSfxChannelGroup7E = 2;
    Audio_nextSfxChannelGroup180 = 0x80;
    Audio_nextSfxChannelGroup200 = 0x200;
    Audio_nextSfxChannelGroupFF000 = 0x1000;
    Audio_nextSfxChannelGroupF00000 = 0x100000;
}

