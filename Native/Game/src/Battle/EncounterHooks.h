#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Optional encounter configuration stays inactive in normal sessions.
// The diagnostic readiness hooks also observe normal-game lifetimes.
void Boot_EncounterRoomPlacements(void** data, unsigned int* size);
int Boot_EncounterUsesRoomEvents(void);
int Boot_EncounterLoadRoomEvents(void);
void Boot_EncounterPollStop(void);
// Diagnostic readiness only; never alters gameplay/event state.
void Boot_ResetGameplayInspection(void);

#ifdef __cplusplus
}
#endif
