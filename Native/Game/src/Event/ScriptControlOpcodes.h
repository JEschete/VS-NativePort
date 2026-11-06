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

#ifdef __cplusplus
}
#endif
