// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Ai/Navigation.h"

// decomp: D_800F16EC (D_800F16EC@BATTLE/BATTLE.PRG)
Battle_Ai_DirectionDelta Battle_Ai_directionDeltas[8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F175C (D_800F175C@BATTLE/BATTLE.PRG)
u_char Battle_Ai_routeStepDirections[272]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F58B8 (D_800F58B8@BATTLE/BATTLE.PRG)
u_short (*Battle_Ai_tileFlagMap)[32];

// decomp: D_800F58C8 (D_800F58C8@BATTLE/BATTLE.PRG)
unsigned short* Battle_Ai_roomWaypoints;

// decomp: D_800F58D0 (D_800F58D0@BATTLE/BATTLE.PRG)
u_short (*Battle_Ai_specialTerrainHeights)[32];

// decomp: D_800F5910 (D_800F5910@BATTLE/BATTLE.PRG)
Battle_Ai_RouteGateEnd* Battle_Ai_routeGates;

}  // extern "C"
