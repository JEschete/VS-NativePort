#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed sound commands
// (Event/ScriptAudioCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live sound state
// (ScriptAudioCommandsLive.cpp).
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptPlaySoundCommandOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandPlayActorSfxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSlideSfxVolumeOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandStopAllSfxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandStopSfxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandLoadSfxSlotOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitSfxSlotReleaseOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_CommandFreeSfxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSelectSfxBankOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandLoadEventMusicOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandCrossfadeMusicOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandLoadMusicSlotOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandFreeMusicOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandPlayMusicOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSlideMusicVolumeOpcode(u_char* command,
                                             int16_t thread_id);
intptr_t Event_CommandSlideMusicTempoOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSlideMusicPitchOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandStopMusicOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitMusicSlotOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandPushMusicStateOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitMusicSlotLoadOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_CommandLoadSoundFileOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitSoundQueueOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitMusicTimerOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSlideSfxPitchOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSlideSfxPanOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandResumeMusicSlotOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandSetMusicJumpThresholdOpcode(u_char* command,
                                                  int16_t thread_id);
intptr_t Event_CommandPlayMenuSelectSfxOpcode(u_char* command,
                                              int16_t thread_id);

#ifdef __cplusplus
}  // extern "C"
#endif
