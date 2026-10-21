#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Optional native encounter configuration. Inactive for normal game sessions.
void Boot_EncounterRoomPlacements(void** data, unsigned int* size);
int Boot_EncounterUsesRoomEvents(void);
int Boot_EncounterLoadRoomEvents(void);
void Boot_EncounterPollStop(void);

#ifdef __cplusplus
}
#endif
