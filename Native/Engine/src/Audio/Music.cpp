// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/ActorSound.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/LegacyFiles.h"
#include "Audio/Music.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/SoundBanks.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/PlayTime.h"

// decomp: BATTLE.PRG func_8007BE10 (src/BATTLE/BATTLE.PRG/146C.c:8579)
int Audio_SetZoneMusicSwitchEnabled(int enabled) { return Audio_zoneMusicSwitchEnabled = enabled; }

// decomp: BATTLE.PRG func_8007C8A4 (src/BATTLE/BATTLE.PRG/146C.c:8917)
void Audio_PlayActorScriptSound(int actor_index, int sound_index)
{
    if ((Battle_actors[actor_index] != 0) && (actor_index < 16)) {
        Audio_PlayActorModelSound(Battle_actors[actor_index]->model, sound_index);
    }
}

// decomp: SLUS_010.40 func_80044D94 (src/SLUS_010.40/main.c:9490)
int Audio_MusicBeatPosition(void) { return Audio_Playback_MusicPosition(); }

// decomp: SLUS_010.40 func_80044DB4 (src/SLUS_010.40/main.c:9492)
void Audio_SaveMusicPlaybackState(void) { Audio_Playback_SaveMusicState(); }

// decomp: SLUS_010.40 func_80044DD4 (src/SLUS_010.40/main.c:9494)
void Audio_SetMusicJumpThreshold(int jump_threshold) { Audio_Control_SetMusicBranchThreshold((void*)jump_threshold); }

// decomp: SLUS_010.40 func_80044DF4 (src/SLUS_010.40/main.c:9496)
int Audio_StopMusicSlotPlayback(int id)
{
    u_int empty_music_handle;
    int slot_index;

    if (id != 0) {
        if (Audio_soundState.musicHandles[id - 1] != 0xFFFF) {
            Audio_Playback_StopMusic(Audio_soundState.musicHandles[id - 1]);
            Audio_soundState.musicHandles[id - 1] = 0xFFFF;
        }
        if (Audio_soundState.currentMusicSlot == id) {
            Audio_soundState.currentMusicSlot = 0;
            Audio_soundState.currentMusicData = NULL;
            Audio_soundState.playingMusicHandle = 0xFFFF;
        }
        return 1;
    }
    Audio_Control_StopAllMusic();
    Audio_soundState.currentMusicSlot = 0;
    Audio_soundState.currentMusicData = NULL;
    Audio_soundState.playingMusicHandle = 0xFFFF;

    empty_music_handle = 0xFFFF;
    for (slot_index = 3; slot_index >= 0; --slot_index) {
        Audio_soundState.musicHandles[slot_index] = empty_music_handle;
    }
    return 1;
}

