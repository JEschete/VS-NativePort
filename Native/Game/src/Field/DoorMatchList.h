#pragma once

typedef struct Field_MpdDoorRecord Field_MpdDoorRecord;

// D_800F1D08_t, BATTLE/BATTLE.PRG/146C.c: native query view, not disc data.
// Records are borrowed from the current room until the next query/room unload.
typedef struct Field_DoorMatchList {
  int count;
  Field_MpdDoorRecord** records;
} Field_DoorMatchList;

#ifdef __cplusplus
extern "C" {
#endif

// Filled by VS::Field::TransitionLiveDoorsAtTile (Field/DoorMatchesLive.hpp).
extern Field_DoorMatchList Field_matchingDoorRecords;

#ifdef __cplusplus
}
#endif
