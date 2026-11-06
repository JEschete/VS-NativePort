// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Event/Triggers.h"

// decomp: D_800F4BA8 (D_800F4BA8@BATTLE/BATTLE.PRG)
char Event_isDirectEventRequest;

// decomp: _mpdClearedSection (_mpdClearedSection@BATTLE/BATTLE.PRG)
void* Event_roomEventTriggers;

// decomp: _mpdRoomDoorSection (_mpdRoomDoorSection@BATTLE/BATTLE.PRG)
void* Event_roomDoorEventData;

// decomp: _mpdScriptSection (_mpdScriptSection@BATTLE/BATTLE.PRG)
short Event_hasRoomEntryScript;

// decomp: BATTLE.PRG func_800BEBEC (src/BATTLE/BATTLE.PRG/4A0A8.c:4273)
void Event_ReleaseZoneEventResources(void) { }

}  // extern "C"
