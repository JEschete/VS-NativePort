// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_SPHERE_H_TYPES
#define VS_BATTLE_SPHERE_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:110
#define vs_getUV1Tpage(u, v, x, y, tp, abr)                                              \
    (((u) | ((v) << 8)) | (getTPage((tp), (abr), (x), (y)) << 16))

#endif // VS_BATTLE_SPHERE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_SPHERE_H_DECLS)
#define VS_BATTLE_SPHERE_H_DECLS
#include "Battle/BattleTypes.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"

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
void Battle_InitSphere(void);
void Battle_FreeSphere(void);
void Battle_SetSphereVolumes(Battle_TargetSphereSpec* arg0, Battle_TargetSphereSpec* arg1, u_char arg2, Battle_TargetPreview* arg3);
void Battle_ClearPrimarySphereVolume(void);
void Battle_SetSecondarySphereVolume(Battle_TargetSphereSpec* arg0);
void Battle_ClearSecondarySphereVolume(void);
int Battle_AppendSphereArcVertices(int phase, int segments, int index);
MATRIX* Battle_BuildSphereVolumeTransform(u_char* scale, int angle);
void Battle_DrawSphereWireLine(SVECTOR* start, SVECTOR* end, u_int color, int intensity);
void Battle_DrawSphere(void);
void Battle_BuildTargetVolumeTestTransform(Battle_TargetVolume* arg0, Battle_TargetVolumeMatrix* arg1, MATRIX* arg2);
int Battle_IsPointInTargetEllipsoid(Battle_TargetVolume* arg0, u_short* arg1);
int Battle_IsPointInTargetCylinder(Battle_TargetVolume* arg0, u_short* arg1, int arg2);
int Battle_IsPointInTargetCone(Battle_TargetVolume* arg0, u_short* arg1, int arg2);
int Battle_IsPointInTargetPrism(Battle_TargetVolume* shape, u_short* position, int angled);
int Battle_IsPointInTargetVolume(Battle_TargetVolume* arg0, u_short* arg1);
void Battle_SetSphereTargets(int arg0, Battle_TargetPreview* arg1, int arg2);
void Battle_ClearSphereTargets(void);
void Battle_DrawRightTargetLabelLink(int x0, int x1, int count);
void Battle_DrawLeftTargetLabelLink(int x0, int x1, int count);
int Battle_AreTargetLabelsSettled(void);
void Battle_DrawSphereTargetLabel(Battle_SphereTargetLabel* arg0);
void Battle_UpdateSphereTargetLabels(void);
int Battle_AppendSphereCircleVertices(int angle, int index);
int Battle_AppendSphereConeVertices(int step, int radius, int index);
void Battle_BuildSphereWireframe(int type);
void* Battle_BuildSphereCameraBasis(void);
void Battle_DrawSpherePositionCursor(SVECTOR* position, int arg1);
void Battle_CacheSphereTerrainRow(SVECTOR* position, u_int row);
void Battle_UpdateSphereTargetHighlights(Battle_TargetLabelLine* arg0, int arg1);
int Battle_SphereInteractionMode(void);

#endif // VS_BATTLE_SPHERE_H_DECLS
