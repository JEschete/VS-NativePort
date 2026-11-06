#pragma once
// Hand-maintained (tools/port/owned.csv).
// The pending room visit as the copied room loading sets it. RoomStateLive.cpp
// defines it and reads it into the typed RoomVisitState (Field/RoomState.hpp).

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F19A0 (BATTLE/BATTLE.PRG): nonzero once a room loads until
// play resumes there and it is marked visited.
extern int Field_isRoomVisitPending;

#ifdef __cplusplus
}
#endif
