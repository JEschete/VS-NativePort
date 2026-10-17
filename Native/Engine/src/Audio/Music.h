// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_MUSIC_H_TYPES
#define VS_AUDIO_MUSIC_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_AUDIO_MUSIC_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_MUSIC_H_DECLS)
#define VS_AUDIO_MUSIC_H_DECLS

extern int Audio_zoneMusicSwitchEnabled;
int Audio_SetZoneMusicSwitchEnabled(int arg0);
void Audio_PlayActorScriptSound(int arg0, int arg1);
int Audio_MusicBeatPosition(void);
void Audio_SaveMusicPlaybackState(void);
void Audio_SetMusicJumpThreshold(int arg0);
int Audio_StopMusicSlotPlayback(int id);
int Audio_ResumeMusicSlot(int id);
int Audio_CrossfadeMusicSlot(int id, int arg1, u_int arg2);
int Audio_PlayMusicSlot(int id, int arg1, int arg2);
int Audio_CurrentMusicSlot(void);
int Audio_CurrentMusicId(void);
int Audio_LoadMusicSlot(int id, int targetSlot);
int Audio_LoadMusicIntoFreeSlot(int id);
int Audio_PollMusicSlotLoad(u_int slot);
int Audio_PollMusicLoads(void);
int Audio_LoadMusicSlotBlocking(int id, int targetSlot);
int Audio_LoadFreeMusicSlotBlocking(int id);
int Audio_FreeMusicSlot(int slotId);
int Audio_MusicSlotVolume(int arg0);
int Audio_QueryMusicSlotVolume(int arg0);
int Audio_FadeMusicSlotVolume(int id, int arg1, int arg2);
int Audio_FadeMusicSlotTempo(int id, int arg1, int arg2);
int Audio_FadeMusicSlotPitch(int id, int arg1, int arg2);
void Audio_StopAndFreeCurrentMusic(void);

#endif // VS_AUDIO_MUSIC_H_DECLS
