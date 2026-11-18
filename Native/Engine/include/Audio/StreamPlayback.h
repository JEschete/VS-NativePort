#pragma once

// The per-tick stream update and reset the generated sound driver calls; the
// stream commands are in StreamControlLive.hpp.

void Audio_Native_UpdateCutsceneStream(void);
void Audio_Native_ResetCutsceneStream(void);
