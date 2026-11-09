// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"

// decomp: D_800E849C (D_800E849C@BATTLE/BATTLE.PRG)
short Field_lockedDoorIndexRanges[54]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E84AC (D_800E84AC@BATTLE/BATTLE.PRG)
char Field_lockedDoorRoomPairs[23][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F18E8 (D_800F18E8@BATTLE/BATTLE.PRG)
Field_DoorFlagIdWord Field_snapshotDoorFlagSource;

// decomp: D_800F1AB0 (D_800F1AB0@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_pendingDestinationDoor;

// decomp: vs_battle_doorEntered (vs_battle_doorEntered@BATTLE/BATTLE.PRG)
short Field_enteredDoorId;

// decomp: D_800F1CC8 (D_800F1CC8@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_entranceDoorRecord;

}  // extern "C"
