#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptNoOperationOpcode(uint8_t* instruction,
                                       int16_t thread_index);
intptr_t Event_ScriptYieldFrameOpcode(uint8_t* instruction,
                                      int16_t thread_index);

#ifdef __cplusplus
}
#endif
