// The room door globals (Doors.hpp).
#include "Field/Doors.hpp"

#include "Field/Mpd.hpp"

namespace VS::Field {
// decomp: D_800E849C (D_800E849C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
LockedDoorRanges lockedDoorIndexRanges;

// decomp: D_800E84AC (D_800E84AC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
LockedDoorPairs lockedDoorRoomPairs;

// decomp: D_800F18E8 (D_800F18E8@BATTLE/BATTLE.PRG)
DoorFlagIdWord snapshotDoorFlagSource;

// decomp: D_800F1AB0 (D_800F1AB0@BATTLE/BATTLE.PRG)
VS::Field::MpdDoorRecord pendingDestinationDoor;

// decomp: vs_battle_doorEntered (vs_battle_doorEntered@BATTLE/BATTLE.PRG)
std::int16_t enteredDoorId;

// decomp: D_800F1CC8 (D_800F1CC8@BATTLE/BATTLE.PRG)
VS::Field::MpdDoorRecord entranceDoorRecord;
}  // namespace VS::Field