// decomp: SLUS_010.40 func_80044EC8 (src/SLUS_010.40/main.c:9525)
int Audio_ResumeMusicSlot(int id)
{
    u_int music_handle;

    if (id != 0) {
        if (Audio_soundState.musicData[id - 1] != 0) {
            Audio_soundState.currentMusicSlot = id;
            Audio_soundState.currentMusicData = Audio_soundState.musicData[id - 1];
            music_handle = Audio_Playback_QueueMusicTransition(Audio_soundState.currentMusicData);
            if (music_handle != 0) {
                Audio_soundState.musicHandles[id - 1] = music_handle;
                Audio_soundState.playingMusicHandle = music_handle;
            }
            Audio_soundState.currentMusicId = Audio_soundState.musicIds[id - 1];
        }
        return Audio_soundState.playingMusicHandle;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80044F60 (src/SLUS_010.40/main.c:9545)
int Audio_CrossfadeMusicSlot(int id, int target_volume, u_int fade_ticks)
{
    u_int music_handle;

    if (id != 0) {
        if (Audio_soundState.musicData[id - 1] != 0) {
            Audio_soundState.currentMusicSlot = id;
            Audio_soundState.currentMusicData = Audio_soundState.musicData[id - 1];
            music_handle = Audio_Playback_CrossfadeMusic(Audio_soundState.currentMusicData, fade_ticks, target_volume);
            if (music_handle != 0) {
                Audio_soundState.musicHandles[id - 1] = music_handle;
                Audio_soundState.playingMusicHandle = music_handle;
            }
            Audio_soundState.currentMusicId = Audio_soundState.musicIds[id - 1];
        }
        return Audio_soundState.playingMusicHandle;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80045000 (src/SLUS_010.40/main.c:9565)
int Audio_PlayMusicSlot(int id, int target_volume, int fade_ticks)
{
    u_int music_handle;

    if (id != 0) {
        if (Audio_soundState.musicData[id - 1] != 0) {
            Audio_soundState.currentMusicSlot = id;
            Audio_soundState.currentMusicData = Audio_soundState.musicData[id - 1];
            Audio_Control_FadeMusicVolume(0, fade_ticks, target_volume);
            music_handle = Audio_Playback_StartMusic(Audio_soundState.currentMusicData);
            if (music_handle != 0) {
                Audio_soundState.musicHandles[id - 1] = music_handle;
                Audio_soundState.playingMusicHandle = music_handle;
                Audio_Control_FadeMusicVolume(0, fade_ticks, target_volume);
            }
            Audio_soundState.currentMusicId = Audio_soundState.musicIds[id - 1];
        }
        return Audio_soundState.playingMusicHandle;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_getcurrentMusicSlot (src/SLUS_010.40/main.c:9587)
int Audio_CurrentMusicSlot(void)
{
    return Audio_soundState.currentMusicSlot;
}

// decomp: SLUS_010.40 vs_main_getCurrentMusicId (src/SLUS_010.40/main.c:9592)
int Audio_CurrentMusicId(void)
{
    if (Audio_soundState.currentMusicSlot != 0) {
        return Audio_soundState.musicIds[Audio_soundState.currentMusicSlot - 1];
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_loadMusicSlot (src/SLUS_010.40/main.c:9600)
int Audio_LoadMusicSlot(int id, int targetSlot)
{
    Content_DiscFile cdFile;

    if (id > 0 && targetSlot > 0) {
        int slot = targetSlot - 1;

        if (targetSlot < 5) {
            if (Audio_soundState.musicIds[slot] != id) {
                do {
                    if (Audio_soundState.musicData[slot] != 0) {
                        Core_IgnoreDiagnostic(142, 0);
                        Audio_FreeMusicSlot(targetSlot);
                    }
                } while (0);

                cdFile.lba = Audio_legacyMusicDiscOffsets[id];
                cdFile.size = Audio_legacyMusicFileSizes[id];

                if ((Audio_soundState.musicRequests[slot] != 0)
                    && (Audio_soundState.musicRequests[slot]
                        != (Content_DiscQueueSlot*)-1)) {
                    Core_IgnoreDiagnostic(0x98, 0);
                }

                Audio_soundState.musicRequests[slot] =
                    Content_AllocateDiscRequest(&cdFile);

                if (Audio_soundState.musicData[slot] != 0) {
                    Core_IgnoreDiagnostic(0x8E, 0);
                }
                Audio_soundState.musicData[slot] = Core_Memory_AllocateFromHighEnd(cdFile.size);
                Content_EnqueueDiscRequest(Audio_soundState.musicRequests[slot],
                    Audio_soundState.musicData[slot]);
                Audio_soundState.musicIds[slot] = id;
            }
            return targetSlot;
        }
    }
    Core_IgnoreDiagnostic(0x8E, 0);
    return 0;
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlot (src/SLUS_010.40/main.c:9643)
int Audio_LoadMusicIntoFreeSlot(int id)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if (Audio_soundState.musicData[i] == 0) {
            return Audio_LoadMusicSlot(id, i + 1);
        }
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadSlot (src/SLUS_010.40/main.c:9655)
int Audio_PollMusicSlotLoad(u_int slot)
{
    if ((slot - 1) < 4) {
        if (Audio_soundState.musicRequests[slot - 1] != 0) {
            if (Audio_soundState.musicRequests[slot - 1]->state
                == Content_DiscRequestState_Loaded) {
                Content_ReleaseDiscRequest(Audio_soundState.musicRequests[slot - 1]);
                Audio_soundState.musicRequests[slot - 1] = NULL;
                return 0;
            }
            if (Audio_soundState.musicRequests[slot - 1]->state
                == Content_DiscRequestState_Error) {
                Content_ReleaseDiscRequest(Audio_soundState.musicRequests[slot - 1]);
                Audio_soundState.musicRequests[slot - 1] = (Content_DiscQueueSlot*)-1;
                return -1;
            }
            return 1;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadQueue (src/SLUS_010.40/main.c:9677)
int Audio_PollMusicLoads(void)
{
    int i;
    int ret;

    ret = 0;
    i = 0;
    do {
        ret |= Audio_PollMusicSlotLoad(++i);
    } while (i < 4);

    return ret;
}

// decomp: SLUS_010.40 vs_main_loadAndWaitMusicSlot (src/SLUS_010.40/main.c:9691)
int Audio_LoadMusicSlotBlocking(int id, int targetSlot)
{
    int ret;

    ret = Audio_LoadMusicSlot(id, targetSlot);
    if (ret != 0) {
        while (Audio_PollMusicSlotLoad(targetSlot) != 0) {
            State_UpdatePlayTimeAndLoading(0);
        }
    }
    return ret;
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlotAndClearQueue (src/SLUS_010.40/main.c:9704)
int Audio_LoadFreeMusicSlotBlocking(int id)
{
    int ret;

    ret = Audio_LoadMusicIntoFreeSlot(id);
    if (ret != 0) {
        while (Audio_PollMusicLoads() != 0) {
            State_UpdatePlayTimeAndLoading(0);
        }
    }
    return ret;
}

// decomp: SLUS_010.40 vs_main_freeMusic (src/SLUS_010.40/main.c:9717)
int Audio_FreeMusicSlot(int slotId)
{
    Audio_StopMusicSlotPlayback(slotId);

    if (Audio_soundState.musicData[slotId - 1] == 0) {
        return 0;
    }

    Core_Memory_FreeFromHighEnd(Audio_soundState.musicData[slotId - 1]);
    Audio_soundState.musicData[slotId - 1] = 0;
    Audio_soundState.musicIds[slotId - 1] = 0;
    return 1;
}

// decomp: SLUS_010.40 func_800454B8 (src/SLUS_010.40/main.c:9733)
int Audio_MusicSlotVolume(int slot_id)
{
    if (slot_id != 0) {
        if (Audio_soundState.musicHandles[slot_id - 1] != 0xFFFF) {
            return Audio_Playback_MusicVolume(Audio_soundState.musicHandles[slot_id - 1]);
        }
    }
    return 0;
}

// decomp: SLUS_010.40 func_8004550C (src/SLUS_010.40/main.c:9743)
int Audio_QueryMusicSlotVolume(int slot_id) { return Audio_MusicSlotVolume(slot_id); }

// decomp: SLUS_010.40 func_8004552C (src/SLUS_010.40/main.c:9745)
int Audio_FadeMusicSlotVolume(int id, int target_volume, int fade_ticks)
{
    if (Audio_soundState.currentMusicSlot == id) {
        Audio_Control_FadeMusicVolume(Audio_soundState.playingMusicHandle, fade_ticks, target_volume);
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80045574 (src/SLUS_010.40/main.c:9754)
int Audio_FadeMusicSlotTempo(int id, int target_tempo_scale, int fade_ticks)
{
    if (Audio_soundState.currentMusicSlot == id) {
        Audio_Control_FadeMusicTempoScale(fade_ticks, target_tempo_scale);
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800455AC (src/SLUS_010.40/main.c:9763)
int Audio_FadeMusicSlotPitch(int id, int target_pitch_scale, int fade_ticks)
{
    if (Audio_soundState.currentMusicSlot == id) {
        Audio_Control_FadeMusicPitchScale(fade_ticks, target_pitch_scale);
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_stopMusic (src/SLUS_010.40/main.c:9776)
void Audio_StopAndFreeCurrentMusic(void)
{
    int id = Audio_soundState.currentMusicSlot;
    if (id != 0) {
        Audio_StopMusicSlotPlayback(id);
        Audio_FreeMusicSlot(id);
    }
}

}  // extern "C"
