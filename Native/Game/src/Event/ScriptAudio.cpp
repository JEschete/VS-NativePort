// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Music.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Core/Memory/Copy.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptVm.h"
#include "Event/Sound.h"
#include "Field/RoomLoading.h"

// decomp: D_800F4C08 (D_800F4C08@BATTLE/BATTLE.PRG)
char Event_isAmbientSoundSuspended;

// decomp: D_800F4C4C (D_800F4C4C@BATTLE/BATTLE.PRG)
short Event_suspendedAmbientSoundId;

// decomp: D_800F4C69 (D_800F4C69@BATTLE/BATTLE.PRG)
char Event_musicSlot;

// decomp: BATTLE.PRG func_800BA2E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2280)
intptr_t Event_ScriptPlaySoundCommand(u_char* instruction, short thread_index)
{
    if (instruction[1] == 2 && Event_isAmbientSoundSuspended == 0) {
        return 0;
    }
    if (instruction[1] == 2) {
        Event_suspendedAmbientSoundId = instruction[2];
    }
    Audio_PlaySfx(Event_sfxSourceIds[instruction[1]], instruction[2], instruction[3], instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA35C (src/BATTLE/BATTLE.PRG/4A0A8.c:2292)
intptr_t Event_CommandPlayActorSfx(u_char* instruction, short thread_index)
{
    Audio_PlayActorScriptSound(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA39C (src/BATTLE/BATTLE.PRG/4A0A8.c:2298)
intptr_t Event_CommandSlideSfxVolume(u_char* instruction, short thread_index)
{
    Audio_FadeSfxVolume(Event_sfxSourceIds[instruction[1]], instruction[2], instruction[3], instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA3E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2304)
intptr_t Event_CommandStopAllSfx(u_char* instruction, short thread_index)
{
    Audio_StopAllSfxPlayback();
    return 0;
}

// decomp: BATTLE.PRG func_800BA404 (src/BATTLE/BATTLE.PRG/4A0A8.c:2310)
intptr_t Event_CommandStopSfx(u_char* instruction, short thread_index)
{
    Audio_StopSfx(Event_sfxSourceIds[instruction[1]], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2316)
intptr_t Event_CommandLoadSfxSlot(u_char* instruction, short thread_index)
{
    Audio_LoadSfxBankSlot(instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2322)
intptr_t Event_CommandWaitSfxSlotRelease(u_char* instruction, short thread_index)
{
    return Audio_PollSfxBankSlotLoad(instruction[1]) != 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2327)
intptr_t Event_CommandFreeSfx(u_char* instruction, short thread_index)
{
    Audio_FreeSfxBankSlot(instruction[1]);
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG vs_battle_script_setCurrentSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2333)
intptr_t Event_CommandSelectSfxBank(u_char* instruction, short thread_index)
{
    Audio_SelectSfxBankSlot(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadSoundFile (src/BATTLE/BATTLE.PRG/4A0A8.c:2339)
intptr_t Event_CommandLoadEventMusic(u_char* instruction, short thread_index)
{
    short zone_sound_file_id = Field_ZoneSoundFileId();
    Audio_LoadMusicSlot(zone_sound_file_id, 3);
    Audio_LoadMappedWaveBank(zone_sound_file_id);
    return 0;
}

// decomp: BATTLE.PRG func_800BA52C (src/BATTLE/BATTLE.PRG/4A0A8.c:2347)
intptr_t Event_CommandCrossfadeMusic(u_char* instruction, short thread_index)
{
    Audio_FadeMusicSlotVolume(Event_musicSlot, 0, instruction[4]);
    Audio_CrossfadeMusicSlot(Event_musicSlot = instruction[1], instruction[2], instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadMusicSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2354)
intptr_t Event_CommandLoadMusicSlot(u_char* instruction, short thread_index)
{
    Audio_LoadMusicSlot(instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeMusic (src/BATTLE/BATTLE.PRG/4A0A8.c:2360)
intptr_t Event_CommandFreeMusic(u_char* instruction, short thread_index)
{
    Audio_FreeMusicSlot(instruction[1]);
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BA5E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2366)
intptr_t Event_CommandPlayMusic(u_char* instruction, short thread_index)
{
    Audio_PlayMusicSlot(Event_musicSlot = instruction[1], instruction[2], instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA61C (src/BATTLE/BATTLE.PRG/4A0A8.c:2372)
intptr_t Event_CommandSlideMusicVolume(u_char* instruction, short thread_index)
{
    Audio_FadeMusicSlotVolume(Event_musicSlot, instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA650 (src/BATTLE/BATTLE.PRG/4A0A8.c:2378)
intptr_t Event_CommandSlideMusicTempo(u_char* instruction, short thread_index)
{
    Audio_FadeMusicSlotTempo(Event_musicSlot, (signed char)instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA684 (src/BATTLE/BATTLE.PRG/4A0A8.c:2384)
intptr_t Event_CommandSlideMusicPitch(u_char* instruction, short thread_index)
{
    Audio_FadeMusicSlotPitch(Event_musicSlot, (signed char)instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA6B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2390)
intptr_t Event_CommandStopMusic(u_char* instruction, short thread_index)
{
    Audio_StopMusicSlotPlayback(Event_musicSlot);
    return 0;
}

// decomp: BATTLE.PRG func_800BA6E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2396)
intptr_t Event_CommandWaitMusicSlot(u_char* instruction, short thread_index)
{
    int music_slot = instruction[1];
    if (Audio_QueryMusicSlotVolume(music_slot) != 0) {
        if (Audio_MusicSlotVolume(music_slot) != 0) {
            return 1;
        }
        Audio_StopMusicSlotPlayback(music_slot);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA72C (src/BATTLE/BATTLE.PRG/4A0A8.c:2408)
intptr_t Event_CommandPushMusicState(u_char* instruction, short thread_index)
{
    Audio_SaveMusicPlaybackState();
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_clearMusicLoadSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2414)
intptr_t Event_CommandWaitMusicSlotLoad(u_char* instruction, short thread_index)
{
    return Audio_PollMusicSlotLoad(instruction[1]) == 1;
}

// decomp: BATTLE.PRG vs_battle_script_loadSoundFile2 (src/BATTLE/BATTLE.PRG/4A0A8.c:2419)
intptr_t Event_CommandLoadSoundFile(u_char* instruction, short thread_index)
{
    Audio_LoadMappedWaveBank(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_processSoundQueue (src/BATTLE/BATTLE.PRG/4A0A8.c:2425)
intptr_t Event_CommandWaitSoundQueue(u_char* instruction, short thread_index)
{
    return Audio_UpdateWaveBankLoad() == 1;
}

// decomp: BATTLE.PRG func_800BA7BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2430)
intptr_t Event_CommandWaitMusicTimer(u_char* instruction, short thread_index)
{
    int music_timer_count = Audio_MusicBeatPosition() >> 0x10;
    return music_timer_count < Core_Memory_ReadUnalignedShort(instruction + 1);
}

// decomp: BATTLE.PRG func_800BA800 (src/BATTLE/BATTLE.PRG/4A0A8.c:2436)
intptr_t Event_CommandSlideSfxPitch(u_char* instruction, short thread_index)
{
    Audio_FadeSfxPitch(Event_sfxSourceIds[instruction[1]], instruction[2], (signed char)instruction[3], instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA848 (src/BATTLE/BATTLE.PRG/4A0A8.c:2442)
intptr_t Event_CommandSlideSfxPan(u_char* instruction, short thread_index)
{
    Audio_FadeSfxPan(Event_sfxSourceIds[instruction[1]], instruction[2], instruction[3], instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA890 (src/BATTLE/BATTLE.PRG/4A0A8.c:2448)
intptr_t Event_CommandResumeMusicSlot(u_char* instruction, short thread_index)
{
    Audio_ResumeMusicSlot(Event_musicSlot = instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAAF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2513)
intptr_t Event_CommandSetMusicJumpThreshold(u_char* instruction, short thread_index)
{
    Audio_SetMusicJumpThreshold(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_playMenuSelectSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2874)
intptr_t Event_CommandPlayMenuSelectSfx(u_char* instruction, short thread_index)
{
    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
    return 0;
}

}  // extern "C"
