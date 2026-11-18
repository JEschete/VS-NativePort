#pragma once

// The per-tick stream update and reset the generated sound driver calls; the
// stream commands are in StreamControlLive.hpp.

namespace VS::Audio {

void UpdateCutsceneStream();
void ResetCutsceneStream();

}  // namespace VS::Audio
