// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTAUDIO_H_TYPES
#define VS_EVENT_SCRIPTAUDIO_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/SLUS_010.40/sfx.h:4
#define VS_SFX_MENUSELECT 5

#endif // VS_EVENT_SCRIPTAUDIO_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTAUDIO_H_DECLS)
#define VS_EVENT_SCRIPTAUDIO_H_DECLS

extern char Event_isAmbientSoundSuspended;
extern short Event_suspendedAmbientSoundId;
extern char Event_musicSlot;
intptr_t Event_ScriptPlaySoundCommand(u_char* arg0, short arg1);
intptr_t Event_CommandPlayActorSfx(u_char* arg0, short arg1);
intptr_t Event_CommandSlideSfxVolume(u_char* arg0, short arg1);
intptr_t Event_CommandStopAllSfx(u_char* arg0, short arg1);
intptr_t Event_CommandStopSfx(u_char* arg0, short arg1);
intptr_t Event_CommandLoadSfxSlot(u_char* arg0, short arg1);
intptr_t Event_CommandWaitSfxSlotRelease(u_char* arg0, short arg1);
intptr_t Event_CommandFreeSfx(u_char* arg0, short arg1);
intptr_t Event_CommandSelectSfxBank(u_char* arg0, short arg1);
intptr_t Event_CommandLoadEventMusic(u_char* arg0, short arg1);
intptr_t Event_CommandCrossfadeMusic(u_char* arg0, short arg1);
intptr_t Event_CommandLoadMusicSlot(u_char* arg0, short arg1);
intptr_t Event_CommandFreeMusic(u_char* arg0, short arg1);
intptr_t Event_CommandPlayMusic(u_char* arg0, short arg1);
intptr_t Event_CommandSlideMusicVolume(u_char* arg0, short arg1);
intptr_t Event_CommandSlideMusicTempo(u_char* arg0, short arg1);
intptr_t Event_CommandSlideMusicPitch(u_char* arg0, short arg1);
intptr_t Event_CommandStopMusic(u_char* arg0, short arg1);
intptr_t Event_CommandWaitMusicSlot(u_char* arg0, short arg1);
intptr_t Event_CommandPushMusicState(u_char* arg0, short arg1);
intptr_t Event_CommandWaitMusicSlotLoad(u_char* arg0, short arg1);
intptr_t Event_CommandLoadSoundFile(u_char* arg0, short arg1);
intptr_t Event_CommandWaitSoundQueue(u_char* arg0, short arg1);
intptr_t Event_CommandWaitMusicTimer(u_char* arg0, short arg1);
intptr_t Event_CommandSlideSfxPitch(u_char* arg0, short arg1);
intptr_t Event_CommandSlideSfxPan(u_char* arg0, short arg1);
intptr_t Event_CommandResumeMusicSlot(u_char* arg0, short arg1);
intptr_t Event_CommandSetMusicJumpThreshold(u_char* arg0, short arg1);
intptr_t Event_CommandPlayMenuSelectSfx(u_char* arg0, short arg1);

#endif // VS_EVENT_SCRIPTAUDIO_H_DECLS
