#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Diagnostic calls only: no VM, flag, skip or completion state is changed.
uint64_t Diagnostics_RecordSceneCommand(int32_t event_id, int32_t source,
                                        int32_t thread, const void* instruction,
                                        const void* script_base, uint32_t size,
                                        int32_t skip_state);
void Diagnostics_RecordSceneResult(uint64_t command, int64_t result);
void Diagnostics_RecordScriptFlagWrite(int32_t flag, uint8_t before,
                                       uint8_t after);

#ifdef __cplusplus
}
#endif
