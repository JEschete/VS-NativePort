#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptNoOperationOpcode(uint8_t* instruction,
                                       int16_t thread_index);
intptr_t Event_ScriptYieldFrameOpcode(uint8_t* instruction,
                                      int16_t thread_index);
intptr_t Event_CommandWaitFramesOpcode(uint8_t* instruction,
                                       int16_t thread_index);
intptr_t Event_CommandSetTickSpeedOpcode(uint8_t* instruction,
                                         int16_t thread_index);
intptr_t Event_CommandWaitScriptContextOpcode(uint8_t* instruction,
                                              int16_t thread_index);
intptr_t Event_CommandStartScriptContextOpcode(uint8_t* instruction,
                                               int16_t thread_index);
intptr_t Event_CommandStopScriptContextOpcode(uint8_t* instruction,
                                              int16_t thread_index);
intptr_t Event_ScriptCacheFlagValueOpcode(uint8_t* instruction,
                                          int16_t thread_index);
intptr_t Event_ScriptBranchOnCachedFlagOpcode(uint8_t* instruction,
                                              int16_t thread_index);
intptr_t Event_ScriptSetLoopCounterOpcode(uint8_t* instruction,
                                          int16_t thread_index);
intptr_t Event_ScriptLoopBackwardOpcode(uint8_t* instruction,
                                        int16_t thread_index);
intptr_t Event_ScriptApplyImmediateFlagOperationOpcode(uint8_t* instruction,
                                                       int16_t thread_index);
intptr_t Event_ScriptApplyFlagPairOperationOpcode(uint8_t* instruction,
                                                  int16_t thread_index);
intptr_t Event_ScriptBranchOnFlagImmediateOpcode(uint8_t* instruction,
                                                 int16_t thread_index);
intptr_t Event_ScriptBranchOnFlagPairOpcode(uint8_t* instruction,
                                            int16_t thread_index);
intptr_t Event_ScriptJumpForwardOpcode(uint8_t* instruction,
                                       int16_t thread_index);
intptr_t Event_ScriptReturnToSavedAddressOpcode(uint8_t* instruction,
                                                int16_t thread_index);
intptr_t Event_ScriptStoreInventoryItemPresenceOpcode(uint8_t* instruction,
                                                      int16_t thread_index);
intptr_t Event_ScriptStoreButtonStateOpcode(uint8_t* instruction,
                                            int16_t thread_index);
intptr_t Event_ScriptBeginSkippableCutsceneOpcode(uint8_t* instruction,
                                                  int16_t thread_index);
intptr_t Event_ScriptEndCutsceneAndJumpOpcode(uint8_t* instruction,
                                              int16_t thread_index);
intptr_t Event_ScriptEndCutsceneOpcode(uint8_t* instruction,
                                       int16_t thread_index);
intptr_t Event_ScriptWaitForButtonAndYieldOpcode(uint8_t* instruction,
                                                 int16_t thread_index);
intptr_t Event_ScriptWaitForButtonOrTimeoutOpcode(uint8_t* instruction,
                                                  int16_t thread_index);
intptr_t Event_ScriptConfigureTimerOpcode(uint8_t* instruction,
                                          int16_t thread_index);
intptr_t Event_ScriptConfigureEasingCurveOpcode(uint8_t* instruction,
                                                int16_t thread_index);
intptr_t Event_ScriptReportInvalidOpcodeOpcode(uint8_t* instruction,
                                               int16_t thread_index);
intptr_t Event_ScriptWaitForEventFileOpcode(uint8_t* instruction,
                                            int16_t thread_index);
intptr_t Event_CommandLoadNextEventOpcode(uint8_t* instruction,
                                          int16_t thread_index);
intptr_t Event_CommandFinishEventOpcode(uint8_t* instruction,
                                        int16_t thread_index);

#ifdef __cplusplus
}
#endif
