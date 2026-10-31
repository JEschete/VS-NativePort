// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"

// decomp: D_80060064 (D_80060064@SLUS_010.40)
int State_currentRoomStateSlot;

// decomp: D_80061078 (D_80061078@SLUS_010.40)
State_RoomEnemyStates State_recentRoomEnemyStates[4];

}  // extern "C"
