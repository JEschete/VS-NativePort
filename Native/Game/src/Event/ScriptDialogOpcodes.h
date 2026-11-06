#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed dialog commands
// (Event/ScriptDialogCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live dialog state
// (ScriptDialogCommandsLive.cpp).
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptInitDialogBoxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptShowDialogTextOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptDismissDialogBoxOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptInitActorDialogBoxOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptWaitForDialogDismissalOpcode(u_char* command,
                                                  int16_t thread_id);
intptr_t Event_ScriptSetDialogIntegerOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptSetDialogTransitionDurationOpcode(u_char* command,
                                                       int16_t thread_id);
intptr_t Event_ScriptSelectDialogChoiceOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptSetDialogSpeakerOpcode(u_char* command, int16_t thread_id);

#ifdef __cplusplus
}  // extern "C"
#endif
