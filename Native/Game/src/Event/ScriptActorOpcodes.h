#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed actor commands
// (Event/ScriptActorCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live actor, script, item and
// battle state (ScriptActorCommandsLive.cpp).
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptResetActorGroupStateOpcode(u_char* command,
                                                int16_t thread_id);
intptr_t Event_ScriptStoreActorTileOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptSpawnActorOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptNavigateActorToTileOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_ScriptPlayActorAnimationOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptSetActorGroupFlagsOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptSetActorSoundMixOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptWalkActorToTileOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptWalkActorToWorldPosOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_ScriptSlideActorToTileOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptSlideActorToWorldPosOpcode(u_char* command,
                                                int16_t thread_id);
intptr_t Event_ScriptSetActorAttachmentOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptNavigateActorToWorldPosOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_ScriptTurnActorToFacingOpcode(u_char* command,
                                             int16_t thread_id);
intptr_t Event_ScriptFaceOtherActorOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptFaceTileOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptScaleActorOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptRemoveActorGroupOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptWaitForActorFlagsOpcode(u_char* command,
                                             int16_t thread_id);
intptr_t Event_ScriptSetActorColorOverrideOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_ScriptQueueActorAnimationOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_ScriptRotateActorAxesOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptSetActorHeadAnglesOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptPlayActorAnimationAtFrameOpcode(u_char* command,
                                                     int16_t thread_id);
intptr_t Event_ScriptQueueActorAnimationAtFrameOpcode(u_char* command,
                                                      int16_t thread_id);
intptr_t Event_ScriptSetActorIdleAnimationOpcode(u_char* command,
                                                 int16_t thread_id);
intptr_t Event_ScriptLookAtActorOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptLookAtWorldPosOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptLoadObjectModelOpcode(u_char* command, int16_t thread_id);
intptr_t Event_ScriptWaitForObjectLoadOpcode(u_char* command,
                                             int16_t thread_id);
intptr_t Event_ScriptReleaseObjectModelOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptSelectObjectTextureBankOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_ScriptSetActorOverlayPassOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_ScriptSetActorWeaponDrawnOpcode(u_char* command,
                                               int16_t thread_id);
intptr_t Event_ScriptPlayActorAnimationSoundOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_ScriptSetNpcUpdatesEnabledOpcode(u_char* command,
                                                int16_t thread_id);
intptr_t Event_ScriptRestoreActorVitalsOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_ScriptSetActorEquipmentModelsOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_ScriptSetActorYawOscillationOpcode(u_char* command,
                                                  int16_t thread_id);
intptr_t Event_ScriptClearAshleyStatusEffectsOpcode(u_char* command,
                                                    int16_t thread_id);
intptr_t Event_ScriptSetActorRenderModeOpcode(u_char* command,
                                              int16_t thread_id);
intptr_t Event_CommandSetEnteredDoorOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandStoreActorRangeBearingOpcode(u_char* command,
                                                   int16_t thread_id);
intptr_t Event_CommandSetActorActivityOpcode(u_char* command,
                                             int16_t thread_id);
intptr_t Event_CommandAwardMiscLootOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandConsumeMiscItemOpcode(u_char* command, int16_t thread_id);
intptr_t Event_CommandStoreActorDistanceOpcode(u_char* command,
                                               int16_t thread_id);

#ifdef __cplusplus
}
#endif
