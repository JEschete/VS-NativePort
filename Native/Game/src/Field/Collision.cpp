// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Field/Collision.h"

// decomp: D_800E9278 (D_800E9278@BATTLE/BATTLE.PRG)
u_char Field_kTileSurfaceIds[2744]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F49F9 (D_800F49F9@BATTLE/BATTLE.PRG)
u_char Field_collisionProbeDirection;

// decomp: D_800F4B00 (D_800F4B00@BATTLE/BATTLE.PRG)
short Field_platformContactSearchDepth;

// decomp: D_800F4B08 (D_800F4B08@BATTLE/BATTLE.PRG)
SVECTOR Field_collisionProbePosition;

}  // extern "C"
