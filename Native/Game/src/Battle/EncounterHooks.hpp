#pragma once

namespace VS::Boot {

// Optional encounter configuration stays inactive in normal sessions.
// The diagnostic readiness hooks also observe normal-game lifetimes.
void EncounterRoomPlacements(void** data, unsigned int* size);
int EncounterUsesRoomEvents();
void EncounterPollStop();
// Diagnostic readiness only; never alters gameplay/event state.
void ResetGameplayInspection();

}  // namespace VS::Boot
