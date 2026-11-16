// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"

// decomp: D_800EA234 (D_800EA234@BATTLE/BATTLE.PRG)
short Battle_sphereArcBases[502]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA2C4 (D_800EA2C4@BATTLE/BATTLE.PRG)
short Battle_sphereMirroredVertices[430]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA438 (D_800EA438@BATTLE/BATTLE.PRG)
u_char Battle_sphereShapeCommandStarts[488]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA46C (D_800EA46C@BATTLE/BATTLE.PRG)
u_short Battle_sphereWireframeCommands[218]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA670 (D_800EA670@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteOffsets[126]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA684 (D_800EA684@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteSizes[121]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA698 (D_800EA698@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteTexCoords[116]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA6AC (D_800EA6AC@BATTLE/BATTLE.PRG)
u_char Battle_positionCursorBlinkFrame; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9AC (D_800EB9AC@BATTLE/BATTLE.PRG)
u_char Battle_reuseSphereVolumes; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9B8 (D_800EB9B8@BATTLE/BATTLE.PRG)
Battle_TargetSphereSetupView* Battle_targetSphere; /* data: contents loaded in Step 5 */

}  // extern "C"
