#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Inclusive byte-indexed range; records begin with a signed 32-bit key.
void Core_SortPointersByDescendingKey(void** values, int first, int last);

#ifdef __cplusplus
}
#endif
