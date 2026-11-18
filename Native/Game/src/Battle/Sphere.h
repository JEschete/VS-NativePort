// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Battle/BattleTypes.h"
#include "Battle/TargetVolume.h"

extern short Battle_sphereArcBases[502];
extern short Battle_sphereMirroredVertices[430];
extern u_char Battle_sphereShapeCommandStarts[488];
extern u_short Battle_sphereWireframeCommands[218];
extern int Battle_positionCursorSpriteOffsets[126];
extern int Battle_positionCursorSpriteSizes[121];
extern int Battle_positionCursorSpriteTexCoords[116];
extern u_char Battle_positionCursorBlinkFrame;
extern u_char Battle_reuseSphereVolumes;
extern Battle_TargetSphereSetupView* Battle_targetSphere;
