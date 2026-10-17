// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
int Event_ScriptPlaySoundCommand(u_char* arg0)
{
    if (arg0[1] == 2 && Event_isAmbientSoundSuspended == 0) {
        return 0;
    }
    if (arg0[1] == 2) {
        Event_suspendedAmbientSoundId = arg0[2];
    }
    Audio_PlaySfx(Event_sfxSourceIds[arg0[1]], arg0[2], arg0[3], arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA35C (src/BATTLE/BATTLE.PRG/4A0A8.c:2292)
int Event_CommandPlayActorSfx(u_char* arg0, short arg1)
{
    Audio_PlayActorScriptSound(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA39C (src/BATTLE/BATTLE.PRG/4A0A8.c:2298)
int Event_CommandSlideSfxVolume(u_char* arg0, short arg1)
{
    Audio_FadeSfxVolume(Event_sfxSourceIds[arg0[1]], arg0[2], arg0[3], arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA3E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2304)
int Event_CommandStopAllSfx(u_char* arg0, short arg1)
{
    Audio_StopAllSfxPlayback();
    return 0;
}

// decomp: BATTLE.PRG func_800BA404 (src/BATTLE/BATTLE.PRG/4A0A8.c:2310)
int Event_CommandStopSfx(u_char* arg0, short arg1)
{
    Audio_StopSfx(Event_sfxSourceIds[arg0[1]], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2316)
int Event_CommandLoadSfxSlot(u_char* arg0, short arg1)
{
    Audio_LoadSfxBankSlot(arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2322)
int Event_CommandWaitSfxSlotRelease(u_char* arg0, short arg1)
{
    return Audio_PollSfxBankSlotLoad(arg0[1]) != 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2327)
int Event_CommandFreeSfx(u_char* arg0, short arg1)
{
    Audio_FreeSfxBankSlot(arg0[1]);
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG vs_battle_script_setCurrentSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2333)
int Event_CommandSelectSfxBank(u_char* arg0, short arg1)
{
    Audio_SelectSfxBankSlot(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadSoundFile (src/BATTLE/BATTLE.PRG/4A0A8.c:2339)
int Event_CommandLoadEventMusic(u_char* arg0, short arg1)
{
    short temp_v0 = Field_ZoneSoundFileId();
    Audio_LoadMusicSlot(temp_v0, 3);
    Audio_LoadMappedWaveBank(temp_v0);
    return 0;
}

// decomp: BATTLE.PRG func_800BA52C (src/BATTLE/BATTLE.PRG/4A0A8.c:2347)
int Event_CommandCrossfadeMusic(u_char* arg0)
{
    Audio_FadeMusicSlotVolume(Event_musicSlot, 0, arg0[4]);
    Audio_CrossfadeMusicSlot(Event_musicSlot = arg0[1], arg0[2], arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_loadMusicSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2354)
int Event_CommandLoadMusicSlot(u_char* arg0, short arg1)
{
    Audio_LoadMusicSlot(arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_freeMusic (src/BATTLE/BATTLE.PRG/4A0A8.c:2360)
int Event_CommandFreeMusic(u_char* arg0, short arg1)
{
    Audio_FreeMusicSlot(arg0[1]);
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BA5E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2366)
int Event_CommandPlayMusic(u_char* arg0, short arg1)
{
    Audio_PlayMusicSlot(Event_musicSlot = arg0[1], arg0[2], arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA61C (src/BATTLE/BATTLE.PRG/4A0A8.c:2372)
int Event_CommandSlideMusicVolume(u_char* arg0, short arg1)
{
    Audio_FadeMusicSlotVolume(Event_musicSlot, arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA650 (src/BATTLE/BATTLE.PRG/4A0A8.c:2378)
int Event_CommandSlideMusicTempo(u_char* arg0, short arg1)
{
    Audio_FadeMusicSlotTempo(Event_musicSlot, (signed char)arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA684 (src/BATTLE/BATTLE.PRG/4A0A8.c:2384)
int Event_CommandSlideMusicPitch(u_char* arg0, short arg1)
{
    Audio_FadeMusicSlotPitch(Event_musicSlot, (signed char)arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA6B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2390)
int Event_CommandStopMusic(u_char* arg0, short arg1)
{
    Audio_StopMusicSlotPlayback(Event_musicSlot);
    return 0;
}

// decomp: BATTLE.PRG func_800BA6E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2396)
int Event_CommandWaitMusicSlot(u_char* arg0, short arg1)
{
    int temp_s0 = arg0[1];
    if (Audio_QueryMusicSlotVolume(temp_s0) != 0) {
        if (Audio_MusicSlotVolume(temp_s0) != 0) {
            return 1;
        }
        Audio_StopMusicSlotPlayback(temp_s0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA72C (src/BATTLE/BATTLE.PRG/4A0A8.c:2408)
int Event_CommandPushMusicState(u_char* arg0, short arg1)
{
    Audio_SaveMusicPlaybackState();
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_clearMusicLoadSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2414)
int Event_CommandWaitMusicSlotLoad(u_char* arg0, short arg1)
{
    return Audio_PollMusicSlotLoad(arg0[1]) == 1;
}

// decomp: BATTLE.PRG vs_battle_script_loadSoundFile2 (src/BATTLE/BATTLE.PRG/4A0A8.c:2419)
int Event_CommandLoadSoundFile(u_char* arg0, short arg1)
{
    Audio_LoadMappedWaveBank(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_processSoundQueue (src/BATTLE/BATTLE.PRG/4A0A8.c:2425)
int Event_CommandWaitSoundQueue(u_char* arg0, short arg1)
{
    return Audio_UpdateWaveBankLoad() == 1;
}

// decomp: BATTLE.PRG func_800BA7BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2430)
int Event_CommandWaitMusicTimer(u_char* arg0, short arg1)
{
    int temp_s1 = Audio_MusicBeatPosition() >> 0x10;
    return temp_s1 < Core_Memory_ReadUnalignedShort(arg0 + 1);
}

// decomp: BATTLE.PRG func_800BA800 (src/BATTLE/BATTLE.PRG/4A0A8.c:2436)
int Event_CommandSlideSfxPitch(u_char* arg0, short arg1)
{
    Audio_FadeSfxPitch(Event_sfxSourceIds[arg0[1]], arg0[2], (signed char)arg0[3], arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA848 (src/BATTLE/BATTLE.PRG/4A0A8.c:2442)
int Event_CommandSlideSfxPan(u_char* arg0, short arg1)
{
    Audio_FadeSfxPan(Event_sfxSourceIds[arg0[1]], arg0[2], arg0[3], arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA890 (src/BATTLE/BATTLE.PRG/4A0A8.c:2448)
int Event_CommandResumeMusicSlot(u_char* arg0, short arg1)
{
    Audio_ResumeMusicSlot(Event_musicSlot = arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAAF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2513)
int Event_CommandSetMusicJumpThreshold(u_char* arg0, short arg1)
{
    Audio_SetMusicJumpThreshold(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_playMenuSelectSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2874)
int Event_CommandPlayMenuSelectSfx(u_char* arg0, short arg1)
{
    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
    return 0;
}

