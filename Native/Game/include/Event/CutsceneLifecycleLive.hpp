#pragma once

namespace VS::Event {

bool PrepareLiveCutsceneActors();
bool StartLivePreparedCutscene();
bool BeginLiveQueuedCutscene();
void EndLiveFieldCutscene();

}  // namespace VS::Event
