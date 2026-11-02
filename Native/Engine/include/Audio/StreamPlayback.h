#pragma once

// The per-tick stream update and reset the generated sound driver calls; the
// stream commands are in StreamControlLive.hpp.
#ifdef __cplusplus
extern "C" {
#endif

void Audio_Native_UpdateCutsceneStream(void);
void Audio_Native_ResetCutsceneStream(void);

#ifdef __cplusplus
}
#endif
