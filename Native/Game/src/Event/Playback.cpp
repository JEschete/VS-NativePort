// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Event/Playback.h"

// decomp: D_800F4BBC (D_800F4BBC@BATTLE/BATTLE.PRG)
short* Event_eventFileDiscRequest;

// decomp: D_800F4BF0 (D_800F4BF0@BATTLE/BATTLE.PRG)
Content_DiscFile Event_eventFileDiscLocation;

}  // extern "C"
