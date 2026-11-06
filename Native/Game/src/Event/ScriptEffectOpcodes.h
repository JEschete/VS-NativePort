#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed effect commands
// (Event/ScriptEffectCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live effect state
// (ScriptEffectCommandsLive.cpp).
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_CommandPlayActorEffectOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandAdvanceEffectStageOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_CommandWaitEffectStateOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandYieldEffectReleaseOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_CommandWaitEffectContinuationOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_CommandWaitEffectReleaseOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_CommandMarkEffectStageReadyOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_CommandPlayVibrationPatternOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_CommandSetActorTrailOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandWaitScreenEffectLoadOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_CommandSetScreenDistortionOpcode(u_char* command,
                                                int16_t thread_id);
intptr_t Event_CommandLoadScreenEffectsOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_CommandReservedScreenEffectOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_CommandFreeScreenEffectsOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_CommandSetScanlineOverlayOpcode(u_char* command,
                                               int16_t thread_id);

#ifdef __cplusplus
}  // extern "C"
#endif
