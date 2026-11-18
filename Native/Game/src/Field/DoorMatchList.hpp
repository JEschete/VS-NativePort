#pragma once

struct Field_MpdDoorRecord;

namespace VS::Field {

// D_800F1D08_t, BATTLE/BATTLE.PRG/146C.c: native query view, not disc data.
// Records are borrowed from the current room until the next query/room unload.
struct DoorMatchList {
  int count;
  Field_MpdDoorRecord** records;
};

// Filled by VS::Field::TransitionLiveDoorsAtTile (Field/DoorMatchesLive.hpp).
extern DoorMatchList matchingDoorRecords;

}  // namespace VS::Field
